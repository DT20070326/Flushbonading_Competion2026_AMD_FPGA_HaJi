// 患者档案 + 训练场次聚合 + 原始采样缓存
import cloud from '@/utils/cloud.js'

const STORAGE_KEY = 'rehab_patients'
const SEED_KEY = 'rehab_seed_version'
const SEED_VERSION = 6   // 改示例患者/示例历史数据时 +1，下次加载自动刷新示例档案
const SESSION_GAP_MS = 90 * 1000     // 超过 90s 没有新采样，视为新的一次训练
const SAMPLE_INTERVAL_S = 2          // KV260 上报周期约 2s
const MAX_SESSIONS = 60              // 每个患者最多保留的场次
const MAX_SAMPLES = 300              // 每个设备最多保留的原始采样
const RAW_DEVICE_ID = 'device-main'   // 仿生手设备级原始数据缓存（不再依赖患者档案）

// ---------- 时间/格式化 ----------
function nowString(d) {
	d = d || new Date()
	const pad = n => String(n).padStart(2, '0')
	return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

function fmtDuration(ms) {
	if (!ms || ms < 0) return '00:00'
	const s = Math.round(ms / 1000)
	const m = Math.floor(s / 60)
	return String(m).padStart(2, '0') + ':' + String(s % 60).padStart(2, '0')
}

function fmtMinutes(ms) {
	if (!ms || ms < 0) return 0
	return Math.round(ms / 60000 * 10) / 10
}

// ---------- 场次统计 ----------
function sessionStats(s) {
	const start = s.startTs || 0
	const end = s.endTs || start
	const span = Math.max(0, end - start)
	const bySamples = (s.samples || 0) * SAMPLE_INTERVAL_S * 1000
	const durationMs = Math.max(span, bySamples)
	const errors = s.errors || []
	const results = s.results || []
	const avgError = errors.length ? Math.round(errors.reduce((a, b) => a + b, 0) / errors.length) : null
	const lastResult = results.length ? results[results.length - 1] : null
	const bestResult = results.length ? Math.max.apply(null, results) : null
	return {
		id: s.id,
		startTs: start,
		endTs: end,
		durationMs,
		minutes: fmtMinutes(durationMs),
		samples: s.samples || 0,
		avgError,
		lastResult,
		bestResult,
		analysis: s.aiReport || '',
		resultWord: lastResult === null ? '--' : cloud.RESULT_WORDS[Math.max(0, Math.min(3, Math.round(lastResult)))]
	}
}

function buildSession(ts) {
	return {
		id: String(ts),
		startTs: ts,
		endTs: ts,
		samples: 0,
		errors: [],
		results: [],
		lastAngles: [null, null, null, null, null],
		lastGoals: null,
		lastMode: null,
		angleSeries: []      // 本次训练逐次采集的五指角度，供训练记录展开查看
	}
}

// ---------- 患者读写 ----------
function loadPatients() {
	let list = null
	let ver = 0
	try {
		const raw = uni.getStorageSync(STORAGE_KEY)
		if (raw) {
			const parsed = JSON.parse(raw)
			if (Array.isArray(parsed)) list = parsed
		}
		ver = parseInt(uni.getStorageSync(SEED_KEY)) || 0
	} catch (e) {}

	// 首次运行或示例版本变了：重建示例患者档案，用户自己新增的（非 demo- 前缀）保留
	if (!list || ver !== SEED_VERSION) {
		const custom = (list || []).filter(p => !String(p.id).startsWith('demo-'))
		const merged = buildDemoPatients().concat(custom)
		savePatients(merged)
		try { uni.setStorageSync(SEED_KEY, String(SEED_VERSION)) } catch (e) {}
		return merged
	}
	return list
}

function savePatients(list) {
	try { uni.setStorageSync(STORAGE_KEY, JSON.stringify(list)) } catch (e) {}
}

function getPatient(id) {
	return loadPatients().find(p => p.id === id) || null
}

// 按登录账号查患者（账号大小写不敏感）
function findPatientByAccount(account) {
	const key = String(account === undefined || account === null ? '' : account).trim().toLowerCase()
	if (!key) return null
	return loadPatients().find(p => String(p.account || '').trim().toLowerCase() === key) || null
}

// 取某个患者的一次训练记录
function getSession(patientId, sessionId) {
	const p = getPatient(patientId)
	if (!p || !Array.isArray(p.sessions)) return null
	return p.sessions.find(s => String(s.id) === String(sessionId)) || null
}

// 更新某一次训练记录（如写入 DeepSeek 单独分析）
function updateSession(patientId, sessionId, patch) {
	const list = loadPatients()
	const p = list.find(x => x.id === patientId)
	if (!p || !Array.isArray(p.sessions)) return null
	const s = p.sessions.find(x => String(x.id) === String(sessionId))
	if (!s) return null
	Object.assign(s, patch)
	savePatients(list)
	return s
}

function addPatient(p) {
	const list = loadPatients()
	list.push(p)
	savePatients(list)
	return list
}

function removePatient(id) {
	const list = loadPatients().filter(p => p.id !== id)
	savePatients(list)
	return list
}

function updatePatient(id, patch) {
	const list = loadPatients()
	const i = list.findIndex(p => p.id === id)
	if (i >= 0) {
		list[i] = Object.assign({}, list[i], patch)
		savePatients(list)
		return list[i]
	}
	return null
}

function newPatientId() {
	return String(Date.now()) + Math.floor(Math.random() * 1000)
}

// ---------- 采样写入（实时轮询时调用） ----------
function recordSnapshot(patientId, snap, ts) {
	ts = ts || Date.now()
	const list = loadPatients()
	const p = list.find(x => x.id === patientId)
	if (!p) return null
	if (!Array.isArray(p.sessions)) p.sessions = []
	let cur = p.sessions[p.sessions.length - 1]
	if (!cur || (ts - (cur.endTs || cur.startTs)) > SESSION_GAP_MS) {
		cur = buildSession(ts)
		p.sessions.push(cur)
	}
	cur.endTs = ts
	cur.samples = (cur.samples || 0) + 1
	cur.lastAngles = snap.angles
	cur.lastGoals = snap.goals
	if (snap.angles && snap.angles.some(v => v !== null)) {
		if (!Array.isArray(cur.angleSeries)) cur.angleSeries = []
		cur.angleSeries.push({ t: ts, angles: snap.angles, goals: snap.goals || null })
		if (cur.angleSeries.length > MAX_SAMPLES) cur.angleSeries = cur.angleSeries.slice(-MAX_SAMPLES)
	}
	if (snap.mode !== null && snap.mode !== undefined) cur.lastMode = snap.mode
	if (snap.error !== null && snap.error !== undefined) cur.errors.push(snap.error)
	if (snap.result !== null && snap.result !== undefined) cur.results.push(snap.result)
	if (p.sessions.length > MAX_SESSIONS) p.sessions = p.sessions.slice(-MAX_SESSIONS)
	p.lastSyncTs = ts
	p.lastSnap = {
		angles: snap.angles, goals: snap.goals, error: snap.error, result: snap.result,
		mode: snap.mode, time: snap.time
	}
	p.online = true
	savePatients(list)
	return p
}

// 原始采样缓存（原始数据页用）
function pushRawSample(patientId, timeStr, snap, rawData) {
	try {
		uni.setStorageSync('rehab_raw_' + patientId, JSON.stringify({ time: timeStr, data: rawData || [] }))
	} catch (e) {}

	let arr = []
	try {
		const r = uni.getStorageSync('rehab_samples_' + patientId)
		if (r) arr = JSON.parse(r)
	} catch (e) {}
	const row = { _sample_at: timeStr }
	cloud.CURRENT_KEYS.forEach((k, i) => { row[k] = snap.angles[i] })
	if (snap.goals) cloud.GOAL_KEYS.forEach((k, i) => { row[k] = snap.goals[i] })
	row[cloud.PROP_ERROR] = snap.error
	row[cloud.PROP_RESULT] = snap.result
	row[cloud.PROP_MODE] = snap.mode
	row[cloud.PROP_TIME] = snap.time
	if (snap.objectName) row[cloud.PROP_OBJECT] = snap.objectName
	if (snap.confidence !== null && snap.confidence !== undefined) row[cloud.PROP_OBJECT_CONF] = snap.confidence
	if (snap.graspCount !== null && snap.graspCount !== undefined) row[cloud.PROP_GRASP_COUNT] = snap.graspCount
	arr.push(row)
	if (arr.length > MAX_SAMPLES) arr = arr.slice(-MAX_SAMPLES)
	try { uni.setStorageSync('rehab_samples_' + patientId, JSON.stringify(arr)) } catch (e) {}
	return row
}

function loadRawDump(patientId) {
	try {
		const r = uni.getStorageSync('rehab_raw_' + patientId)
		if (r) return JSON.parse(r)
	} catch (e) {}
	return null
}

function loadSamples(patientId) {
	try {
		const r = uni.getStorageSync('rehab_samples_' + patientId)
		if (r) return JSON.parse(r)
	} catch (e) {}
	return []
}

function clearSamples(patientId) {
	try {
		uni.removeStorageSync('rehab_samples_' + patientId)
		uni.removeStorageSync('rehab_raw_' + patientId)
	} catch (e) {}
}

// ---------- 示例患者 ----------
// 患者档案 + 历史训练记录都是演示用的编造数据；
// 但「当前数据」（模式/角度/时长/效果/误差/在线状态）一律只来自云平台实读：
// 这里不写 lastSnap / online，云平台读不到就是 -- / 离线，见 recordSnapshot()。
const DEMO_DEFS = [
	{ name: '王建国', account: 'Patient', gender: '男', age: 58, hand: '右手', note: '脑卒中后遗症 · 康复中期', dur: 20, err: 34, res: 0, sessions: 8 },
	{ name: '李秀兰', account: 'Patient2', gender: '女', age: 64, hand: '左手', note: '脑卒中恢复期 · 肌力Ⅱ级', dur: 15, err: 28, res: 1, sessions: 7 },
	{ name: '张 伟', account: 'Patient3', gender: '男', age: 47, hand: '右手', note: '工伤术后 · 屈伸训练', dur: 24, err: 40, res: 0, sessions: 6 },
	{ name: '陈美玲', account: 'Patient4', gender: '女', age: 52, hand: '双手', note: '类风湿康复 · 关节活动度', dur: 12, err: 22, res: 1, sessions: 9 }
]

const ANGLE_STEP_MS = 30000   // 示例历史里每 30s 记一条五指角度

// 造一次训练记录（含逐条舵机值，供训练记录展开查看）
function genSessions(def) {
	const out = []
	const now = Date.now()
	const count = def.sessions
	for (let k = 0; k < count; k++) {
		const progress = count > 1 ? k / (count - 1) : 1            // 0 -> 1，越靠后康复越好
		const daysAgo = (count - 1 - k) * 2 + 1                      // 最近一次在 1 天前
		const startTs = now - daysAgo * 86400000 - (k % 3) * 6 * 3600000
		const durMin = Math.max(5, Math.round(def.dur * (0.65 + 0.35 * progress) + ((k * 7) % 5) - 2))
		const endTs = startTs + durMin * 60000
		const err = Math.max(6, Math.round(def.err * (1 - 0.7 * progress)))
		const res = Math.max(0, Math.min(3, Math.round(def.res + 3 * progress)))

		// 本次训练的五指目标值：每个舵机一个目标
		const goals = [95, 92, 90, 88, 86]
		const n = Math.max(2, Math.round(durMin * 60000 / ANGLE_STEP_MS))
		const angleSeries = []
		for (let i = 0; i < n; i++) {
			const phase = i / Math.max(1, n - 1)
			const amp = 28 * (1 - 0.55 * progress)      // 康复越好，原始值越贴近目标
			const angles = [0, 1, 2, 3, 4].map(f => {
				const off = amp * Math.cos((phase * 3 + f * 0.35) * Math.PI) + (((i * 13 + f * 7) % 9) - 4)
				return Math.max(0, Math.min(180, Math.round(goals[f] + off)))
			})
			angleSeries.push({ t: startTs + i * ANGLE_STEP_MS, angles, goals })
		}

		out.push({
			id: String(startTs),
			startTs,
			endTs,
			samples: Math.round(durMin * 60 / SAMPLE_INTERVAL_S),
			errors: [err + 4, err, Math.max(4, err - 3), err + 2],
			results: [Math.max(0, res - 1), res, res],
			angleSeries,
			lastAngles: angleSeries[angleSeries.length - 1].angles,
			lastGoals: goals,
			lastMode: 1
		})
	}
	return out
}

function buildDemoPatients() {
	return DEMO_DEFS.map((def, i) => ({
		id: 'demo-' + (i + 1),
		name: def.name,
		account: def.account || '',
		gender: def.gender,
		age: def.age,
		hand: def.hand,
		note: def.note,
		productId: cloud.DEFAULT_PRODUCT_ID,
		deviceName: cloud.DEFAULT_DEVICE_NAME,
		online: false,
		createdAt: Date.now() - (30 - i) * 86400000,
		lastSyncTs: null,
		sessions: genSessions(def)
	}))
}

export {
	STORAGE_KEY, SESSION_GAP_MS, SAMPLE_INTERVAL_S, RAW_DEVICE_ID,
	nowString, fmtDuration, fmtMinutes, sessionStats, buildSession,
	loadPatients, savePatients, getPatient, findPatientByAccount, getSession, updateSession,
	addPatient, removePatient, updatePatient, newPatientId,
	recordSnapshot, pushRawSample, loadRawDump, loadSamples, clearSamples,
	buildDemoPatients
}

export default {
	STORAGE_KEY, SESSION_GAP_MS, SAMPLE_INTERVAL_S, RAW_DEVICE_ID,
	nowString, fmtDuration, fmtMinutes, sessionStats, buildSession,
	loadPatients, savePatients, getPatient, findPatientByAccount, getSession, updateSession,
	addPatient, removePatient, updatePatient, newPatientId,
	recordSnapshot, pushRawSample, loadRawDump, loadSamples, clearSamples,
	buildDemoPatients
}

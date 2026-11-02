// 仿生手「视觉-动作」物品档案
// - 已记录：优先读 OneNET 云端档案 / 当前识别属性，读不到时保留本地缓存
// - 未记录：保存抓拍图片，调用视觉 API 识别后再入库
// - 每次复现抓取累加抓取次数，并记录最近动作
import cloud from '@/utils/cloud.js'

const STORAGE_KEY = 'grasp_objects_v1'
const META_KEY = 'grasp_meta_v1'
const SEED_VERSION = 3
const MAX_RECENT = 30

const SOURCE_LABELS = {
	cloud: 'OneNET 云端',
	api: 'AI 识别入库',
	demo: '演示档案',
	capture: '设备抓拍',
	manual: '手动录入'
}

const DEFAULT_IMAGE = '/static/objects/unknown.png'

// 识别结果没有图片地址时，按物品名称回退到本地图片
const LOCAL_IMAGES = {
	'鼠标': '/static/鼠标.jpg',
	'苹果': '/static/苹果.jpg',
	'apple': '/static/苹果.jpg'
}

function localImage(name) {
	const key = String(name || '').trim().toLowerCase()
	return LOCAL_IMAGES[key] || ''
}

function now() { return Date.now() }

function pad(n) { return String(n).padStart(2, '0') }

function fmtTime(ts) {
	if (!ts) return '--'
	const d = new Date(ts)
	return `${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`
}

function fmtFullTime(ts) {
	if (!ts) return '--'
	const d = new Date(ts)
	return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

function relativeTime(ts) {
	if (!ts) return '--'
	const diff = now() - Number(ts)
	if (diff < 60 * 1000) return '刚刚'
	if (diff < 60 * 60 * 1000) return Math.floor(diff / 60000) + ' 分钟前'
	if (diff < 24 * 60 * 60 * 1000) return Math.floor(diff / 3600000) + ' 小时前'
	if (diff < 7 * 24 * 60 * 60 * 1000) return Math.floor(diff / 86400000) + ' 天前'
	return fmtTime(ts)
}

function clampServo(v) {
	const n = Number(v)
	if (isNaN(n)) return null
	return Math.max(0, Math.min(180, Math.round(n)))
}

function parseTs(v) {
	if (v === null || v === undefined || v === '') return 0
	if (typeof v === 'number') return v
	const n = Number(v)
	if (!isNaN(n)) return n
	const d = Date.parse(v)
	return isNaN(d) ? 0 : d
}

function normalizeServo(v) {
	if (!v) return null
	if (Array.isArray(v)) {
		const arr = v.slice(0, 5).map(clampServo)
		return arr.some(x => x !== null) ? arr : null
	}
	if (typeof v === 'string') {
		const s = v.trim()
		if (s.charAt(0) === '[') {
			try {
				const parsed = JSON.parse(s)
				if (Array.isArray(parsed)) return normalizeServo(parsed)
			} catch (e) {}
		}
		const parts = s.split(/[,，/\s]+/).filter(Boolean)
		if (parts.length >= 5) return normalizeServo(parts)
		return null
	}
	if (typeof v === 'object') {
		const keys = ['Damuzhi', 'shizhi', 'Zhongzhi', 'wumingzhi', 'xiaomuzhi']
		const lower = {}
		Object.keys(v).forEach(k => { lower[k.toLowerCase()] = v[k] })
		const arr = keys.map(k => {
			const hit = Object.keys(lower).find(x => x.indexOf(k.toLowerCase()) >= 0)
			return hit ? clampServo(lower[hit]) : null
		})
		return arr.some(x => x !== null) ? arr : null
	}
	return null
}

function makeSeed() {
	const t = now()
	const day = 86400000
	return [
		{
			id: 'demo-cup', name: '水杯', category: '生活用品', image: '/static/objects/cup.png',
			status: 'recorded', servo: [64, 74, 78, 76, 70], count: 18, confidence: 0.96,
			graspKey: 'cylinder', graspType: '圆柱抓握', description: '圆柱形杯身，从杯身中段包握，保持杯口朝上。',
			source: 'cloud', demo: true, hint: 'cup', createdAt: t - 26 * day, updatedAt: t - 2 * 3600000,
			lastGrabAt: t - 2 * 3600000, cloudSyncedAt: t - 30 * 60000
		},
		{
			id: 'demo-apple', name: '苹果', category: '食物', image: '/static/苹果.jpg',
			status: 'recorded', servo: [86, 90, 92, 88, 82], count: 12, confidence: 0.94,
			graspKey: 'sphere', graspType: '球形抓握', description: '球形果实，整掌包覆，减小指尖压力。',
			source: 'cloud', demo: true, hint: 'apple', createdAt: t - 24 * day, updatedAt: t - 1 * 3600000,
			lastGrabAt: t - 1 * 3600000, cloudSyncedAt: t - 30 * 60000
		},
		{
			id: 'demo-pill', name: '药瓶', category: '医疗用品', image: '/static/objects/pill.png',
			status: 'recorded', servo: [38, 52, 58, 50, 42], count: 7, confidence: 0.91,
			graspKey: 'pinch', graspType: '指尖捏取', description: '小直径瓶体，指尖捏取并控制夹持力。',
			source: 'cloud', demo: true, hint: 'pill', createdAt: t - 20 * day, updatedAt: t - 5 * 3600000,
			lastGrabAt: t - 5 * 3600000, cloudSyncedAt: t - 30 * 60000
		},
		{
			id: 'demo-remote', name: '遥控器', category: '电子设备', image: '/static/objects/remote.png',
			status: 'recorded', servo: [72, 82, 86, 82, 70], count: 9, confidence: 0.89,
			graspKey: 'hook', graspType: '钩状抓握', description: '长条扁平物体，沿短边包握，避开按键。',
			source: 'cloud', demo: true, hint: 'remote', createdAt: t - 18 * day, updatedAt: t - 7 * 3600000,
			lastGrabAt: t - 7 * 3600000, cloudSyncedAt: t - 30 * 60000
		},
		{
			id: 'demo-tissue', name: '纸巾盒', category: '生活用品', image: '/static/objects/tissue.png',
			status: 'recorded', servo: [22, 32, 36, 34, 28], count: 6, confidence: 0.9,
			graspKey: 'palm', graspType: '平掌托举', description: '方正软质盒体，平掌托举配合侧向夹持。',
			source: 'cloud', demo: true, hint: 'tissue', createdAt: t - 15 * day, updatedAt: t - 30 * 3600000,
			lastGrabAt: t - 30 * 3600000, cloudSyncedAt: t - 30 * 60000
		},
		{
			id: 'demo-bowl', name: '小碗', category: '餐具', image: '/static/objects/bowl.png',
			status: 'recorded', servo: [22, 32, 36, 34, 28], count: 11, confidence: 0.93,
			graspKey: 'palm', graspType: '平掌托举', description: '托住碗底，拇指扣住碗沿，保持水平。',
			source: 'cloud', demo: true, hint: 'bowl', createdAt: t - 12 * day, updatedAt: t - 3 * 3600000,
			lastGrabAt: t - 3 * 3600000, cloudSyncedAt: t - 30 * 60000
		},
		{
			id: 'demo-glasses', name: '', category: '', image: '/static/objects/glasses.png',
			status: 'unrecorded', servo: null, count: 0, confidence: 0,
			graspKey: '', graspType: '', description: '',
			source: 'capture', demo: true, hint: 'glasses', createdAt: t - 50 * 60000, updatedAt: t - 50 * 60000,
			lastGrabAt: 0, cloudSyncedAt: 0
		},
		{
			id: 'demo-milk', name: '', category: '', image: '/static/objects/milk.png',
			status: 'unrecorded', servo: null, count: 0, confidence: 0,
			graspKey: '', graspType: '', description: '',
			source: 'capture', demo: true, hint: 'milk', createdAt: t - 35 * 60000, updatedAt: t - 35 * 60000,
			lastGrabAt: 0, cloudSyncedAt: 0
		},
		{
			id: 'demo-banana', name: '', category: '', image: '/static/objects/banana.png',
			status: 'unrecorded', servo: null, count: 0, confidence: 0,
			graspKey: '', graspType: '', description: '',
			source: 'capture', demo: true, hint: 'banana', createdAt: t - 18 * 60000, updatedAt: t - 18 * 60000,
			lastGrabAt: 0, cloudSyncedAt: 0
		},
		{
			id: 'demo-key', name: '', category: '', image: '/static/objects/key.png',
			status: 'unrecorded', servo: null, count: 0, confidence: 0,
			graspKey: '', graspType: '', description: '',
			source: 'capture', demo: true, hint: 'key', createdAt: t - 8 * 60000, updatedAt: t - 8 * 60000,
			lastGrabAt: 0, cloudSyncedAt: 0
		}
	]
}

function defaultMeta() {
	return { version: SEED_VERSION, currentId: '', lastCloudSync: 0, cloudOnline: false, recent: [] }
}

function loadMeta() {
	try {
		const raw = uni.getStorageSync(META_KEY)
		if (raw) {
			const m = typeof raw === 'string' ? JSON.parse(raw) : raw
			return Object.assign(defaultMeta(), m)
		}
	} catch (e) {}
	return defaultMeta()
}

function saveMeta(meta) {
	try { uni.setStorageSync(META_KEY, JSON.stringify(meta)) } catch (e) {}
}

function saveObjects(list) {
	try {
		uni.setStorageSync(STORAGE_KEY, JSON.stringify(list))
		return true
	} catch (e) {
		return false
	}
}

function loadObjects() {
	let list = null
	let ver = 0
	try {
		const raw = uni.getStorageSync(STORAGE_KEY)
		if (raw) {
			const parsed = typeof raw === 'string' ? JSON.parse(raw) : raw
			if (Array.isArray(parsed)) list = parsed
		}
		ver = parseInt(uni.getStorageSync('grasp_seed_version')) || 0
	} catch (e) {}

	if (!list || ver !== SEED_VERSION) {
		// 保留用户自己新增/识别过的对象，只重建演示样例
		const custom = (list || []).filter(o => o && !o.demo)
		const merged = makeSeed().concat(custom)
		saveObjects(merged)
		try { uni.setStorageSync('grasp_seed_version', String(SEED_VERSION)) } catch (e) {}
		return merged
	}
	return list
}

function getObject(id) {
	return loadObjects().find(o => String(o.id) === String(id)) || null
}

function findByName(name) {
	const key = String(name || '').trim().toLowerCase()
	if (!key) return null
	return loadObjects().find(o => String(o.name || '').trim().toLowerCase() === key) || null
}

function updateObject(id, patch) {
	const list = loadObjects()
	const idx = list.findIndex(o => String(o.id) === String(id))
	if (idx < 0) return null
	list[idx] = Object.assign({}, list[idx], patch || {}, { updatedAt: now() })
	saveObjects(list)
	return list[idx]
}

function addPending(item) {
	const list = loadObjects()
	const obj = {
		id: 'obj-' + now() + '-' + Math.floor(Math.random() * 1000),
		name: '', category: '', image: (item && item.image) || DEFAULT_IMAGE,
		status: 'unrecorded', servo: normalizeServo(item && item.servo), count: 0, confidence: 0,
		graspKey: '', graspType: '', description: '',
		source: (item && item.source) || 'capture', demo: false, hint: (item && item.hint) || '',
		createdAt: now(), updatedAt: now(), lastGrabAt: 0, cloudSyncedAt: 0
	}
	list.unshift(obj)
	const ok = saveObjects(list)
	if (!ok) throw new Error('本地存储空间不足，请删除部分图片后重试')
	return obj
}

function removeObject(id) {
	const list = loadObjects().filter(o => String(o.id) !== String(id))
	saveObjects(list)
	const meta = loadMeta()
	if (String(meta.currentId) === String(id)) meta.currentId = ''
	meta.recent = (meta.recent || []).filter(r => String(r.objectId) !== String(id))
	saveMeta(meta)
	return list
}

function getCurrentObject() {
	const meta = loadMeta()
	if (meta.currentId) {
		const hit = getObject(meta.currentId)
		if (hit) return hit
	}
	const list = loadObjects()
	return list.find(o => o.status === 'recorded') || null
}

function setCurrent(id) {
	const meta = loadMeta()
	meta.currentId = id || ''
	saveMeta(meta)
}

function stats() {
	const list = loadObjects()
	return {
		total: list.length,
		recorded: list.filter(o => o.status === 'recorded').length,
		pending: list.filter(o => o.status === 'unrecorded').length,
		grabs: list.reduce((a, o) => a + (Number(o.count) || 0), 0)
	}
}

function recentActivity(limit) {
	const meta = loadMeta()
	return (meta.recent || []).slice(0, limit || 6)
}

function applyRecognition(id, result, servoOverride) {
	const list = loadObjects()
	const idx = list.findIndex(o => String(o.id) === String(id))
	if (idx < 0) return null
	const o = list[idx]
	const servo = normalizeServo(servoOverride) || normalizeServo(result && result.servo)
	Object.assign(o, {
		name: (result && result.name) || '待命名物品',
		category: (result && result.category) || '未分类',
		image: localImage(result && result.name) || (result && result.image) || o.image || DEFAULT_IMAGE,
		status: 'recorded',
		servo: servo,
		confidence: Number(result && result.confidence) || 0,
		graspKey: (result && result.graspKey) || '',
		graspType: (result && result.graspType) || '',
		description: (result && result.description) || '',
		source: result && result.demo ? 'demo' : 'api',
		demo: !!(result && result.demo),
		providerLabel: (result && result.providerLabel) || '',
		identifiedAt: now(),
		updatedAt: now()
	})
	const ok = saveObjects(list)
	if (!ok) throw new Error('本地存储空间不足，识别结果未能保存')
	const meta = loadMeta()
	meta.currentId = o.id
	saveMeta(meta)
	return o
}

// 主界面「存入」：把当前识别物品 + 五指参数保存/更新到档案
function saveSnapshot(payload) {
	const p = payload || {}
	const name = String(p.name || '').trim()
	if (!name) return null
	const servo = normalizeServo(p.servo)
	if (!servo || servo.length < 5 || servo.some(v => v === null || v === undefined)) return null

	const ts = now()
	const list = loadObjects()
	let hit = list.find(o => String(o.name || '').trim().toLowerCase() === name.toLowerCase())
	const source = p.source || 'manual'
	const demo = p.demo === undefined ? false : !!p.demo

	if (hit) {
		Object.assign(hit, {
			name,
			category: p.category || hit.category || '未分类',
			image: localImage(name) || p.image || hit.image || DEFAULT_IMAGE,
			status: 'recorded',
			servo,
			confidence: (p.confidence === null || p.confidence === undefined) ? (hit.confidence || 0) : Number(p.confidence),
			graspKey: p.graspKey || hit.graspKey || '',
			graspType: p.graspType || hit.graspType || '',
			description: p.description || hit.description || '由主界面存入的五指抓取参数。',
			source,
			demo,
			cloudSyncedAt: source === 'cloud' ? ts : (hit.cloudSyncedAt || 0),
			updatedAt: ts,
			savedAt: ts
		})
	} else {
		hit = {
			id: 'saved-' + ts + '-' + Math.floor(Math.random() * 1000),
			name,
			category: p.category || '未分类',
			image: localImage(name) || p.image || DEFAULT_IMAGE,
			status: 'recorded',
			servo,
			count: 0,
			confidence: Number(p.confidence) || 0,
			graspKey: p.graspKey || '',
			graspType: p.graspType || '',
			description: p.description || '由主界面存入的五指抓取参数。',
			source,
			demo,
			createdAt: ts,
			updatedAt: ts,
			lastGrabAt: 0,
			cloudSyncedAt: source === 'cloud' ? ts : 0,
			savedAt: ts
		}
		list.unshift(hit)
	}

	const ok = saveObjects(list)
	if (!ok) throw new Error('本地存储空间不足，参数未能存入')
	const meta = loadMeta()
	meta.currentId = hit.id
	saveMeta(meta)
	return hit
}

// 主界面「存入档案」：只存五指舵机值 + 本地图片，等待后续 AI 识别命名。
// 同一张图不会重复堆积：再次存入时只更新舵机值。
function savePendingSnapshot(payload) {
	const p = payload || {}
	const servo = normalizeServo(p.servo)
	if (!servo || servo.length < 5 || servo.some(v => v === null || v === undefined)) return null
	const image = p.image || DEFAULT_IMAGE
	const ts = now()
	const list = loadObjects()
	let hit = list.find(o => o.status === 'unrecorded' && o.source === 'manual' && o.image === image)

	if (hit) {
		Object.assign(hit, {
			servo,
			image,
			hint: p.hint || hit.hint || '',
			updatedAt: ts,
			savedAt: ts
		})
	} else {
		hit = {
			id: 'pending-' + ts + '-' + Math.floor(Math.random() * 1000),
			name: '', category: '', image,
			status: 'unrecorded', servo, count: 0, confidence: 0,
			graspKey: '', graspType: '', description: '',
			source: 'manual', demo: false, hint: p.hint || '',
			createdAt: ts, updatedAt: ts, lastGrabAt: 0, cloudSyncedAt: 0, savedAt: ts
		}
		list.unshift(hit)
	}

	const ok = saveObjects(list)
	if (!ok) throw new Error('本地存储空间不足，参数未能存入')
	const meta = loadMeta()
	meta.currentId = hit.id
	saveMeta(meta)
	return hit
}

function incrementGrab(id) {
	const list = loadObjects()
	const o = list.find(x => String(x.id) === String(id))
	if (!o) return null
	o.count = (Number(o.count) || 0) + 1
	o.lastGrabAt = now()
	o.updatedAt = now()
	saveObjects(list)

	const meta = loadMeta()
	meta.currentId = o.id
	meta.recent = [{
		id: 'grab-' + now(),
		objectId: o.id, name: o.name, image: o.image,
		servo: o.servo ? o.servo.slice() : null,
		graspType: o.graspType || '',
		ts: o.lastGrabAt
	}].concat(meta.recent || []).slice(0, MAX_RECENT)
	saveMeta(meta)
	return o
}

function sourceLabel(source) {
	return SOURCE_LABELS[source] || '本地档案'
}

// 云端档案条目 -> 本地对象
function fromCloudEntry(entry, idx, ts) {
	if (!entry || typeof entry !== 'object') return null
	const name = String(entry.name || entry.object_name || entry.objectName || entry.wupin || entry.title || '').trim()
	if (!name) return null
	const image = String(entry.image || entry.imageUrl || entry.image_url || entry.img || entry.photo || '')
	const servo = normalizeServo(entry.servo || entry.servos || entry.angles || entry.finger || entry.fingers)
	let confidence = Number(entry.confidence != null ? entry.confidence : entry.conf)
	if (isNaN(confidence)) confidence = 0
	if (confidence > 1) confidence = confidence / 100
	const countVal = Number(entry.count != null ? entry.count : (entry.grab_count != null ? entry.grab_count : entry.graspCount))
	return {
		id: entry.id ? String(entry.id) : 'cloud-' + idx + '-' + name,
		name,
		category: String(entry.category || entry.class || entry.type || '未分类'),
		image: localImage(name) || image || '',
		status: 'recorded',
		servo,
		count: isNaN(countVal) ? 0 : Math.max(0, Math.round(countVal)),
		confidence,
		graspKey: entry.graspKey || '',
		graspType: entry.graspType || entry.grasp || '',
		description: entry.description || entry.desc || '',
		source: 'cloud',
		demo: false,
		createdAt: parseTs(entry.createdAt) || ts,
		updatedAt: ts,
		lastGrabAt: parseTs(entry.lastGrabAt) || 0,
		cloudSyncedAt: ts
	}
}

function mergeCloudList(cloudList, ts) {
	if (!Array.isArray(cloudList) || !cloudList.length) return { added: 0, updated: 0 }
	const list = loadObjects()
	let added = 0
	let updated = 0
	cloudList.forEach((entry, idx) => {
		const c = fromCloudEntry(entry, idx, ts)
		if (!c) return
		let hit = list.find(o => String(o.id) === String(c.id)) ||
			list.find(o => String(o.name || '').trim().toLowerCase() === c.name.toLowerCase())
		if (hit) {
			// 云端字段可能是部分字段，缺省时保留本地已保存的抓取参数
			const patch = Object.assign({}, c, {
				id: hit.id,
				image: c.image || hit.image,
				servo: c.servo || hit.servo,
				confidence: c.confidence || hit.confidence,
				category: (c.category && c.category !== '未分类') ? c.category : hit.category,
				description: c.description || hit.description,
				graspKey: c.graspKey || hit.graspKey,
				graspType: c.graspType || hit.graspType,
				count: Math.max(Number(hit.count) || 0, Number(c.count) || 0),
				createdAt: hit.createdAt || c.createdAt,
				lastGrabAt: Math.max(Number(hit.lastGrabAt) || 0, Number(c.lastGrabAt) || 0),
				status: 'recorded',
				updatedAt: ts
			})
			Object.assign(hit, patch)
			updated++
		} else {
			if (!c.image) c.image = DEFAULT_IMAGE
			list.push(c)
			added++
		}
	})
	saveObjects(list)
	return { added, updated }
}

// 把一次云平台快照合并进档案（不发起网络请求）
function ingestSnapshot(snap, ts) {
	ts = ts || now()
	const prev = getCurrentObject()
	let merged = { added: 0, updated: 0 }
	let current = null

	if (snap && snap.archive && snap.archive.length) {
		merged = mergeCloudList(snap.archive, ts)
	}

	if (snap && snap.objectName) {
		let hit = findByName(snap.objectName)
		const localImg = localImage(snap.objectName)
		const servo = normalizeServo(snap.angles)
		if (hit) {
			const patch = {
				status: 'recorded',
				source: 'cloud',
				demo: false,
				cloudSyncedAt: ts,
				updatedAt: ts
			}
			if (servo) patch.servo = servo
			if (localImg) patch.image = localImg
			else if (snap.objectImage) patch.image = snap.objectImage
			if (snap.confidence !== null) patch.confidence = snap.confidence > 1 ? snap.confidence / 100 : snap.confidence
			if (snap.graspCount !== null) {
				// 抓取次数只增不减，避免云端旧值覆盖本地刚完成的复现
				patch.count = Math.max(Number(hit.count) || 0, Math.max(0, Math.round(snap.graspCount)))
			}
			hit = updateObject(hit.id, patch)
		} else {
			hit = {
				id: 'cloud-live-' + ts,
				name: snap.objectName,
				category: '云端识别',
				image: localImg || snap.objectImage || DEFAULT_IMAGE,
				status: 'recorded',
				servo: servo,
				count: snap.graspCount === null ? 0 : Math.max(0, Math.round(snap.graspCount)),
				confidence: snap.confidence === null ? 0 : (snap.confidence > 1 ? snap.confidence / 100 : snap.confidence),
				graspKey: '', graspType: '', description: '',
				source: 'cloud', demo: false,
				createdAt: ts, updatedAt: ts, lastGrabAt: 0, cloudSyncedAt: ts
			}
			const list = loadObjects()
			list.unshift(hit)
			saveObjects(list)
			merged.added++
		}
		current = hit
	} else {
		// 云端没有物品属性时，不回写已存的抓取序列，避免实时角度覆盖教学中保存的参数；
		// 主界面的五指值仍然直接从 snap.angles 显示。
		const cur = getCurrentObject()
		if (cur) current = cur
	}

	const meta = loadMeta()
	meta.lastCloudSync = ts
	meta.cloudOnline = true
	// 用户刚存入/识别过的本地档案优先，云端轮询不要抢走「当前物品」
	if (current && !(prev && prev.source !== 'cloud')) meta.currentId = current.id
	saveMeta(meta)

	return {
		added: merged.added,
		updated: merged.updated,
		cloudTotal: snap && snap.archive ? snap.archive.length : 0,
		current: getCurrentObject() || current
	}
}

// 读取 OneNET：云端档案数组优先；没有数组则把当前识别物品/五指值合并进档案
async function syncCloud(productId, deviceName) {
	const snap = await cloud.queryDeviceProperty(
		productId || cloud.DEFAULT_PRODUCT_ID,
		deviceName || cloud.DEFAULT_DEVICE_NAME
	)
	const result = ingestSnapshot(snap, now())
	return Object.assign({ snap }, result)
}

// 标记云端读取失败（只影响状态，不删本地档案）
function markCloudOffline() {
	const meta = loadMeta()
	meta.cloudOnline = false
	saveMeta(meta)
}

function cloudMeta() {
	const meta = loadMeta()
	return {
		lastCloudSync: meta.lastCloudSync || 0,
		cloudOnline: !!meta.cloudOnline,
		lastCloudText: meta.lastCloudSync ? fmtFullTime(meta.lastCloudSync) : '尚未同步'
	}
}

function exportCsv() {
	const list = loadObjects().filter(o => o.status === 'recorded')
	const headers = ['物品名称', '类别', '抓取次数', '拇指', '食指', '中指', '无名指', '小指', '抓握方式', '数据来源', '最近抓取']
	let csv = '﻿' + headers.join(',') + '\n'
	list.forEach(o => {
		const s = o.servo || []
		csv += [
			'"' + String(o.name).replace(/"/g, '""') + '"',
			'"' + String(o.category || '').replace(/"/g, '""') + '"',
			o.count || 0,
			s[0] == null ? '' : s[0], s[1] == null ? '' : s[1], s[2] == null ? '' : s[2],
			s[3] == null ? '' : s[3], s[4] == null ? '' : s[4],
			'"' + String(o.graspType || '').replace(/"/g, '""') + '"',
			'"' + sourceLabel(o.source) + '"',
			'"' + fmtFullTime(o.lastGrabAt) + '"'
		].join(',') + '\n'
	})
	return csv
}

export {
	STORAGE_KEY, META_KEY, SOURCE_LABELS, DEFAULT_IMAGE,
	loadObjects, saveObjects, getObject, findByName, updateObject, addPending, removeObject,
	getCurrentObject, setCurrent, stats, recentActivity, applyRecognition, saveSnapshot, savePendingSnapshot, incrementGrab,
	ingestSnapshot, syncCloud, markCloudOffline, cloudMeta, sourceLabel, exportCsv,
	normalizeServo, fmtTime, fmtFullTime, relativeTime, localImage, LOCAL_IMAGES
}

export default {
	STORAGE_KEY, META_KEY, SOURCE_LABELS, DEFAULT_IMAGE, LOCAL_IMAGES, localImage,
	loadObjects, saveObjects, getObject, findByName, updateObject, addPending, removeObject,
	getCurrentObject, setCurrent, stats, recentActivity, applyRecognition, saveSnapshot, savePendingSnapshot, incrementGrab,
	ingestSnapshot, syncCloud, markCloudOffline, cloudMeta, sourceLabel, exportCsv,
	normalizeServo, fmtTime, fmtFullTime, relativeTime
}


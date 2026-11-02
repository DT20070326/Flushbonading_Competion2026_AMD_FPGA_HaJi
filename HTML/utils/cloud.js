// OneNET 云平台读取（仿生手 / 手部康复训练设备）
// 与设备端（ESP32）上报的属性标识符保持一致
const { createProductToken } = require('@/key.js')

const API_URL = 'https://iot-api.heclouds.com/thingmodel/query-device-property'

// OneNET 产品级鉴权（2022-05-01 / HMAC-SHA1）。
// 产品维度授权：同一产品下任意设备都可查询，换设备只改 DEFAULT_DEVICE_NAME。
// 产品密钥放在本地 secret.js（不提交到仓库），模板见 secret.example.js。
const SECRET = require('./secret.js')
const AUTH = {
	access_key: SECRET.access_key,
	version: '2022-05-01'
}

// 五指当前角度标识符（Damuzhi/shizhi/Zhongzhi/wumingzhi/xiaomuzhi）
const CURRENT_KEYS = ['Damuzhi', 'shizhi', 'Zhongzhi', 'wumingzhi', 'xiaomuzhi']
// 五指目标角度标识符
const GOAL_KEYS = ['Damuzhi_Goal', 'Shizhi_Goal', 'Zhongzhi_Goal', 'Wumingzhi_Goal', 'Xiaomuzhi_Goal']
// 误差（度）
const PROP_ERROR = 'Wuchayuanshizhi'
// 训练效果 0~3
const PROP_RESULT = 'kangfu'
// 模式 0=正常 / 1=训练 / 2=录入
const PROP_MODE = 'mode'
// 上一次训练时长（秒）
const PROP_TIME = 'time'

// ---------- 仿生手抓取档案相关属性 ----------
// 设备端若已上报这些标识符，会直接读取；没有时页面退回本地档案缓存。
const PROP_OBJECT = 'wupin'             // 当前识别到的物品名称
const PROP_OBJECT_IMG = 'wupin_img'     // 物品图片 URL（可选）
const PROP_OBJECT_CONF = 'wupin_conf'   // 识别置信度 0~1（可选）
const PROP_GRASP_COUNT = 'grab_count'   // 该物品累计抓取次数（可选）
const PROP_ARCHIVE = 'ObjectArchive'    // 云端物品档案 JSON 字符串（可选）

const MODE_WORDS = ['正常模式', '训练模式', '录入模式']
const FINGER_CN = ['大拇指', '食指', '中指', '无名指', '小指']
const FINGER_ICON = ['👍', '☝️', '✋', '💍', '🤙']
const RESULT_WORDS = ['较差', '良', '好', '优秀']
const RESULT_COLORS = ['#e53935', '#fb8c00', '#43a047', '#1b5e20']

// 当前设备（日志 topic: $sys/9OX8NAt415/Fpga/thing/property/post）
const DEFAULT_PRODUCT_ID = '9OX8NAt415'
const DEFAULT_DEVICE_NAME = 'Fpga'

// 新旧设备的中指标识符不同：仿生手用 Zhong，康复训练设备用 Zhongzhi
const KEY_ALIASES = { zhongzhi: ['zhong'], zhong: ['zhongzhi'] }

function makeToken(productId) {
	try {
		return createProductToken({
			access_key: AUTH.access_key,
			version: AUTH.version,
			product_id: productId || DEFAULT_PRODUCT_ID
		})
	} catch (e) { return '' }
}

function normKey(s) {
	// 兼容 wupin_img / wupinImg / wupin-img 等命名差异
	return String(s === undefined || s === null ? '' : s).trim().toLowerCase().replace(/[_\-\s]+/g, '')
}

// 兼容两种返回：带 identifier/name 的对象数组，或按顺序排列的值数组
function pickValue(dataList, identifier, posIdx) {
	if (!Array.isArray(dataList)) return undefined
	const named = dataList.some(it => it && (it.identifier != null || it.name != null))
	if (named) {
		const target = normKey(identifier)
		const hit = dataList.find(it => normKey(it.identifier != null ? it.identifier : it.name) === target)
		if (hit) return hit.value
		const alts = KEY_ALIASES[target] || []
		const aliasHit = dataList.find(it => alts.indexOf(normKey(it.identifier != null ? it.identifier : it.name)) >= 0)
		return aliasHit ? aliasHit.value : undefined
	}
	const it = dataList[posIdx]
	return (it && typeof it === 'object') ? it.value : it
}

function num(v) { const n = Number(v); return (v === null || v === undefined || v === '' || isNaN(n)) ? null : n }

function str(v) {
	if (v === null || v === undefined) return ''
	if (typeof v === 'object') return ''
	return String(v).trim()
}

// 属性名不完全确定时，按关键字在云平台返回里找（兼容命名差异）
function pickByKeyword(dataList, keywords, excludes) {
	if (!Array.isArray(dataList)) return undefined
	const keys = keywords.map(normKey)
	const ex = (excludes || []).map(normKey)
	const hit = dataList.find(it => {
		if (!it || typeof it !== 'object') return false
		const k = normKey(it.identifier != null ? it.identifier : it.name)
		if (ex.some(w => w && k.indexOf(w) >= 0)) return false
		return keys.some(w => k.indexOf(w) >= 0)
	})
	return hit ? hit.value : undefined
}

// 云端档案既可能是数组，也可能是 JSON 字符串
function parseArchiveValue(v) {
	if (!v) return null
	if (Array.isArray(v)) return v
	if (typeof v === 'object') {
		if (Array.isArray(v.records)) return v.records
		if (Array.isArray(v.objects)) return v.objects
		if (v.name || v.object_name) return [v]
		return null
	}
	if (typeof v === 'string') {
		const s = v.trim()
		if (!s || (s.charAt(0) !== '[' && s.charAt(0) !== '{')) return null
		try {
			const parsed = JSON.parse(s)
			if (Array.isArray(parsed)) return parsed
			if (parsed && Array.isArray(parsed.records)) return parsed.records
			if (parsed && Array.isArray(parsed.objects)) return parsed.objects
			if (parsed && (parsed.name || parsed.object_name)) return [parsed]
		} catch (e) {}
	}
	return null
}

// 模式值 -> 文字；兼容云端直接上报中文串的情况
function modeWord(m) {
	if (m === null || m === undefined || m === '') return null
	const n = Number(m)
	if (isNaN(n)) return String(m)
	return MODE_WORDS[n] || ('模式' + n)
}

// 秒 -> MM:SS
function fmtSeconds(sec) {
	if (sec === null || sec === undefined || isNaN(Number(sec))) return '--:--'
	const s = Math.max(0, Math.round(Number(sec)))
	return String(Math.floor(s / 60)).padStart(2, '0') + ':' + String(s % 60).padStart(2, '0')
}

// 把云平台返回的属性数组解析成一次采样快照
function parseSnapshot(dataList) {
	const angles = CURRENT_KEYS.map((k, i) => num(pickValue(dataList, k, i)))
	const goals = GOAL_KEYS.map((k, i) => num(pickValue(dataList, k, 5 + i)))
	const objectName = str(pickValue(dataList, PROP_OBJECT, 14)) ||
		str(pickByKeyword(
			dataList,
			['wupin', 'object', 'item', 'targetname', 'shiwu'],
			['img', 'image', 'conf', 'count', 'archive', 'photo', 'pic']
		))
	const objectImage = str(pickValue(dataList, PROP_OBJECT_IMG, 15)) ||
		str(pickByKeyword(dataList, ['wupinimg', 'objectimg', 'image', 'img', 'photo']))
	const confidence = num(pickValue(dataList, PROP_OBJECT_CONF, 16))
	const confidenceFuzzy = confidence === null
		? num(pickByKeyword(dataList, ['conf', 'score', 'prob']))
		: confidence
	const graspCount = num(pickValue(dataList, PROP_GRASP_COUNT, 17))
	const graspCountFuzzy = graspCount === null
		? num(pickByKeyword(dataList, ['grabcount', 'graspcount', 'grabnum', 'grabcishu', 'zhuaciquanshu']))
		: graspCount
	const archiveRaw = pickValue(dataList, PROP_ARCHIVE, 18) !== undefined
		? pickValue(dataList, PROP_ARCHIVE, 18)
		: pickByKeyword(dataList, ['objectarchive', 'dangan'])
	const archive = parseArchiveValue(archiveRaw)
	return {
		angles,                                   // 五指当前角度 [5]，无效为 null
		goals: goals.some(v => v !== null) ? goals : null,  // 目标角度，没有则为 null（非训练模式）
		error: num(pickValue(dataList, PROP_ERROR, 10)),
		result: num(pickValue(dataList, PROP_RESULT, 11)),
		mode: num(pickValue(dataList, PROP_MODE, 12)),      // 0正常 / 1训练 / 2录入
		time: num(pickValue(dataList, PROP_TIME, 13)),      // 上一次训练时长（秒）
		objectName,                               // 当前识别物品（云端有则用云端）
		objectImage,
		confidence: confidenceFuzzy,
		graspCount: graspCountFuzzy,
		archive                                    // 云端档案数组或 null
	}
}

// 查询设备最新属性；返回解析后的快照（含 raw 原始数组）
async function queryDeviceProperty(productId, deviceName) {
	const pid = productId || DEFAULT_PRODUCT_ID
	const token = makeToken(pid)
	if (!token) throw new Error('Token 生成失败')
	const res = await uni.request({
		url: API_URL,
		method: 'GET',
		data: { product_id: pid, device_name: deviceName || DEFAULT_DEVICE_NAME },
		header: { 'authorization': token, 'Content-Type': 'application/json' },
		timeout: 10000
	})
	const body = res.data || {}
	if (body.code !== 0) throw new Error(body.msg || ('接口错误码 ' + body.code))
	const dataList = body.data || []
	const snap = parseSnapshot(dataList)
	snap.raw = dataList
	return snap
}

export {
	API_URL, AUTH, CURRENT_KEYS, GOAL_KEYS, PROP_ERROR, PROP_RESULT, PROP_MODE, PROP_TIME,
	PROP_OBJECT, PROP_OBJECT_IMG, PROP_OBJECT_CONF, PROP_GRASP_COUNT, PROP_ARCHIVE,
	MODE_WORDS, FINGER_CN, FINGER_ICON, RESULT_WORDS, RESULT_COLORS,
	DEFAULT_PRODUCT_ID, DEFAULT_DEVICE_NAME,
	modeWord, fmtSeconds, parseSnapshot, parseArchiveValue, queryDeviceProperty
}

export default {
	API_URL, AUTH, CURRENT_KEYS, GOAL_KEYS, PROP_ERROR, PROP_RESULT, PROP_MODE, PROP_TIME,
	PROP_OBJECT, PROP_OBJECT_IMG, PROP_OBJECT_CONF, PROP_GRASP_COUNT, PROP_ARCHIVE,
	MODE_WORDS, FINGER_CN, FINGER_ICON, RESULT_WORDS, RESULT_COLORS,
	DEFAULT_PRODUCT_ID, DEFAULT_DEVICE_NAME,
	modeWord, fmtSeconds, parseSnapshot, parseArchiveValue, queryDeviceProperty
}

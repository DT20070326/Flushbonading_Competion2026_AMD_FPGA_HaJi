// 未记录物品的视觉识别服务
// 支持：演示模式（离线，不联网）/ 智谱 GLM-4V / 通义千问 VL / 自定义 OpenAI 兼容接口
// Key 只保存在本机 storage，不上传到其它地方。

const CFG_KEY = 'grasp_vision_cfg'

const PROVIDERS = {
	demo: {
		label: '演示识别',
		short: 'DEMO',
		model: 'local-demo',
		url: '',
		tip: '离线演示，结果来自内置样例，不调用外部接口'
	},
	zhipu: {
		label: '智谱 GLM-4V',
		short: 'GLM',
		model: 'glm-4v-flash',
		url: 'https://open.bigmodel.cn/api/paas/v4/chat/completions',
		tip: '在 open.bigmodel.cn 创建 API Key 后填入'
	},
	qwen: {
		label: '通义千问 VL',
		short: 'QWEN',
		model: 'qwen-vl-plus',
		url: 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
		tip: '在阿里云百炼创建 DashScope API Key 后填入'
	},
	custom: {
		label: '自定义接口',
		short: 'API',
		model: '',
		url: '',
		tip: '填写 OpenAI 兼容的 /chat/completions 地址与模型名'
	}
}

// 抓握类型 -> 五指舵机建议值（拇指 / 食指 / 中指 / 无名指 / 小指）
const GRASP_SERVO = {
	cylinder: [64, 74, 78, 76, 70],
	sphere: [86, 90, 92, 88, 82],
	pinch: [38, 52, 58, 50, 42],
	hook: [72, 82, 86, 82, 70],
	palm: [22, 32, 36, 34, 28],
	power: [88, 94, 96, 92, 86]
}

const GRASP_LABEL = {
	cylinder: '圆柱抓握',
	sphere: '球形抓握',
	pinch: '指尖捏取',
	hook: '钩状抓握',
	palm: '平掌托举',
	power: '强力包握'
}

// 演示模式内置结果
const DEMO_LIBRARY = {
	cup: { name: '水杯', category: '生活用品', graspKey: 'cylinder', confidence: 0.96, description: '圆柱形杯身，建议从杯身中段包握，保持杯口朝上。' },
	apple: { name: '苹果', category: '食物', graspKey: 'sphere', confidence: 0.94, description: '球形果实，适合整掌包覆抓取，注意减小指尖压力。' },
	pill: { name: '药瓶', category: '医疗用品', graspKey: 'pinch', confidence: 0.91, description: '小直径瓶体，建议指尖捏取并控制夹持力，避免压裂。' },
	remote: { name: '遥控器', category: '电子设备', graspKey: 'cylinder', confidence: 0.89, description: '长条扁平物体，沿短边包握，避开按键区域。' },
	tissue: { name: '纸巾盒', category: '生活用品', graspKey: 'palm', confidence: 0.9, description: '方正软质盒体，适合平掌托举配合侧向夹持。' },
	bowl: { name: '小碗', category: '餐具', graspKey: 'palm', confidence: 0.93, description: '开口容器，托住碗底并让拇指扣住碗沿。' },
	glasses: { name: '眼镜盒', category: '随身物品', graspKey: 'palm', confidence: 0.88, description: '扁长硬盒，建议平掌托举，避免单侧受力。' },
	milk: { name: '牛奶盒', category: '食物', graspKey: 'cylinder', confidence: 0.92, description: '方形纸盒，抓握时让五指贴合四个侧面。' },
	banana: { name: '香蕉', category: '食物', graspKey: 'hook', confidence: 0.87, description: '弯曲软质物体，适合钩状抓握，减小接触压力。' },
	key: { name: '钥匙串', category: '随身物品', graspKey: 'pinch', confidence: 0.84, description: '小体积金属件，建议指尖捏取并提高夹持稳定性。' },
	box: { name: '收纳盒', category: '生活用品', graspKey: 'power', confidence: 0.86, description: '规则盒体，采用强力包握，注意先对齐再闭合。' },
	mouse: { name: '鼠标', category: '电子设备', graspKey: 'power', confidence: 0.93, description: '掌部包住鼠标，拇指与无名指夹住两侧，避免压到按键。' }
}

function defaultConfig() {
	return { provider: 'demo', key: '', url: '', model: '' }
}

function getConfig() {
	try {
		const raw = uni.getStorageSync(CFG_KEY)
		if (raw) {
			const cfg = typeof raw === 'string' ? JSON.parse(raw) : raw
			if (cfg && cfg.provider) return Object.assign(defaultConfig(), cfg)
		}
	} catch (e) {}
	return defaultConfig()
}

function saveConfig(cfg) {
	const next = Object.assign(defaultConfig(), cfg || {})
	try { uni.setStorageSync(CFG_KEY, JSON.stringify(next)) } catch (e) {}
	return next
}

function providerInfo(cfg) {
	const c = cfg || getConfig()
	return Object.assign({}, PROVIDERS[c.provider] || PROVIDERS.demo)
}

function isDemo() {
	return getConfig().provider === 'demo'
}

function sleep(ms) {
	return new Promise(resolve => setTimeout(resolve, ms))
}

function hashString(s) {
	let h = 0
	const str = String(s || '')
	for (let i = 0; i < str.length; i++) h = (h * 31 + str.charCodeAt(i)) >>> 0
	return h
}

function normalizeGrasp(raw) {
	const s = String(raw || '').toLowerCase()
	if (s.indexOf('圆柱') >= 0 || s.indexOf('cylinder') >= 0 || s.indexOf('杯') >= 0) return 'cylinder'
	if (s.indexOf('球') >= 0 || s.indexOf('sphere') >= 0) return 'sphere'
	if (s.indexOf('捏') >= 0 || s.indexOf('pinch') >= 0 || s.indexOf('指尖') >= 0) return 'pinch'
	if (s.indexOf('钩') >= 0 || s.indexOf('hook') >= 0) return 'hook'
	if (s.indexOf('掌') >= 0 || s.indexOf('托') >= 0 || s.indexOf('palm') >= 0) return 'palm'
	if (s.indexOf('包') >= 0 || s.indexOf('power') >= 0 || s.indexOf('强') >= 0) return 'power'
	return 'cylinder'
}

function buildResult(name, category, graspKey, confidence, description, extra) {
	const key = GRASP_SERVO[graspKey] ? graspKey : 'cylinder'
	return Object.assign({
		name: String(name || '').trim() || '待命名物品',
		category: String(category || '').trim() || '未分类',
		graspKey: key,
		graspType: GRASP_LABEL[key],
		servo: GRASP_SERVO[key].slice(),
		confidence: Number(confidence) || 0,
		description: String(description || '').trim()
	}, extra || {})
}

function demoRecognize(hint, image) {
	const keys = Object.keys(DEMO_LIBRARY)
	const key = hint && DEMO_LIBRARY[hint] ? hint : keys[hashString(image || Date.now()) % keys.length]
	const d = DEMO_LIBRARY[key]
	return buildResult(d.name, d.category, d.graspKey, d.confidence, d.description, {
		demo: true,
		provider: 'demo',
		providerLabel: PROVIDERS.demo.label,
		hint: key
	})
}

// H5 / App 图片转 base64；远程 URL 直接交给视觉大模型抓取
function toDataUrl(image) {
	return new Promise((resolve, reject) => {
		const src = String(image || '')
		if (!src) { reject(new Error('没有可识别的图片')); return }
		if (/^data:/i.test(src)) { resolve(src); return }
		if (/^https?:/i.test(src)) { resolve(src); return }

		// #ifdef H5
		fetch(src)
			.then(r => { if (!r.ok) throw new Error('图片读取失败'); return r.blob() })
			.then(blob => {
				const reader = new FileReader()
				reader.onload = () => resolve(reader.result)
				reader.onerror = () => reject(new Error('图片编码失败'))
				reader.readAsDataURL(blob)
			})
			.catch(reject)
		return
		// #endif

		// #ifndef H5
		try {
			uni.getFileSystemManager().readFile({
				filePath: src,
				encoding: 'base64',
				success: r => resolve('data:image/jpeg;base64,' + r.data),
				fail: () => reject(new Error('图片读取失败'))
			})
		} catch (e) { reject(new Error('图片读取失败')) }
		// #endif
	})
}

function parseModelText(body) {
	const choice = body && body.choices && body.choices[0]
	const msg = choice && choice.message
	if (!msg) return ''
	const content = msg.content
	if (typeof content === 'string') return content.trim()
	if (Array.isArray(content)) {
		return content.map(x => (x && (x.text || x.content)) || '').join('').trim()
	}
	return ''
}

function parseJsonBlock(text) {
	const raw = String(text || '')
	const start = raw.indexOf('{')
	const end = raw.lastIndexOf('}')
	if (start < 0 || end <= start) return null
	try { return JSON.parse(raw.slice(start, end + 1)) } catch (e) { return null }
}

async function apiRecognize(image, cfg) {
	const provider = PROVIDERS[cfg.provider] || PROVIDERS.custom
	const url = (cfg.provider === 'custom' ? cfg.url : provider.url) || provider.url
	const model = cfg.model || provider.model
	if (!url) throw new Error('请先填写视觉 API 地址')
	if (!model) throw new Error('请先填写视觉模型名称')
	if (!cfg.key) throw new Error('请先在识别设置中填写 API Key')

	const dataUrl = await toDataUrl(image)
	const prompt = '你是助残仿生手的视觉感知模块。识别图片中最主要的可抓取物品，并给出抓握方式建议。'
		+ '只返回一个 JSON 对象，不要 markdown，不要解释。格式：'
		+ '{"name":"物品中文名","category":"类别","confidence":0.95,"graspType":"圆柱抓握/球形抓握/指尖捏取/钩状抓握/平掌托举/强力包握 之一","description":"20字以内的抓取建议"}'

	const body = {
		model,
		messages: [{
			role: 'user',
			content: [
				{ type: 'image_url', image_url: { url: dataUrl } },
				{ type: 'text', text: prompt }
			]
		}],
		temperature: 0.1,
		max_tokens: 300
	}

	const res = await uni.request({
		url,
		method: 'POST',
		header: {
			'Content-Type': 'application/json',
			'Authorization': 'Bearer ' + cfg.key
		},
		data: body,
		timeout: 30000
	})
	const data = res.data || {}
	if (data.error) throw new Error(data.error.message || '视觉接口返回错误')
	const text = parseModelText(data)
	if (!text) throw new Error('视觉接口未返回识别结果')
	const parsed = parseJsonBlock(text)
	if (!parsed) throw new Error('识别结果格式无法解析')

	let confidence = Number(parsed.confidence)
	if (isNaN(confidence)) confidence = 0
	if (confidence > 1) confidence = confidence / 100
	confidence = Math.max(0, Math.min(1, confidence))
	const graspKey = normalizeGrasp(parsed.graspType || parsed.grasp)
	return buildResult(
		parsed.name || parsed.objectName || parsed.label,
		parsed.category || parsed.class,
		graspKey,
		confidence,
		parsed.description || parsed.advice,
		{ demo: false, provider: cfg.provider, providerLabel: provider.label }
	)
}

// image：图片路径 / dataURL / 远程 URL；hint：演示样例的固定键（可选）
async function recognize(image, hint) {
	const cfg = getConfig()
	if (cfg.provider === 'demo') {
		await sleep(850)
		return demoRecognize(hint, image)
	}
	try {
		return await apiRecognize(image, cfg)
	} catch (e) {
		throw new Error(e.message || e.errMsg || '识别失败')
	}
}

export {
	CFG_KEY, PROVIDERS, GRASP_SERVO, GRASP_LABEL, DEMO_LIBRARY,
	getConfig, saveConfig, providerInfo, isDemo, recognize, toDataUrl
}

export default {
	CFG_KEY, PROVIDERS, GRASP_SERVO, GRASP_LABEL, DEMO_LIBRARY,
	getConfig, saveConfig, providerInfo, isDemo, recognize, toDataUrl
}

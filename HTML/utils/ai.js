// DeepSeek 康复分析报告（做法参考 ACCESS 的物种分布分析）
const DEEPSEEK_KEY = 'sk-0d29ea22c6c9426a94e8fba8e0629a5e'
const API_URL = 'https://api.deepseek.com/v1/chat/completions'

async function chat(messages, maxTokens, temperature) {
	const res = await uni.request({
		url: API_URL,
		method: 'POST',
		header: { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + DEEPSEEK_KEY },
		data: {
			model: 'deepseek-chat',
			messages,
			max_tokens: maxTokens || 400,
			temperature: temperature === undefined ? 0.6 : temperature
		},
		timeout: 20000
	})
	const body = res.data || {}
	const msg = body.choices && body.choices[0] && body.choices[0].message
	return (msg && msg.content ? msg.content : '').trim()
}

function fmtDay(ts) {
	const d = new Date(ts)
	const pad = n => String(n).padStart(2, '0')
	return `${pad(d.getMonth() + 1)}-${pad(d.getDate())}`
}

// 根据患者近期训练场次生成康复分析报告
async function rehabReport(patient) {
	const sessions = (patient.sessions || []).slice(-10)
	if (!sessions.length) return ''

	const lines = sessions.map((s, i) => {
		const start = s.startTs || 0
		const end = s.endTs || start
		const min = Math.max(0, Math.round((end - start) / 60000))
		const errs = s.errors || []
		const avgErr = errs.length ? Math.round(errs.reduce((a, b) => a + b, 0) / errs.length) : null
		const res = (s.results && s.results.length) ? s.results[s.results.length - 1] : null
		const goals = s.lastGoals ? s.lastGoals.join('/') : '--'
		const perf = (s.lastAngles || []).join('/')
		return `第${i + 1}次 ${fmtDay(start)} 时长${min}分 效果${res === null ? '--' : res} 平均误差${avgErr === null ? '--' : avgErr + '度'} 目标值${goals} 实际值${perf}`
	})

	const info = `${patient.name}（${patient.gender} ${patient.age}岁，患侧${patient.hand}，${patient.note || '无备注'}）`
	const sys = '你是康复科医生。根据患者近几次手部康复训练数据，用中文写一段100-160字的康复分析报告：'
		+ '包含整体进步趋势、训练时长与效果的关系、误差变化、以及后续训练建议。'
		+ '只输出报告正文，不要标题、不要 markdown 标记、不要分点符号。'
	const user = `患者：${info}\n训练数据（由早到晚；效果 0较差/1良/2好/3优秀；目标值、实际值均为五指舵机角度）：\n` + lines.join('\n')

	return chat([
		{ role: 'system', content: sys },
		{ role: 'user', content: user }
	], 400, 0.6)
}

// 针对某一次训练生成单独的分析
async function sessionReport(patient, session) {
	const start = session.startTs || 0
	const end = session.endTs || start
	const min = Math.max(0, Math.round((end - start) / 60000))
	const errs = session.errors || []
	const avgErr = errs.length ? Math.round(errs.reduce((a, b) => a + b, 0) / errs.length) : null
	const res = (session.results && session.results.length) ? session.results[session.results.length - 1] : null

	const series = session.angleSeries || []
	const names = ['大拇', '食指', '中指', '无名', '小指']
	let perf = '无逐条舵机值'
	if (series.length) {
		const avg = [0, 1, 2, 3, 4].map(f => {
			let sum = 0, cnt = 0
			series.forEach(x => {
				const v = x.angles ? x.angles[f] : null
				if (v !== null && v !== undefined) { sum += v; cnt++ }
			})
			return cnt ? Math.round(sum / cnt) : null
		})
		const goals = (series[series.length - 1].goals) || session.lastGoals || []
		perf = names.map((n, f) => {
			const g = goals[f]
			return `${n} 目标${g === null || g === undefined ? '--' : Math.round(g)}/原始${avg[f] === null ? '--' : avg[f]}`
		}).join('，')
	}

	const info = `${patient.name}（${patient.gender} ${patient.age}岁，患侧${patient.hand}，${patient.note || '无备注'}）`
	const sys = '你是康复科医生。针对这一次手部康复训练写一段60-100字的中文分析：'
		+ '本次完成情况、哪几根手指偏离目标较大、下一步训练建议。'
		+ '只输出正文，不要标题、不要 markdown 标记、不要分点符号。'
	const user = `患者：${info}\n本次训练：${fmtDay(start)}，时长${min}分钟，训练效果${res === null ? '--' : res}`
		+ `（0较差/1良/2好/3优秀），平均误差${avgErr === null ? '--' : avgErr + '度'}\n五指表现（目标/原始，单位度）：${perf}`

	return chat([
		{ role: 'system', content: sys },
		{ role: 'user', content: user }
	], 260, 0.6)
}

export { DEEPSEEK_KEY, chat, rehabReport, sessionReport }
export default { DEEPSEEK_KEY, chat, rehabReport, sessionReport }

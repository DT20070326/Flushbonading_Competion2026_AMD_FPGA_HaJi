<template>
	<view class="duration-page">
		<!-- 患者信息 -->
		<view class="patient-head">
			<text class="ph-emoji">{{ avatarOf() }}</text>
			<view class="ph-info">
				<view class="ph-name-row">
					<text class="ph-name">{{ patient.name }}</text>
					<text class="ph-hand">{{ patient.hand }}</text>
				</view>
				<text class="ph-meta">{{ patient.gender }} · {{ patient.age }}岁 · {{ patient.note || '—' }}</text>
				<text class="ph-dev">{{ patient.deviceName }} · {{ patient.online ? '设备在线' : '设备离线' }}</text>
			</view>
		</view>

		<!-- 汇总 -->
		<view class="summary">
			<view class="sum-item"><text class="sum-num">{{ summary.count }}</text><text class="sum-label">训练场次</text></view>
			<view class="sum-item"><text class="sum-num">{{ summary.totalMin }}</text><text class="sum-label">累计分钟</text></view>
			<view class="sum-item"><text class="sum-num">{{ summary.avgMin }}</text><text class="sum-label">场均分钟</text></view>
			<view class="sum-item"><text class="sum-num">{{ summary.lastResult }}</text><text class="sum-label">最近效果</text></view>
		</view>

		<!-- 柱状图 -->
		<view class="chart-card">
			<view class="chart-tabs">
				<view class="chart-tab" :class="{ active: tab === 'duration' }" @click="switchTab('duration')">⏱ 训练时长</view>
				<view class="chart-tab" :class="{ active: tab === 'result' }" @click="switchTab('result')">🏅 训练效果</view>
			</view>
			<view class="chart-wrap">
				<canvas canvas-id="d_chart" id="d_chart" class="d-canvas"
					:style="{ width: chartW + 'px', height: chartH + 'px' }"></canvas>
			</view>
			<text class="chart-hint">{{ tab === 'duration' ? '纵轴：每次训练时长（分钟）' : '纵轴：训练效果（0 较差 ~ 3 优秀）' }}</text>
		</view>

		<!-- DeepSeek 整体分析 -->
		<view class="ai-card">
			<view class="ai-head">
				<text class="ai-title">🤖 DeepSeek 康复分析</text>
				<view class="ai-btn" :class="{ busy: analyzing }" @click="genReport">{{ analyzing ? '分析中…' : '重新生成' }}</view>
			</view>
			<view class="ai-content" v-if="analysis">{{ analysis }}</view>
			<view class="ai-content ai-loading" v-else>{{ analyzing ? '正在生成分析报告…' : '暂无分析报告' }}</view>
		</view>

		<view class="back-btn" @click="goBack">← 返回</view>
	</view>
</template>

<script>
import store from '@/utils/store.js'
import ai from '@/utils/ai.js'

export default {
	data() {
		return {
			patientId: '',
			patient: {},
			tab: 'duration',
			chartW: 340,
			chartH: 220,
			analysis: '',
			analyzing: false
		}
	},
	computed: {
		sessions() { return (this.patient.sessions || []) },
		summary() {
			const list = this.sessions.map(s => store.sessionStats(s))
			const count = list.length
			const totalMin = Math.round(list.reduce((a, s) => a + s.minutes, 0))
			const avgMin = count ? Math.round(totalMin / count * 10) / 10 : 0
			const last = list[list.length - 1]
			return { count, totalMin, avgMin, lastResult: last ? last.resultWord : '--' }
		}
	},
	onLoad(options) {
		this.patientId = (options && options.id) || ''
		this.loadPatient()
		this.$nextTick(() => { this.drawChart() })
	},
	onReady() { setTimeout(() => { this.drawChart() }, 300) },
	onShow() { this.loadPatient() },
	methods: {
		loadPatient() {
			let p = this.patientId ? store.getPatient(this.patientId) : null
			if (!p) {
				const list = store.loadPatients()
				p = list[0] || { sessions: [] }
				this.patientId = p.id || ''
			}
			this.patient = p
			this.analysis = p.aiReport || ''
			this.$nextTick(() => { this.drawChart() })
			this.maybeGenReport()
		},
		avatarOf() {
			const p = this.patient
			if (p.gender === '女') return p.age >= 60 ? '👵' : '👩'
			return p.age >= 60 ? '👴' : '👨'
		},
		switchTab(t) {
			this.tab = t
			this.$nextTick(() => { setTimeout(() => this.drawChart(), 120) })
		},
		drawChart() {
			const sys = uni.getSystemInfoSync()
			const sessions = this.sessions.slice(-10)
			this.chartW = sys.windowWidth - 72
			this.chartH = Math.floor(this.chartW * 0.42)
			this.$nextTick(() => {
				const labels = sessions.map((s, i) => '第' + (this.sessions.length - sessions.length + i + 1) + '次')
				if (this.tab === 'duration') {
					const data = sessions.map(s => Math.max(1, Math.round(store.sessionStats(s).minutes)))
					this.drawBar('d_chart', labels, data, '#26a69a', 0)
				} else {
					const data = sessions.map(s => {
						const st = store.sessionStats(s)
						return st.lastResult === null ? 0 : st.lastResult
					})
					this.drawBar('d_chart', labels, data, '#fb8c00', 4)
				}
			})
		},
		drawBar(canvasId, labels, data, color, maxOverride) {
			const ctx = uni.createCanvasContext(canvasId, this)
			const W = this.chartW, H = this.chartH
			const pad = { top: 26, right: 16, bottom: 36, left: 46 }
			const pw = W - pad.left - pad.right
			const ph = H - pad.top - pad.bottom
			ctx.setFillStyle('#ffffff'); ctx.fillRect(0, 0, W, H)

			if (!data.length) {
				ctx.setFontSize(13); ctx.setFillStyle('#bdbdbd'); ctx.setTextAlign('center')
				ctx.fillText('暂无训练数据', W / 2, H / 2)
				ctx.draw()
				return
			}

			const maxVal = maxOverride || Math.ceil(Math.max(1, Math.max.apply(null, data)) * 1.25)
			const count = data.length
			const slot = pw / count
			const barW = Math.max(8, Math.floor(slot * 0.55))

			const gridN = 4
			for (let i = 0; i <= gridN; i++) {
				const y = pad.top + (ph / gridN) * i
				ctx.beginPath()
				ctx.setStrokeStyle('#eeeeee'); ctx.setLineWidth(1)
				ctx.moveTo(pad.left, y); ctx.lineTo(W - pad.right, y); ctx.stroke()
				ctx.setFontSize(10); ctx.setFillStyle('#9e9e9e'); ctx.setTextAlign('right')
				ctx.fillText(String(Math.round(maxVal * (gridN - i) / gridN)), pad.left - 6, y + 4)
			}

			for (let i = 0; i < count; i++) {
				const v = data[i]
				const barH = maxVal > 0 ? (v / maxVal) * ph : 0
				const x = pad.left + i * slot + (slot - barW) / 2
				const y = pad.top + ph - barH
				ctx.setFillStyle(color)
				ctx.fillRect(x, y, barW, barH)
				if (v > 0) {
					ctx.setFontSize(11); ctx.setFillStyle('#455a64'); ctx.setTextAlign('center')
					ctx.fillText(String(v), x + barW / 2, y - 6)
				}
				if (count <= 6 || i % 2 === 0) {
					ctx.setFontSize(10); ctx.setFillStyle('#9e9e9e'); ctx.setTextAlign('center')
					ctx.fillText(labels[i], x + barW / 2, H - 8)
				}
			}
			ctx.draw()
		},
		maybeGenReport() {
			if (this.analysis || this.analyzing) return
			if (!(this.patient.sessions || []).length) return
			this.genReport()
		},
		async genReport() {
			if (this.analyzing) return
			if (!(this.patient.sessions || []).length) {
				uni.showToast({ title: '暂无训练数据', icon: 'none' })
				return
			}
			this.analyzing = true
			try {
				const text = await ai.rehabReport(this.patient)
				if (text) {
					this.analysis = text
					store.updatePatient(this.patientId, { aiReport: text })
				} else {
					this.analysis = '分析暂不可用'
				}
			} catch (e) {
				this.analysis = '分析暂不可用'
			}
			this.analyzing = false
		},
		goBack() { uni.navigateBack() }
	}
}
</script>

<style scoped>
.duration-page {
	min-height: 100vh;
	background: linear-gradient(180deg, #e3f2fd 0%, #f5f9ff 50%, #ffffff 100%);
	padding: 30rpx 36rpx 60rpx;
}

.patient-head {
	display: flex; align-items: center; gap: 24rpx;
	background: linear-gradient(135deg, #42a5f5 0%, #1565c0 100%);
	border-radius: 28rpx; padding: 32rpx 36rpx; margin-bottom: 28rpx;
	box-shadow: 0 10rpx 30rpx rgba(21,101,192,0.25);
}
.ph-emoji { font-size: 96rpx; line-height: 1; }
.ph-info { flex: 1; min-width: 0; }
.ph-name-row { display: flex; align-items: center; gap: 16rpx; }
.ph-name { font-size: 42rpx; font-weight: 800; color: #fff; }
.ph-hand { font-size: 22rpx; color: #1565c0; background: #fff; padding: 4rpx 16rpx; border-radius: 20rpx; }
.ph-meta { display: block; font-size: 26rpx; color: rgba(255,255,255,0.9); margin-top: 8rpx; }
.ph-dev { display: block; font-size: 24rpx; color: rgba(255,255,255,0.75); margin-top: 4rpx; }

.summary {
	display: flex; background: #fff; border-radius: 24rpx; padding: 28rpx 0;
	box-shadow: 0 4rpx 18rpx rgba(0,0,0,0.05); margin-bottom: 28rpx;
}
.sum-item { flex: 1; display: flex; flex-direction: column; align-items: center; }
.sum-num { font-size: 40rpx; font-weight: 800; color: #1565c0; }
.sum-label { font-size: 22rpx; color: #90a4ae; margin-top: 6rpx; }

.chart-card {
	background: #fff; border-radius: 24rpx; padding: 28rpx 20rpx 20rpx;
	box-shadow: 0 4rpx 18rpx rgba(0,0,0,0.05); margin-bottom: 28rpx;
	display: flex; flex-direction: column; align-items: center;
}
.chart-tabs { display: flex; gap: 20rpx; margin-bottom: 20rpx; }
.chart-tab { font-size: 28rpx; color: #78909c; padding: 12rpx 34rpx; border-radius: 28rpx; background: #f5f5f5; }
.chart-tab.active { background: #1565c0; color: #fff; font-weight: 600; }
.chart-wrap { display: flex; justify-content: center; }
.d-canvas { background: #fff; }
.chart-hint { font-size: 22rpx; color: #b0bec5; margin-top: 12rpx; }

.ai-card {
	background: #fff; border-radius: 24rpx; padding: 28rpx 28rpx;
	box-shadow: 0 4rpx 18rpx rgba(0,0,0,0.05); margin-bottom: 28rpx;
	border-left: 10rpx solid #7e57c2;
}
.ai-head { display: flex; align-items: center; justify-content: space-between; margin-bottom: 16rpx; }
.ai-title { font-size: 32rpx; font-weight: 700; color: #4527a0; }
.ai-btn { font-size: 24rpx; color: #7e57c2; padding: 8rpx 24rpx; border: 1rpx solid #7e57c2; border-radius: 24rpx; }
.ai-btn.busy { opacity: 0.5; }
.ai-btn:active { opacity: 0.7; }
.ai-content { font-size: 28rpx; color: #455a64; line-height: 1.9; }
.ai-loading { color: #b0bec5; }

.back-btn { text-align: center; padding: 40rpx 0 0; font-size: 28rpx; color: #5c6bc0; }
</style>

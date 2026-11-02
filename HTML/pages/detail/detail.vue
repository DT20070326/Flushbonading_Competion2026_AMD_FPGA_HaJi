<template>
	<view class="records-page">
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

		<!-- 训练记录 -->
		<view class="records-head">
			<text class="records-title">📋 训练记录（共 {{ recentTotal }} 次）</text>
			<view class="records-head-btns">
				<text class="mini-btn" @click="goDuration">📈 训练分析</text>
				<text class="mini-btn export" @click="exportCsv" v-if="recentTotal > 0">导出</text>
			</view>
		</view>
		<text class="records-hint">点某条记录进去，可看该时段全部舵机值（目标 / 原始）和 DeepSeek 单独分析</text>

		<view v-if="recent.length > 0">
			<view class="rec-card" v-for="(r, i) in recent" :key="r.id" @click="openSession(r)">
				<view class="rc-badge">{{ recentTotal - i }}</view>
				<view class="rc-main">
					<view class="rc-line1">
						<text class="rc-title">{{ fmtTime(r.startTs) }}</text>
						<text class="result-tag" :style="{ background: resultBg(r.lastResult) }">{{ r.resultWord }}</text>
					</view>
					<text class="rc-meta">第 {{ recentTotal - i }} 次 · 时长 {{ store.fmtDuration(r.durationMs) }} · 误差 {{ r.avgError === null ? '--' : r.avgError + '°' }}</text>
				</view>
			</view>
		</view>
		<view class="rec-empty" v-else>暂无训练记录，患者端连接设备后自动累积</view>

		<view class="back-btn" @click="goBack">← 返回</view>
	</view>
</template>

<script>
import cloud from '@/utils/cloud.js'
import store from '@/utils/store.js'

export default {
	data() {
		return {
			patientId: '',
			patient: {},
			store
		}
	},
	computed: {
		sessions() { return (this.patient.sessions || []) },
		recent() { return this.sessions.slice().reverse().map(s => store.sessionStats(s)) },
		recentTotal() { return this.sessions.length }
	},
	onLoad(options) {
		this.patientId = (options && options.id) || ''
		this.loadPatient()
	},
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
		},
		avatarOf() {
			const p = this.patient
			if (p.gender === '女') return p.age >= 60 ? '👵' : '👩'
			return p.age >= 60 ? '👴' : '👨'
		},
		resultBg(n) {
			if (n === null || n === undefined) return '#bdbdbd'
			return cloud.RESULT_COLORS[Math.max(0, Math.min(3, Math.round(n)))]
		},
		fmtTime(ts) {
			if (!ts) return '--'
			const d = new Date(ts)
			const pad = n => String(n).padStart(2, '0')
			return `${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`
		},
		openSession(r) {
			uni.navigateTo({ url: '/pages/session/session?pid=' + this.patientId + '&sid=' + r.id })
		},
		goDuration() {
			uni.navigateTo({ url: '/pages/duration/duration?id=' + this.patientId })
		},
		exportCsv() {
			const headers = ['序号', '开始时间', '结束时间', '时长(分钟)', '训练效果', '平均误差(度)', '采样点数']
			let csv = '﻿' + headers.join(',') + '\n'
			this.sessions.forEach((s, i) => {
				const st = store.sessionStats(s)
				const row = [
					i + 1,
					`"${store.nowString(new Date(s.startTs))}"`,
					`"${store.nowString(new Date(s.endTs || s.startTs))}"`,
					st.minutes,
					`"${st.resultWord}"`,
					st.avgError === null ? '' : st.avgError,
					st.samples
				]
				csv += row.join(',') + '\n'
			})
			this.download(csv, `康复训练数据_${this.patient.name}_${store.nowString().slice(0, 10)}.csv`)
		},
		download(csv, filename) {
			// #ifdef H5
			const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' })
			const url = URL.createObjectURL(blob)
			const a = document.createElement('a')
			a.href = url; a.download = filename
			document.body.appendChild(a); a.click()
			document.body.removeChild(a); URL.revokeObjectURL(url)
			uni.showToast({ title: '下载成功', icon: 'success' })
			// #endif
			// #ifndef H5
			uni.setClipboardData({
				data: csv,
				success: () => { uni.showToast({ title: 'CSV已复制到剪贴板，粘贴到Excel即可', icon: 'none', duration: 3000 }) }
			})
			// #endif
		},
		goBack() { uni.navigateBack() }
	}
}
</script>

<style scoped>
.records-page {
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

.records-head { display: flex; align-items: center; justify-content: space-between; margin-bottom: 8rpx; }
.records-title { font-size: 32rpx; font-weight: 700; color: #263238; }
.records-hint { display: block; font-size: 22rpx; color: #b0bec5; margin-bottom: 20rpx; }
.records-head-btns { display: flex; gap: 14rpx; }
.mini-btn { font-size: 24rpx; padding: 8rpx 22rpx; border-radius: 24rpx; color: #1565c0; border: 1rpx solid #1565c0; }
.mini-btn.export { color: #00897b; border-color: #00897b; }
.mini-btn:active { opacity: 0.7; }

.rec-card {
	display: flex; align-items: center; gap: 20rpx;
	background: #fff; border-radius: 24rpx; margin-bottom: 22rpx; padding: 26rpx 24rpx;
	box-shadow: 0 4rpx 18rpx rgba(0,0,0,0.05); border-left: 10rpx solid #1565c0;
}
.rec-card:active { background: #f5f9ff; }
.rc-badge {
	width: 64rpx; height: 64rpx; border-radius: 50%; flex-shrink: 0;
	background: #e3f2fd; color: #1565c0; font-size: 28rpx; font-weight: 800;
	display: flex; align-items: center; justify-content: center;
}
.rc-main { flex: 1; min-width: 0; }
.rc-line1 { display: flex; align-items: center; gap: 14rpx; }
.rc-title { font-size: 30rpx; font-weight: 700; color: #263238; }
.rc-meta { display: block; font-size: 24rpx; color: #78909c; margin-top: 8rpx; }
.result-tag { font-size: 22rpx; color: #fff; padding: 4rpx 18rpx; border-radius: 18rpx; }

.rec-empty { text-align: center; font-size: 26rpx; color: #bdbdbd; padding: 100rpx 0; }
.back-btn { text-align: center; padding: 40rpx 0 0; font-size: 28rpx; color: #5c6bc0; }
</style>

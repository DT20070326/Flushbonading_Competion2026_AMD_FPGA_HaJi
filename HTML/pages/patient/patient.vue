<template>
	<view class="patient-page">
		<view class="page-header">
			<UserAvatar />🖐 患者端 · 训练监测
		</view>

		<!-- 患者选择 -->
		<scroll-view scroll-x class="picker" v-if="patients.length > 1">
			<view class="picker-inner">
				<view class="chip" v-for="p in patients" :key="p.id"
					:class="{ active: p.id === patientId }" @click="selectPatient(p.id)">
					{{ p.name }}
					<text class="chip-dot" :class="{ on: p.online }"></text>
				</view>
			</view>
		</scroll-view>

		<!-- 状态条：参考 KV260 面板标题栏（模式 + 连接信号） -->
		<view class="status-bar">
			<view class="sb-left">
				<text class="sb-title">{{ patient.name || '未选择患者' }}</text>
				<text class="sb-sub">{{ patient.deviceName || '—' }}</text>
			</view>
			<view class="sb-right">
				<text class="mode-badge" :class="modeClass">{{ modeText }}</text>
				<view class="signal" :class="{ on: online }">
					<text class="signal-icon">{{ online ? '📶' : '🚫' }}</text>
					<text class="signal-text">{{ online ? '有信号' : '无信号' }}</text>
				</view>
			</view>
		</view>

		<!-- 计时 / 上次训练 / 效果 / 误差（参考 KV260 录入信息行） -->
		<view class="timer-bar">
			<view class="timer-item"><text class="timer-k">本次计时</text><text class="timer-v">{{ elapsedText }}</text></view>
			<view class="timer-item"><text class="timer-k">上次训练</text><text class="timer-v">{{ lastTrainText }}</text></view>
			<view class="timer-item"><text class="timer-k">训练效果</text><text class="timer-v" :style="{ color: resultColor }">{{ resultText }}</text></view>
			<view class="timer-item"><text class="timer-k">误差</text><text class="timer-v">{{ errorText }}</text></view>
		</view>

		<!-- 五指角度（当前 + 目标） -->
		<view class="section-title">🖐 手指角度</view>
		<view class="finger-flex">
			<view class="finger-card" v-for="(name, i) in fingerNames" :key="i" :style="{ borderTopColor: fingerColors[i] }">
				<text class="finger-icon">{{ fingerIcons[i] }}</text>
				<text class="finger-name">{{ name }}</text>
				<view class="finger-val">
					<text class="finger-num">{{ angleText(i) }}</text>
					<text class="finger-unit">°</text>
				</view>
				<view class="finger-goal" v-if="goals">
					<text class="goal-label">目标 {{ goals[i] === null ? '--' : goals[i] }}</text>
				</view>
			</view>
		</view>

		<!-- 操作 -->
		<view class="actions">
			<view class="act-btn" @click="refresh(true)">⟳ 刷新</view>
			<view class="act-btn" @click="goDuration">📈 训练分析</view>
			<view class="act-btn primary" @click="goDetail">📋 训练记录</view>
			<view class="act-btn" @click="goRaw">📡 原始数据</view>
		</view>

		<view class="footer">
			<text class="update-time" v-if="updateTime">更新于 {{ updateTime }}</text>
			<view class="back-btn" @click="goBack">← 返回首页</view>
		</view>
	</view>
</template>

<script>
import UserAvatar from '@/components/UserAvatar.vue'
import cloud from '@/utils/cloud.js'
import store from '@/utils/store.js'

export default {
	components: { UserAvatar },
	data() {
		return {
			patients: [],
			patientId: '',
			patient: {},
			role: '',
			snap: null,
			online: false,
			updateTime: '',
			elapsedText: '--:--',
			fingerNames: cloud.FINGER_CN,
			fingerIcons: cloud.FINGER_ICON,
			fingerColors: ['#e53935', '#8e24aa', '#f9a825', '#43a047', '#1e88e5'],
			_timer: null,
			_elapsedTimer: null
		}
	},
	computed: {
		// 读不到 mode 时默认按「训练模式」处理
		modeVal() {
			const m = this.snap ? this.snap.mode : null
			return (m === 0 || m === 1 || m === 2) ? m : 1
		},
		// 目标值只在训练模式显示；正常模式没有目标值
		goals() {
			if (this.modeVal !== 1) return null
			return (this.snap && this.snap.goals) || null
		},
		modeText() { return cloud.modeWord(this.modeVal) },
		modeClass() {
			if (this.modeVal === 1) return 'train'
			if (this.modeVal === 2) return 'record'
			return 'normal'
		},
		resultText() {
			const r = this.snap && this.snap.result
			if (r === null || r === undefined) return '--'
			return cloud.RESULT_WORDS[Math.max(0, Math.min(3, Math.round(r)))]
		},
		resultColor() {
			const r = this.snap && this.snap.result
			if (r === null || r === undefined) return '#90a4ae'
			return cloud.RESULT_COLORS[Math.max(0, Math.min(3, Math.round(r)))]
		},
		errorText() {
			const e = this.snap && this.snap.error
			return (e === null || e === undefined) ? '--' : (e + '°')
		},
		lastTrainText() {
			// 上次训练时长只认云平台返回的 time；没有就 --
			return cloud.fmtSeconds(this.snap ? this.snap.time : null)
		}
	},
	onLoad(options) {
		this.role = this.readRole()
		this.patients = this.scopedPatients()
		let pid = (options && options.id) || ''
		if (this.role === 'patient') pid = (this.patients[0] && this.patients[0].id) || ''
		this.patientId = pid || (this.patients[0] && this.patients[0].id) || ''
		this.loadPatient()
		this.refresh(false)
		this._timer = setInterval(() => this.refresh(false), 5000)
		this._elapsedTimer = setInterval(() => this.tickElapsed(), 1000)
	},
	onShow() {
		this.role = this.readRole()
		this.patients = this.scopedPatients()
		this.loadPatient()
	},
	onUnload() {
		if (this._timer) { clearInterval(this._timer); this._timer = null }
		if (this._elapsedTimer) { clearInterval(this._elapsedTimer); this._elapsedTimer = null }
	},
	methods: {
		readRole() {
			try { return uni.getStorageSync('_kf_role') || '' } catch (e) { return '' }
		},
		// 患者登录只能看本人；医生登录可看全部
		scopedPatients() {
			const all = store.loadPatients()
			if (this.role !== 'patient') return all
			let acct = '', pid = ''
			try {
				acct = uni.getStorageSync('_kf_account') || ''
				pid = uni.getStorageSync('_kf_patient_id') || ''
			} catch (e) {}
			const p = (pid && store.getPatient(pid)) || store.findPatientByAccount(acct)
			return p ? [p] : []
		},
		selectPatient(id) {
			this.patientId = id
			this.snap = null
			this.loadPatient()
			this.refresh(true)
		},
		loadPatient() {
			const p = this.patientId ? store.getPatient(this.patientId) : null
			this.patient = p || {}
			if (p && p.lastSnap) this.snap = p.lastSnap
			this.online = !!(p && p.online)
			this.tickElapsed()
		},
		angleText(i) {
			const a = this.snap && this.snap.angles ? this.snap.angles[i] : null
			return (a === null || a === undefined) ? '--' : String(Math.round(a))
		},
		async refresh(manual) {
			const p = this.patient
			if (!p.id) return
			try {
				const snap = await cloud.queryDeviceProperty(p.productId, p.deviceName)
				this.snap = snap
				this.online = true
				store.recordSnapshot(p.id, snap, Date.now())
				store.pushRawSample(p.id, store.nowString(), snap, snap.raw)
				store.updatePatient(p.id, { online: true })
				this.loadPatient()
				this.updateTime = store.nowString().slice(11, 16)
				if (manual) uni.showToast({ title: '已刷新', icon: 'success' })
			} catch (e) {
				this.online = false
				store.updatePatient(p.id, { online: false })
				if (manual) uni.showToast({ title: '读取失败：' + (e.message || e.errMsg || '网络错误'), icon: 'none' })
			}
		},
		tickElapsed() {
			// 只有训练/录入模式才计时；正常模式不计时
			if (this.modeVal !== 1 && this.modeVal !== 2) { this.elapsedText = '--:--'; return }
			const sessions = (this.patient && this.patient.sessions) || []
			const last = sessions[sessions.length - 1]
			if (!last) { this.elapsedText = '--:--'; return }
			const end = last.endTs || last.startTs
			if (Date.now() - end > store.SESSION_GAP_MS) { this.elapsedText = '--:--'; return }
			this.elapsedText = store.fmtDuration(new Date() - last.startTs)
		},
		goDetail() {
			if (!this.patientId) { uni.showToast({ title: '请先选择患者', icon: 'none' }); return }
			uni.navigateTo({ url: '/pages/detail/detail?id=' + this.patientId })
		},
		goDuration() {
			if (!this.patientId) { uni.showToast({ title: '请先选择患者', icon: 'none' }); return }
			uni.navigateTo({ url: '/pages/duration/duration?id=' + this.patientId })
		},
		goRaw() {
			if (!this.patientId) { uni.showToast({ title: '请先选择患者', icon: 'none' }); return }
			uni.navigateTo({ url: '/pages/raw/raw?id=' + this.patientId })
		},
		goBack() { uni.reLaunch({ url: '/pages/index/index' }) }
	}
}
</script>

<style scoped>
.patient-page {
	min-height: 100vh;
	background: linear-gradient(180deg, #e8f6f4 0%, #f5f9ff 50%, #ffffff 100%);
	padding: 30rpx 36rpx 60rpx;
}
.page-header {
	position: relative; font-size: 44rpx; font-weight: 700; color: #00695c;
	text-align: center; margin-bottom: 24rpx;
}

.picker { white-space: nowrap; margin-bottom: 20rpx; }
.picker-inner { display: inline-flex; gap: 16rpx; padding: 4rpx 0; }
.chip {
	display: inline-flex; align-items: center; gap: 10rpx;
	font-size: 28rpx; color: #607d8b; background: #fff;
	padding: 12rpx 30rpx; border-radius: 32rpx;
	box-shadow: 0 2rpx 12rpx rgba(0,0,0,0.05);
}
.chip.active { background: #00897b; color: #fff; font-weight: 600; }
.chip-dot { width: 14rpx; height: 14rpx; border-radius: 50%; background: #cfd8dc; }
.chip-dot.on { background: #43a047; }

.status-bar {
	display: flex; align-items: center; justify-content: space-between;
	background: #fff; border-radius: 24rpx; padding: 24rpx 32rpx;
	box-shadow: 0 4rpx 18rpx rgba(0,0,0,0.05); margin-bottom: 20rpx;
}
.sb-title { display: block; font-size: 38rpx; font-weight: 800; color: #00695c; }
.sb-sub { display: block; font-size: 24rpx; color: #90a4ae; margin-top: 4rpx; }
.sb-right { display: flex; align-items: center; gap: 20rpx; }
.mode-badge { font-size: 28rpx; font-weight: 700; padding: 10rpx 26rpx; border-radius: 24rpx; }
.mode-badge.normal { background: #e3f2fd; color: #1565c0; }
.mode-badge.train { background: #fff3e0; color: #ef6c00; }
.mode-badge.record { background: #fce4ec; color: #c2185b; }
.signal { display: flex; align-items: center; gap: 8rpx; }
.signal-icon { font-size: 34rpx; }
.signal-text { font-size: 24rpx; color: #b0bec5; }
.signal.on .signal-text { color: #43a047; }

.timer-bar {
	display: flex; background: #fff; border-radius: 24rpx; padding: 24rpx 8rpx;
	box-shadow: 0 4rpx 18rpx rgba(0,0,0,0.05); margin-bottom: 32rpx;
}
.timer-item { flex: 1; display: flex; flex-direction: column; align-items: center; }
.timer-k { font-size: 22rpx; color: #90a4ae; }
.timer-v { font-size: 34rpx; font-weight: 700; color: #263238; margin-top: 8rpx; }

.section-title { font-size: 38rpx; font-weight: 700; color: #00897b; text-align: center; margin-bottom: 24rpx; }
.finger-flex { display: flex; flex-wrap: wrap; justify-content: center; gap: 24rpx; margin-bottom: 36rpx; }
.finger-card {
	flex: 0 0 calc((100% - 24rpx * 4) / 5); min-width: 0; box-sizing: border-box;
	background: #fff; border-radius: 28rpx; padding: 32rpx 12rpx 24rpx;
	box-shadow: 0 6rpx 22rpx rgba(0,0,0,0.06); border-top: 12rpx solid #ccc;
	display: flex; flex-direction: column; align-items: center;
}
.finger-icon { font-size: 48rpx; }
.finger-name { font-size: 28rpx; color: #78909c; margin-top: 8rpx; }
.finger-val { display: flex; align-items: baseline; margin-top: 8rpx; }
.finger-num { font-size: 76rpx; font-weight: 800; color: #263238; line-height: 1; }
.finger-unit { font-size: 30rpx; color: #90a4ae; margin-left: 4rpx; }
.finger-goal { margin-top: 10rpx; }
.goal-label { font-size: 22rpx; color: #ef6c00; background: #fff3e0; padding: 4rpx 16rpx; border-radius: 18rpx; }

.actions { display: flex; flex-wrap: wrap; gap: 24rpx; justify-content: center; margin-bottom: 24rpx; }
.act-btn {
	font-size: 30rpx; color: #00897b; padding: 18rpx 56rpx;
	border: 2rpx solid #00897b; border-radius: 44rpx; background: #fff;
}
.act-btn.primary { color: #fff; background: linear-gradient(135deg, #26a69a 0%, #00897b 100%); border-color: #00897b; }
.act-btn:active { opacity: 0.85; }

.footer { display: flex; flex-direction: column; align-items: center; padding: 24rpx 0 0; }
.update-time { font-size: 28rpx; color: #9e9e9e; margin-bottom: 12rpx; }
.back-btn { text-align: center; padding: 16rpx 0; font-size: 28rpx; color: #5c6bc0; }
</style>

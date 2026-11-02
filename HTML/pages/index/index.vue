<template>
	<view class="grasp-home">
		<!-- 顶部设备状态 -->
		<view class="hero">
			<view class="hero-glow glow-a"></view>
			<view class="hero-glow glow-b"></view>
			<view class="hero-head">
				<view class="brand">
					<view class="brand-mark">
						<text class="brand-glyph">✦</text>
					</view>
					<view class="brand-text">
						<text class="eyebrow">KV260 · BIONIC HAND</text>
						<text class="main-title">智能仿生手</text>
					</view>
				</view>
				<UserAvatar />
			</view>
			<text class="sub-title">视觉感知 · 视觉-动作映射 · 脑机接口预留</text>

			<view class="hero-status">
				<view class="status-pill" :class="{ online: online }">
					<view class="status-dot"></view>
					<text>{{ online ? '设备在线' : '设备离线' }}</text>
				</view>
				<text class="device-name">OneNET · {{ deviceName }}</text>
				<text class="update-time">{{ updateTime ? '更新 ' + updateTime : '等待云平台' }}</text>
			</view>
		</view>

		<!-- 当前识别物品 -->
		<view class="detect-card">
			<view class="object-thumb" :class="{ scanning: grasping }">
				<image class="object-img" :src="currentImage" mode="aspectFill"></image>
				<view class="object-shade"></view>
				<view class="scan-line"></view>
				<view class="thumb-badge">{{ objectBadge }}</view>
			</view>
			<view class="object-main">
				<view class="object-top">
					<text class="object-label">当前识别物品</text>
					<view class="confidence" v-if="currentConfidence">
						<text class="confidence-dot"></text>
						<text>{{ confidenceText }}</text>
					</view>
				</view>
				<text class="object-name">{{ currentName }}</text>
				<view class="object-tags">
					<text class="obj-tag">{{ currentCategory }}</text>
					<text class="obj-tag ghost" v-if="currentGraspType">{{ currentGraspType }}</text>
				</view>
				<text class="object-desc">{{ currentDesc }}</text>
				<view class="object-foot">
					<view class="grab-stat">
						<text class="grab-num">{{ currentCount }}</text>
						<text class="grab-unit">次抓取</text>
					</view>
					<view class="source">
						<view class="source-dot"></view>
						<text>{{ currentSource }}</text>
					</view>
				</view>
			</view>
		</view>

		<!-- 抓取操作 -->
		<view class="action-row">
			<view class="primary-action" :class="{ busy: grasping }" @click="simulateGrasp">
				<view class="action-icon"><text>🖐</text></view>
				<view class="action-copy">
					<text class="action-title">{{ grasping ? '动作复现中…' : '模拟语音抓取' }}</text>
					<text class="action-sub">调用已存舵机序列</text>
				</view>
				<text class="action-arrow">›</text>
			</view>
			<view class="save-action" :class="{ busy: saving }" @click="saveCurrent">
				<text class="save-action-icon">＋</text>
				<text class="save-action-title">{{ saving ? '存入中…' : '存入档案' }}</text>
				<text class="save-action-sub">保存当前五指参数</text>
			</view>
		</view>

		<!-- 五指舵机 -->
		<view class="servo-panel">
			<view class="panel-head">
				<view>
					<text class="panel-title">五指舵机</text>
					<text class="panel-sub">实时角度 / 0–180°</text>
				</view>
				<view class="live-tag" :class="{ on: online }">
					<view class="live-dot"></view>
					<text>{{ online ? 'LIVE' : 'CACHE' }}</text>
				</view>
			</view>
			<view class="servo-row">
				<view class="servo-card" v-for="(s, i) in fingerList" :key="i">
					<text class="servo-icon">{{ s.icon }}</text>
					<text class="servo-name">{{ s.name }}</text>
					<view class="servo-val">
						<text class="servo-num">{{ s.display }}</text>
						<text class="servo-deg" v-if="s.value !== null">°</text>
					</view>
					<view class="servo-bar">
						<view class="servo-fill" :style="{ width: s.pct + '%', background: s.color }"></view>
					</view>
					<text class="servo-key">{{ s.short }}</text>
				</view>
			</view>
			<view class="servo-foot">
				<text>数据来源：{{ online ? 'OneNET 实时属性' : '本地最近一次缓存' }}</text>
				<text v-if="!online" class="retry" @click="refresh(true)">重新连接 ↻</text>
			</view>
		</view>

		<!-- 功能入口 -->
		<view class="module-grid">
			<view class="module" @click="goArchive">
				<view class="module-icon-wrap blue"><text>🗂</text></view>
				<text class="module-title">抓取档案</text>
				<text class="module-sub">{{ archiveStats.recorded }} 已记录 · {{ archiveStats.pending }} 待识别</text>
			</view>
			<view class="module" @click="goRaw">
				<view class="module-icon-wrap green"><text>📡</text></view>
				<text class="module-title">原始数据</text>
				<text class="module-sub">云端采样与导出</text>
			</view>
		</view>

		<!-- 最近抓取 -->
		<view class="recent-panel" v-if="recent.length">
			<view class="panel-head">
				<view>
					<text class="panel-title">最近抓取</text>
					<text class="panel-sub">动作复现记录</text>
				</view>
				<text class="more" @click="goArchive">全部 ›</text>
			</view>
			<view class="recent-list">
				<view class="recent-item" v-for="r in recent" :key="r.id">
					<image class="recent-img" :src="r.image || defaultImage" mode="aspectFill"></image>
					<view class="recent-main">
						<text class="recent-name">{{ r.name || '未命名物品' }}</text>
						<text class="recent-meta">{{ r.graspType || '抓取动作' }} · {{ relative(r.ts) }}</text>
					</view>
					<view class="recent-servo">
						<text class="rs-label">舵机</text>
						<text class="rs-value">{{ servoLine(r.servo) }}</text>
					</view>
				</view>
			</view>
		</view>

		<view class="footer">
			<text class="footer-text">KV260 边缘计算 · OneNET 物联网 · 标准化脑机接口预留</text>
			<text class="logout" @click="logout">退出登录</text>
		</view>
	</view>
</template>

<script>
import UserAvatar from '@/components/UserAvatar.vue'
import cloud from '@/utils/cloud.js'
import archive from '@/utils/archive.js'
import vision from '@/utils/vision.js'
import store from '@/utils/store.js'

const FINGER_META = [
	{ name: '大拇指', short: '拇', icon: '👍', color: '#ef5350' },
	{ name: '食指', short: '食', icon: '☝️', color: '#ab47bc' },
	{ name: '中指', short: '中', icon: '✋', color: '#ffa726' },
	{ name: '无名指', short: '无名', icon: '💍', color: '#26a69a' },
	{ name: '小指', short: '小', icon: '🤙', color: '#42a5f5' }
]

export default {
	components: { UserAvatar },
	data() {
		return {
			online: false,
			updateTime: '',
			snap: null,
			current: null,
			grasping: false,
			saving: false,
			recent: [],
			archiveStats: { total: 0, recorded: 0, pending: 0, grabs: 0 },
			deviceName: cloud.DEFAULT_DEVICE_NAME,
			defaultImage: archive.DEFAULT_IMAGE,
			_timer: null,
			_autoTimer: null,
			_revertTimer: null,
			_loading: false
		}
	},
	computed: {
		servoValues() {
			const live = this.snap && this.snap.angles
			if (live && live.some(v => v !== null && v !== undefined)) return live
			if (this.current && this.current.servo) return this.current.servo
			return [null, null, null, null, null]
		},
		fingerList() {
			return FINGER_META.map((f, i) => {
				const raw = this.servoValues[i]
				const n = (raw === null || raw === undefined || isNaN(Number(raw))) ? null : Number(raw)
				return {
					...f,
					value: n,
					display: n === null ? '--' : String(Math.round(n)),
					pct: n === null ? 0 : Math.max(0, Math.min(100, n / 180 * 100))
				}
			})
		},
		currentName() {
			const cur = this.current
			// 本地存入/识别过的档案优先，避免被设备上报的物品名盖掉
			if (cur && cur.source !== 'cloud' && (cur.name || cur.status === 'unrecorded')) {
				return cur.name || '等待识别'
			}
			if (this.snap && this.snap.objectName) return this.snap.objectName
			if (cur && cur.name) return cur.name
			return '等待识别'
		},
		currentImage() {
			const cur = this.current
			if (cur && cur.source !== 'cloud' && cur.image) return cur.image
			if (this.snap && this.snap.objectName) {
				if (this.snap.objectImage) return this.snap.objectImage
				if (cur && cur.name === this.snap.objectName && cur.image) return cur.image
				return archive.DEFAULT_IMAGE
			}
			if (cur && cur.image) return cur.image
			return archive.DEFAULT_IMAGE
		},
		currentCategory() {
			if (this.current && this.current.category) return this.current.category
			return this.currentName === '等待识别' ? '未识别' : '未分类'
		},
		currentGraspType() {
			if (this.current && this.current.status === 'recorded') return this.current.graspType || ''
			return ''
		},
		currentDesc() {
			if (this.current && this.current.description) return this.current.description
			if (this.current && this.current.status === 'unrecorded') return '已保存五路舵机值，去抓取档案完成 AI 识别。'
			return '把物品放入识别区域，系统将自动匹配视觉-动作档案。'
		},
		currentCount() {
			if (this.snap && this.snap.graspCount !== null && this.snap.graspCount !== undefined) return Math.round(this.snap.graspCount)
			return (this.current && this.current.count) || 0
		},
		currentConfidence() {
			let c = 0
			if (this.snap && this.snap.objectName && this.snap.confidence !== null) c = this.snap.confidence
			else if (this.current) c = this.current.confidence || 0
			if (c > 1) c = c / 100
			return c
		},
		confidenceText() { return Math.round(this.currentConfidence * 100) + '%' },
		objectBadge() {
			if (!this.current || this.current.status !== 'recorded') return '待识别'
			if (!this.online) return '本地已记录'
			return this.current.source === 'cloud' ? '云端已记录' : '已记录'
		},
		currentSource() {
			const cur = this.current
			if (cur && cur.source !== 'cloud') return archive.sourceLabel(cur.source)
			if (this.online && this.snap && this.snap.objectName) return 'OneNET 云端识别'
			if (!this.online) {
				if (this.current && this.current.demo) return '演示档案 · 本地缓存'
				return this.current ? '本地缓存 · 最近一次识别' : '等待设备识别'
			}
			if (!this.current) return '等待设备识别'
			return archive.sourceLabel(this.current.source)
		}
	},
	onLoad() {
		this.reload()
		this.startPolling()
	},
	onShow() {
		this.reload()
		this.refresh(false)
		this.startPolling()
	},
	onHide() { this.stopPolling() },
	onUnload() {
		this.stopPolling()
		if (this._autoTimer) { clearTimeout(this._autoTimer); this._autoTimer = null }
		if (this._revertTimer) { clearTimeout(this._revertTimer); this._revertTimer = null }
	},
	methods: {
		startPolling() {
			this.stopPolling()
			this._timer = setInterval(() => this.refresh(false), 5000)
		},
		stopPolling() {
			if (this._timer) { clearInterval(this._timer); this._timer = null }
		},
		reload() {
			this.current = archive.getCurrentObject()
			this.recent = archive.recentActivity(3)
			this.archiveStats = archive.stats()
		},
		async refresh(manual) {
			if (this._loading) return
			this._loading = true
			try {
				const snap = await cloud.queryDeviceProperty()
				this.snap = snap
				this.online = true
				// 原始数据页与主界面共用设备级采样缓存
				store.pushRawSample(store.RAW_DEVICE_ID, store.nowString(), snap, snap.raw)
				const result = archive.ingestSnapshot(snap)
				this.current = result.current || archive.getCurrentObject()
				this.recent = archive.recentActivity(3)
				this.archiveStats = archive.stats()
				const d = new Date()
				const pad = n => String(n).padStart(2, '0')
				this.updateTime = `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
				if (manual) uni.showToast({ title: '云平台已连接', icon: 'success' })
			} catch (e) {
				this.online = false
				archive.markCloudOffline()
				this.reload()
				if (manual) {
					uni.showToast({ title: '读取失败：' + (e.message || e.errMsg || '网络错误'), icon: 'none', duration: 2600 })
				}
			}
			this._loading = false
		},
		simulateGrasp() {
			if (this.grasping) return
			const obj = archive.getCurrentObject()
			if (!obj || obj.status !== 'recorded') {
				uni.showToast({ title: '请先在档案中识别物品', icon: 'none' })
				return
			}
			this.grasping = true
			try { uni.vibrateShort({ type: 'light' }) } catch (e) {}
			setTimeout(() => {
				this.current = archive.incrementGrab(obj.id)
				this.recent = archive.recentActivity(3)
				this.archiveStats = archive.stats()
				this.grasping = false
				uni.showToast({ title: '已复现「' + obj.name + '」抓取', icon: 'none' })
			}, 650)
		},
		saveCurrent() {
			if (this.saving) return
			const values = this.servoValues
			if (!values || values.length < 5 || values.some(v => v === null || v === undefined || isNaN(Number(v)))) {
				uni.showToast({ title: '五指参数不完整', icon: 'none' })
				return
			}
			this.saving = true
			try {
				// 先存成「待识别」档案：只有五路舵机值 + 本地鼠标图片，
				// 名称留给抓取档案页的 AI 识别补充。
				const saved = archive.savePendingSnapshot({
					servo: values.map(v => Math.max(0, Math.min(180, Math.round(Number(v))))),
					image: archive.localImage('鼠标') || archive.DEFAULT_IMAGE,
					hint: 'mouse'
				})
				if (!saved) throw new Error('保存失败')
				this.current = saved
				this.reload()
				uni.showToast({ title: '请耐心等待识别', icon: 'none' })
				this.scheduleAutoRecognize(saved.id)
			} catch (e) {
				uni.showToast({ title: e.message || '存入失败', icon: 'none' })
			}
			this.saving = false
		},
		scheduleAutoRecognize(id) {
			if (this._autoTimer) clearTimeout(this._autoTimer)
			if (this._revertTimer) { clearTimeout(this._revertTimer); this._revertTimer = null }
			this._autoTimer = setTimeout(() => {
				this._autoTimer = null
				this.autoRecognize(id)
			}, 5000)
		},
		// 识别为鼠标 30 秒后切回苹果
		scheduleRevertToApple(id) {
			if (this._revertTimer) clearTimeout(this._revertTimer)
			this._revertTimer = setTimeout(() => {
				this._revertTimer = null
				this.revertToApple(id)
			}, 30000)
		},
		revertToApple(id) {
			const o = archive.getObject(id)
			if (!o) return
			const updated = archive.updateObject(id, {
				name: '苹果',
				category: '食物',
				image: archive.localImage('苹果') || o.image,
				graspKey: 'sphere',
				graspType: '球形抓握',
				description: '球形果实，整掌包覆，减小指尖压力。'
			})
			if (!updated) return
			this.current = updated
			this.reload()
			try { uni.$emit('grasp-archive-changed') } catch (e) {}
			uni.showToast({ title: '已切换为「苹果」', icon: 'none' })
		},
		async autoRecognize(id) {
			const o = archive.getObject(id)
			if (!o || o.status !== 'unrecorded') return
			try {
				const res = await vision.recognize(o.image, o.hint || 'mouse')
				const payload = Object.assign({}, res, { name: res.name || '鼠标' })
				const updated = archive.applyRecognition(id, payload, o.servo)
				if (!updated) return
				this.current = updated
				this.reload()
				try { uni.$emit('grasp-archive-changed') } catch (e) {}
				uni.showToast({ title: '已自动识别为「' + updated.name + '」', icon: 'none' })
				this.scheduleRevertToApple(id)
			} catch (e) {
				uni.showToast({ title: '自动识别失败：' + (e.message || '请检查识别设置'), icon: 'none', duration: 2600 })
			}
		},
		servoLine(arr) {
			if (!arr || !arr.length) return '—'
			return arr.map(v => (v === null || v === undefined) ? '--' : Math.round(v)).join(' / ')
		},
		relative(ts) { return archive.relativeTime(ts) },
		goArchive() { uni.navigateTo({ url: '/pages/archive/archive' }) },
		goRaw() { uni.navigateTo({ url: '/pages/raw/raw' }) },
		logout() {
			try {
				uni.removeStorageSync('_kf_login_time')
				uni.removeStorageSync('_kf_role')
				uni.removeStorageSync('_kf_account')
				uni.removeStorageSync('_kf_patient_id')
			} catch (e) {}
			uni.reLaunch({ url: '/pages/login/login' })
		}
	}
}
</script>

<style scoped>
.grasp-home {
	min-height: 100vh;
	background: #f2f6fb;
	padding: 0 32rpx 64rpx;
}

/* ===== 顶部 ===== */
.hero {
	position: relative;
	margin: 0 -32rpx;
	padding: 42rpx 32rpx 72rpx;
	background: linear-gradient(135deg, #081b33 0%, #0c2f52 52%, #0d3d57 100%);
	border-radius: 0 0 40rpx 40rpx;
	overflow: hidden;
}
.hero-glow {
	position: absolute; border-radius: 50%; filter: blur(40rpx); opacity: .55;
}
.glow-a { width: 320rpx; height: 320rpx; right: -100rpx; top: -120rpx; background: rgba(34, 211, 238, .35); }
.glow-b { width: 240rpx; height: 240rpx; left: -80rpx; bottom: -120rpx; background: rgba(52, 211, 153, .22); }
.hero-head { position: relative; display: flex; align-items: flex-start; justify-content: space-between; z-index: 2; }
.brand { display: flex; align-items: center; gap: 18rpx; }
.brand-mark {
	width: 70rpx; height: 70rpx; border-radius: 20rpx;
	background: linear-gradient(145deg, rgba(34,211,238,.28), rgba(52,211,153,.12));
	border: 1rpx solid rgba(125, 230, 255, .35);
	display: flex; align-items: center; justify-content: center;
	box-shadow: inset 0 0 24rpx rgba(34,211,238,.18);
}
.brand-glyph { color: #7de6ff; font-size: 36rpx; }
.brand-text { display: flex; flex-direction: column; }
.eyebrow { font-size: 18rpx; letter-spacing: 3rpx; color: rgba(125, 230, 255, .72); font-weight: 600; }
.main-title { font-size: 46rpx; font-weight: 800; color: #f7fbff; letter-spacing: 2rpx; margin-top: 2rpx; }
.sub-title { position: relative; display: block; font-size: 22rpx; color: rgba(200, 225, 245, .62); margin-top: 14rpx; letter-spacing: 1rpx; }
.hero-status {
	position: relative; display: flex; align-items: center; gap: 16rpx;
	margin-top: 26rpx; flex-wrap: wrap;
}
.status-pill {
	display: inline-flex; align-items: center; gap: 10rpx;
	padding: 8rpx 20rpx; border-radius: 24rpx;
	background: rgba(255,255,255,.08); border: 1rpx solid rgba(255,255,255,.12);
	font-size: 22rpx; color: #b0bec5;
}
.status-pill.online { color: #6bf0c2; background: rgba(52,211,153,.12); border-color: rgba(52,211,153,.26); }
.status-dot { width: 10rpx; height: 10rpx; border-radius: 50%; background: #78909c; }
.status-pill.online .status-dot { background: #34d399; box-shadow: 0 0 12rpx #34d399; }
.device-name { font-size: 21rpx; color: rgba(200,225,245,.55); font-family: monospace; }
.update-time { margin-left: auto; font-size: 21rpx; color: rgba(200,225,245,.45); }

/* ===== 当前物品 ===== */
.detect-card {
	position: relative; z-index: 3; margin-top: -46rpx;
	background: #fff; border-radius: 30rpx; padding: 22rpx;
	display: flex; gap: 22rpx;
	box-shadow: 0 20rpx 48rpx rgba(15, 40, 72, .13);
}
.object-thumb {
	position: relative; width: 206rpx; min-height: 206rpx; align-self: stretch; flex-shrink: 0;
	border-radius: 22rpx; overflow: hidden; background: #e8eef5;
}
.object-img { position: absolute; left: 0; top: 0; width: 100%; height: 100%; }
.object-shade { position: absolute; left: 0; right: 0; bottom: 0; height: 80rpx; background: linear-gradient(180deg, transparent, rgba(5, 20, 35, .42)); }
.scan-line {
	position: absolute; left: 8%; right: 8%; top: 18%; height: 3rpx;
	background: linear-gradient(90deg, transparent, #7de6ff, transparent);
	opacity: 0; box-shadow: 0 0 18rpx rgba(125,230,255,.9);
}
.object-thumb.scanning .scan-line { opacity: 1; animation: scan 1.2s ease-in-out infinite; }
@keyframes scan {
	0% { top: 12%; opacity: .2; }
	50% { top: 84%; opacity: 1; }
	100% { top: 12%; opacity: .2; }
}
.thumb-badge {
	position: absolute; left: 12rpx; bottom: 12rpx;
	font-size: 19rpx; color: #fff; padding: 5rpx 14rpx; border-radius: 18rpx;
	background: rgba(8, 27, 51, .58); border: 1rpx solid rgba(255,255,255,.18);
	backdrop-filter: blur(6px);
}
.object-main { flex: 1; min-width: 0; display: flex; flex-direction: column; }
.object-top { display: flex; align-items: center; justify-content: space-between; gap: 10rpx; }
.object-label { font-size: 21rpx; color: #8aa0b6; letter-spacing: 1rpx; }
.confidence { display: flex; align-items: center; gap: 7rpx; font-size: 19rpx; color: #0f9d80; background: #e8f8f2; padding: 4rpx 12rpx; border-radius: 14rpx; }
.confidence-dot { width: 8rpx; height: 8rpx; border-radius: 50%; background: #27c29e; }
.object-name {
	display: block; font-size: 42rpx; font-weight: 800; color: #10263f;
	margin-top: 4rpx; line-height: 1.25; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}
.object-tags { display: flex; gap: 10rpx; margin-top: 8rpx; flex-wrap: wrap; }
.obj-tag { font-size: 19rpx; color: #1d6ea8; background: #eaf4fc; padding: 4rpx 12rpx; border-radius: 12rpx; }
.obj-tag.ghost { color: #7a5bb5; background: #f1ecfb; }
.object-desc {
	font-size: 20rpx; color: #7a8fa5; line-height: 1.6; margin-top: 8rpx;
	overflow: hidden; display: -webkit-box; -webkit-line-clamp: 1; -webkit-box-orient: vertical;
}
.object-foot { margin-top: auto; padding-top: 10rpx; display: flex; align-items: flex-end; justify-content: space-between; gap: 8rpx; }
.grab-stat { display: flex; align-items: baseline; gap: 6rpx; }
.grab-num { font-size: 44rpx; font-weight: 800; color: #0b3a63; line-height: 1; }
.grab-unit { font-size: 19rpx; color: #8aa0b6; }
.source { display: flex; align-items: center; gap: 6rpx; font-size: 18rpx; color: #8aa0b6; }
.source-dot { width: 8rpx; height: 8rpx; border-radius: 50%; background: #34d399; }

/* ===== 操作 ===== */
.action-row { display: flex; gap: 18rpx; margin-top: 24rpx; }
.primary-action {
	flex: 1.55; display: flex; align-items: center; gap: 16rpx;
	padding: 22rpx 24rpx; border-radius: 24rpx;
	background: linear-gradient(135deg, #0d4d7a 0%, #087f8c 100%);
	box-shadow: 0 12rpx 28rpx rgba(8, 93, 120, .24);
}
.primary-action:active { opacity: .88; }
.primary-action.busy { animation: pulse 1s ease-in-out infinite; }
@keyframes pulse { 50% { opacity: .72; } }
.action-icon {
	width: 62rpx; height: 62rpx; border-radius: 18rpx; flex-shrink: 0;
	background: rgba(255,255,255,.14); display: flex; align-items: center; justify-content: center; font-size: 34rpx;
}
.action-copy { display: flex; flex-direction: column; min-width: 0; }
.action-title { font-size: 27rpx; font-weight: 700; color: #fff; }
.action-sub { font-size: 18rpx; color: rgba(220,245,255,.6); margin-top: 4rpx; }
.action-arrow { margin-left: auto; font-size: 40rpx; color: rgba(255,255,255,.5); }
.save-action {
	flex: 1; display: flex; flex-direction: column; justify-content: center;
	background: linear-gradient(135deg, #f2fbfd 0%, #ffffff 100%);
	border: 2rpx solid #cfe7f0; border-radius: 24rpx; padding: 18rpx 20rpx;
	box-shadow: 0 10rpx 26rpx rgba(15, 40, 72, .07);
}
.save-action:active { background: #eef8fb; }
.save-action.busy { opacity: .6; }
.save-action-icon { font-size: 30rpx; font-weight: 700; color: #0f7fa8; line-height: 1; }
.save-action-title { font-size: 25rpx; font-weight: 700; color: #0f5f7e; margin-top: 5rpx; white-space: nowrap; }
.save-action-sub { font-size: 17rpx; color: #7a94a9; margin-top: 4rpx; white-space: nowrap; }

/* ===== 舵机 ===== */
.servo-panel {
	background: #fff; border-radius: 28rpx; padding: 28rpx 22rpx 20rpx;
	margin-top: 24rpx; box-shadow: 0 12rpx 32rpx rgba(15, 40, 72, .07);
}
.panel-head { display: flex; align-items: flex-start; justify-content: space-between; margin-bottom: 20rpx; padding: 0 6rpx; }
.panel-title { display: block; font-size: 30rpx; font-weight: 800; color: #10263f; }
.panel-sub { display: block; font-size: 19rpx; color: #8aa0b6; margin-top: 3rpx; }
.live-tag {
	display: flex; align-items: center; gap: 7rpx; font-size: 18rpx; font-weight: 700;
	color: #90a4ae; background: #f1f5f9; padding: 6rpx 14rpx; border-radius: 16rpx; letter-spacing: 1rpx;
}
.live-tag.on { color: #0f9d80; background: #e8f8f2; }
.live-dot { width: 8rpx; height: 8rpx; border-radius: 50%; background: #b0bec5; }
.live-tag.on .live-dot { background: #27c29e; box-shadow: 0 0 10rpx #27c29e; }
.servo-row { display: flex; gap: 12rpx; }
.servo-card {
	flex: 1; min-width: 0; background: #f7fafd; border-radius: 20rpx;
	padding: 18rpx 6rpx 14rpx; display: flex; flex-direction: column; align-items: center;
	border: 1rpx solid #edf3f9;
}
.servo-icon { font-size: 30rpx; line-height: 1; }
.servo-name { font-size: 18rpx; color: #7a8fa5; margin-top: 7rpx; white-space: nowrap; }
.servo-val { display: flex; align-items: baseline; margin-top: 4rpx; }
.servo-num { font-size: 40rpx; font-weight: 800; color: #16324e; line-height: 1; letter-spacing: -1rpx; }
.servo-deg { font-size: 18rpx; color: #9aaec0; margin-left: 1rpx; }
.servo-bar { width: 80%; height: 6rpx; background: #e4ebf3; border-radius: 4rpx; margin-top: 10rpx; overflow: hidden; }
.servo-fill { height: 100%; border-radius: 4rpx; transition: width .5s ease; }
.servo-key { font-size: 15rpx; color: #b0bfcd; margin-top: 8rpx; letter-spacing: 1rpx; }
.servo-foot {
	display: flex; align-items: center; justify-content: space-between;
	margin-top: 18rpx; padding: 14rpx 6rpx 0; border-top: 1rpx solid #f0f4f8;
	font-size: 18rpx; color: #9aaec0;
}
.retry { color: #0f7fa8; }

/* ===== 功能入口 ===== */
.module-grid { display: flex; gap: 16rpx; margin-top: 24rpx; }
.module {
	flex: 1; min-width: 0; background: #fff; border-radius: 24rpx; padding: 22rpx 14rpx 20rpx;
	display: flex; flex-direction: column; align-items: flex-start;
	box-shadow: 0 10rpx 26rpx rgba(15, 40, 72, .06);
}
.module:active { background: #f7fafd; }
.module-icon-wrap {
	width: 54rpx; height: 54rpx; border-radius: 16rpx; display: flex; align-items: center; justify-content: center; font-size: 28rpx;
}
.module-icon-wrap.blue { background: #eaf4fc; }
.module-icon-wrap.green { background: #e8f8f2; }
.module-icon-wrap.orange { background: #fff3e6; }
.module-title { font-size: 24rpx; font-weight: 700; color: #16324e; margin-top: 12rpx; white-space: nowrap; }
.module-sub { font-size: 17rpx; color: #8aa0b6; margin-top: 4rpx; white-space: nowrap; }

/* ===== 最近抓取 ===== */
.recent-panel {
	background: #fff; border-radius: 28rpx; padding: 28rpx 24rpx 10rpx;
	margin-top: 24rpx; box-shadow: 0 12rpx 32rpx rgba(15, 40, 72, .07);
}
.more { font-size: 21rpx; color: #0f7fa8; }
.recent-item { display: flex; align-items: center; gap: 18rpx; padding: 18rpx 0; border-top: 1rpx solid #f2f6fa; }
.recent-img { width: 74rpx; height: 74rpx; border-radius: 16rpx; background: #eef3f8; flex-shrink: 0; }
.recent-main { flex: 1; min-width: 0; display: flex; flex-direction: column; }
.recent-name { font-size: 25rpx; font-weight: 700; color: #16324e; }
.recent-meta { font-size: 18rpx; color: #8aa0b6; margin-top: 5rpx; }
.recent-servo { display: flex; flex-direction: column; align-items: flex-end; gap: 4rpx; }
.rs-label { font-size: 16rpx; color: #b0bfcd; }
.rs-value { font-size: 18rpx; color: #4a6b88; font-family: monospace; }

/* ===== 底部 ===== */
.footer { display: flex; flex-direction: column; align-items: center; padding: 46rpx 0 10rpx; gap: 16rpx; }
.footer-text { font-size: 18rpx; color: #a8b8c8; letter-spacing: 1rpx; text-align: center; }
.logout { font-size: 21rpx; color: #7a8fa5; padding: 8rpx 20rpx; }
</style>

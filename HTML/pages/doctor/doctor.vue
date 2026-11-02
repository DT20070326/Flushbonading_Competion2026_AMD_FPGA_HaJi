<template>
	<view class="doctor-page">
		<view class="page-header">
			<UserAvatar />👨‍⚕️ 医生端 · 患者管理
		</view>

		<!-- 工具条 -->
		<view class="toolbar">
			<view class="tool-info">
				<text class="tool-count">共 {{ patients.length }} 位患者</text>
				<text class="tool-sync" v-if="syncTime"> · 同步于 {{ syncTime }}</text>
			</view>
			<view class="tool-btns">
				<view class="tool-btn sync" :class="{ busy: syncing }" @click="syncAll">
					{{ syncing ? '同步中…' : '⟳ 同步云平台' }}
				</view>
				<view class="tool-btn add" @click="openAdd">＋ 添加患者</view>
			</view>
		</view>

		<!-- 患者方框网格 -->
		<view class="patient-grid" v-if="patients.length > 0">
			<view class="patient-box" v-for="p in patients" :key="p.id"
				:style="{ borderTopColor: p.online ? '#1565c0' : '#b0bec5' }">
				<view class="pb-top">
					<view class="pb-status" :class="{ on: p.online }"></view>
					<text class="pb-name">{{ p.name }}</text>
					<text class="pb-del" @click.stop="delPatient(p)">✕</text>
				</view>
				<view class="pb-avatar">
					<text class="pb-emoji">{{ avatarOf(p) }}</text>
					<text class="pb-hand">{{ p.hand }}</text>
				</view>
				<view class="pb-meta">
					<text class="pb-meta-line">{{ p.gender }} · {{ p.age }}岁</text>
					<text class="pb-meta-line note">{{ p.note || '—' }}</text>
					<text class="pb-meta-line account" v-if="p.account">账号 {{ p.account }}</text>
				</view>
				<view class="pb-stat">
					<view class="pb-stat-item">
						<text class="pb-stat-num">{{ statsOf(p).count }}</text>
						<text class="pb-stat-label">训练场次</text>
					</view>
					<view class="pb-stat-item">
						<text class="pb-stat-num">{{ statsOf(p).lastDur }}</text>
						<text class="pb-stat-label">最近时长</text>
					</view>
				</view>

				<!-- 直接放卡片下面 -->
				<view class="pb-btns">
					<text class="pb-btn records" @click.stop="goRecords(p)">📋 训练记录</text>
					<text class="pb-btn duration" @click.stop="goDuration(p)">📈 训练分析</text>
				</view>
			</view>
		</view>

		<view class="empty" v-else>
			<text class="empty-icon">🧑🦽</text>
			<text class="empty-text">暂无患者，点击「添加患者」录入云平台设备</text>
		</view>

		<view class="back-btn" @click="goBack">← 返回首页</view>

		<!-- 添加患者弹窗 -->
		<view class="modal-overlay" v-if="showAdd" @click="showAdd = false">
			<view class="modal-card" @click.stop>
				<text class="modal-title">添加患者</text>
				<text class="modal-tip">绑定该患者使用的 OneNET 云平台设备</text>

				<view class="form-item">
					<text class="form-label">姓名</text>
					<input class="form-input" v-model="form.name" placeholder="请输入患者姓名" />
				</view>

				<view class="form-item">
					<text class="form-label">登录账号（患者端登录用，可留空）</text>
					<input class="form-input" v-model="form.account" placeholder="如 Patient" />
				</view>

				<view class="form-item">
					<text class="form-label">性别</text>
					<view class="seg">
						<text class="seg-item" v-for="g in ['男','女']" :key="g"
							:class="{ active: form.gender === g }" @click="form.gender = g">{{ g }}</text>
					</view>
				</view>

				<view class="form-item">
					<text class="form-label">年龄</text>
					<input class="form-input" v-model="form.age" type="number" placeholder="请输入年龄" />
				</view>

				<view class="form-item">
					<text class="form-label">患侧手</text>
					<view class="seg">
						<text class="seg-item" v-for="h in ['左手','右手','双手']" :key="h"
							:class="{ active: form.hand === h }" @click="form.hand = h">{{ h }}</text>
					</view>
				</view>

				<view class="form-item">
					<text class="form-label">病情备注</text>
					<input class="form-input" v-model="form.note" placeholder="如：脑卒中后遗症 · 康复中期" />
				</view>

				<view class="form-divider"><text>云平台设备（OneNET）</text></view>

				<view class="form-item">
					<text class="form-label">产品ID</text>
					<input class="form-input" v-model="form.productId" placeholder="product_id" />
				</view>
				<view class="form-item">
					<text class="form-label">设备名</text>
					<input class="form-input" v-model="form.deviceName" placeholder="device_name" />
				</view>

				<view class="modal-btns">
					<view class="modal-btn cancel" @click="showAdd = false">取消</view>
					<view class="modal-btn ok" @click="submitAdd">保存并绑定</view>
				</view>
			</view>
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
			showAdd: false,
			syncing: false,
			syncTime: '',
			form: this.blankForm()
		}
	},
	onShow() { this.load() },
	methods: {
		blankForm() {
			return {
				name: '', account: '', gender: '男', age: '', hand: '右手', note: '',
				productId: cloud.DEFAULT_PRODUCT_ID, deviceName: cloud.DEFAULT_DEVICE_NAME
			}
		},
		load() { this.patients = store.loadPatients() },
		avatarOf(p) {
			if (p.gender === '女') return p.age >= 60 ? '👵' : '👩'
			return p.age >= 60 ? '👴' : '👨'
		},
		statsOf(p) {
			const sessions = p.sessions || []
			const last = sessions[sessions.length - 1]
			return {
				count: sessions.length,
				lastDur: last ? store.fmtDuration(store.sessionStats(last).durationMs) : '--:--'
			}
		},
		goRecords(p) { uni.navigateTo({ url: '/pages/detail/detail?id=' + p.id }) },
		goDuration(p) { uni.navigateTo({ url: '/pages/duration/duration?id=' + p.id }) },
		delPatient(p) {
			uni.showModal({
				title: '确认删除',
				content: `确定删除患者「${p.name}」及其训练记录吗？`,
				success: (res) => {
					if (res.confirm) {
						store.removePatient(p.id)
						store.clearSamples(p.id)
						this.load()
						uni.showToast({ title: '已删除', icon: 'success' })
					}
				}
			})
		},
		openAdd() { this.form = this.blankForm(); this.showAdd = true },
		submitAdd() {
			const f = this.form
			if (!f.name.trim()) { uni.showToast({ title: '请输入姓名', icon: 'none' }); return }
			const acc = f.account.trim()
			if (acc) {
				if (/^doctor$/i.test(acc)) { uni.showToast({ title: '该账号为医生端保留', icon: 'none' }); return }
				const dup = store.loadPatients().find(p => String(p.account || '').trim().toLowerCase() === acc.toLowerCase())
				if (dup) { uni.showToast({ title: '账号已被「' + dup.name + '」占用', icon: 'none' }); return }
			}
			if (!f.productId.trim()) { uni.showToast({ title: '请输入产品ID', icon: 'none' }); return }
			if (!f.deviceName.trim()) { uni.showToast({ title: '请输入设备名', icon: 'none' }); return }
			const p = {
				id: store.newPatientId(),
				name: f.name.trim(),
				account: f.account.trim(),
				gender: f.gender,
				age: Number(f.age) || 0,
				hand: f.hand,
				note: f.note.trim(),
				productId: f.productId.trim(),
				deviceName: f.deviceName.trim(),
				online: false,
				createdAt: Date.now(),
				lastSyncTs: null,
				sessions: []
			}
			store.addPatient(p)
			this.showAdd = false
			this.load()
			uni.showToast({ title: '已添加，正在校验设备…', icon: 'none' })
			this.syncOne(p.id)
		},
		async syncOne(id) {
			const p = store.getPatient(id)
			if (!p) return
			try {
				const snap = await cloud.queryDeviceProperty(p.productId, p.deviceName)
				const ts = Date.now()
				store.recordSnapshot(id, snap, ts)
				store.pushRawSample(id, store.nowString(), snap, snap.raw)
				store.updatePatient(id, { online: true })
			} catch (e) {
				store.updatePatient(id, { online: false })
			}
			this.load()
		},
		async syncAll() {
			if (this.syncing) return
			this.syncing = true
			for (const p of store.loadPatients()) {
				try {
					const snap = await cloud.queryDeviceProperty(p.productId, p.deviceName)
					store.recordSnapshot(p.id, snap, Date.now())
					store.pushRawSample(p.id, store.nowString(), snap, snap.raw)
					store.updatePatient(p.id, { online: true })
				} catch (e) {
					store.updatePatient(p.id, { online: false })
				}
			}
			this.load()
			this.syncing = false
			this.syncTime = store.nowString().slice(11, 16)
			uni.showToast({ title: '同步完成', icon: 'success' })
		},
		goBack() { uni.reLaunch({ url: '/pages/index/index' }) }
	}
}
</script>

<style scoped>
.doctor-page {
	min-height: 100vh;
	background: linear-gradient(180deg, #e3f2fd 0%, #f5f9ff 50%, #ffffff 100%);
	padding: 30rpx 36rpx 60rpx;
}
.page-header {
	position: relative; font-size: 44rpx; font-weight: 700; color: #0d47a1;
	text-align: center; margin-bottom: 28rpx;
}

.toolbar {
	display: flex; align-items: center; justify-content: space-between;
	margin-bottom: 28rpx;
}
.tool-info { font-size: 26rpx; color: #78909c; }
.tool-count { font-weight: 700; color: #1565c0; }
.tool-btns { display: flex; gap: 16rpx; }
.tool-btn {
	font-size: 26rpx; padding: 12rpx 28rpx; border-radius: 32rpx; color: #fff;
}
.tool-btn.sync { background: linear-gradient(135deg, #42a5f5 0%, #1e88e5 100%); }
.tool-btn.sync.busy { opacity: 0.6; }
.tool-btn.add { background: linear-gradient(135deg, #26a69a 0%, #00897b 100%); }
.tool-btn:active { opacity: 0.85; }

.patient-grid {
	display: flex; flex-wrap: wrap; gap: 28rpx; justify-content: flex-start;
}
.patient-box {
	flex: 0 0 calc((100% - 28rpx * 3) / 4);
	min-width: 0; box-sizing: border-box;
	background: #fff; border-radius: 28rpx; padding: 24rpx 24rpx 20rpx;
	box-shadow: 0 6rpx 22rpx rgba(0,0,0,0.07); border-top: 14rpx solid #b0bec5;
}
.pb-top { display: flex; align-items: center; gap: 10rpx; }
.pb-status { width: 16rpx; height: 16rpx; border-radius: 50%; background: #bdbdbd; flex-shrink: 0; }
.pb-status.on { background: #43a047; box-shadow: 0 0 10rpx rgba(67,160,71,0.6); }
.pb-name { flex: 1; font-size: 34rpx; font-weight: 700; color: #263238; white-space: nowrap; overflow: hidden; }
.pb-del { font-size: 26rpx; color: #cfd8dc; padding: 4rpx 8rpx; flex-shrink: 0; }
.pb-del:active { color: #e53935; }

.pb-avatar {
	display: flex; flex-direction: column; align-items: center;
	margin: 20rpx 0 12rpx;
}
.pb-emoji { font-size: 96rpx; line-height: 1; }
.pb-hand {
	font-size: 22rpx; color: #1565c0; background: #e3f2fd;
	padding: 2rpx 16rpx; border-radius: 20rpx; margin-top: 10rpx;
}

.pb-meta { display: flex; flex-direction: column; gap: 4rpx; margin-bottom: 16rpx; }
.pb-meta-line { font-size: 24rpx; color: #546e7a; }
.pb-meta-line.note { color: #90a4ae; white-space: nowrap; overflow: hidden; }
.pb-meta-line.account { color: #1565c0; font-weight: 600; }

.pb-stat {
	display: flex; border-top: 1rpx solid #f0f0f0; padding-top: 14rpx;
}
.pb-stat-item { flex: 1; display: flex; flex-direction: column; align-items: center; }
.pb-stat-num { font-size: 32rpx; font-weight: 700; color: #1565c0; }
.pb-stat-label { font-size: 20rpx; color: #b0bec5; margin-top: 2rpx; }

.pb-btns { display: flex; gap: 12rpx; margin-top: 16rpx; }
.pb-btn { flex: 1; text-align: center; font-size: 22rpx; padding: 12rpx 0; border-radius: 12rpx; white-space: nowrap; }
.pb-btn.records { background: #e3f2fd; color: #1565c0; }
.pb-btn.duration { background: #e0f2f1; color: #00897b; }
.pb-btn:active { opacity: 0.7; }

.empty { display: flex; flex-direction: column; align-items: center; padding: 160rpx 0; }
.empty-icon { font-size: 120rpx; margin-bottom: 28rpx; }
.empty-text { font-size: 28rpx; color: #9e9e9e; }

.back-btn { text-align: center; padding: 48rpx 0 0; font-size: 28rpx; color: #5c6bc0; }

/* ===== 弹窗 ===== */
.modal-overlay {
	position: fixed; top: 0; left: 0; right: 0; bottom: 0;
	background: rgba(0,0,0,0.5); z-index: 999;
	display: flex; align-items: center; justify-content: center; padding: 40rpx;
}
.modal-card {
	width: 100%; max-width: 660rpx; max-height: 86vh; overflow-y: auto;
	background: #fff; border-radius: 32rpx; padding: 48rpx 44rpx 36rpx;
	display: flex; flex-direction: column;
}
.modal-title { font-size: 40rpx; font-weight: 800; color: #0d47a1; text-align: center; }
.modal-tip { font-size: 24rpx; color: #90a4ae; text-align: center; margin: 8rpx 0 28rpx; }
.form-item { margin-bottom: 22rpx; }
.form-label { font-size: 26rpx; color: #546e7a; margin-bottom: 10rpx; display: block; font-weight: 600; }
.form-input {
	width: 100%; height: 76rpx; border: 2rpx solid #e0e0e0; border-radius: 14rpx;
	padding: 0 22rpx; font-size: 28rpx; box-sizing: border-box;
}
.seg { display: flex; gap: 16rpx; }
.seg-item {
	flex: 1; text-align: center; padding: 16rpx 0; font-size: 28rpx;
	color: #78909c; background: #f5f5f5; border-radius: 14rpx;
}
.seg-item.active { background: #1565c0; color: #fff; font-weight: 600; }
.form-divider {
	text-align: center; font-size: 24rpx; color: #90a4ae;
	margin: 20rpx 0 20rpx; border-top: 1rpx dashed #e0e0e0; padding-top: 22rpx;
}
.modal-btns { display: flex; gap: 24rpx; margin-top: 16rpx; }
.modal-btn { flex: 1; text-align: center; padding: 22rpx 0; border-radius: 16rpx; font-size: 30rpx; }
.modal-btn.cancel { background: #f5f5f5; color: #78909c; }
.modal-btn.ok { background: linear-gradient(135deg, #42a5f5 0%, #1565c0 100%); color: #fff; font-weight: 600; }
</style>

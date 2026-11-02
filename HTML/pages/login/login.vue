<template>
	<view class="login-page">
		<view class="login-card">
			<view class="login-mark">✦</view>
			<view class="login-title">智能仿生手系统</view>
			<view class="login-subtitle">KV260 边缘计算 · OneNET 云平台</view>
			<view class="login-tags">
				<text>视觉抓取</text>
				<text>五指舵机</text>
				<text>原始数据</text>
			</view>

			<view class="login-form">
				<view class="input-row">
					<text class="input-label">账号</text>
					<input class="input-field" v-model="username" placeholder="请输入操作员账号" @confirm="doLogin" />
				</view>
				<view class="input-row">
					<text class="input-label">密码</text>
					<input class="input-field" v-model="password" type="password" placeholder="请输入密码" @confirm="doLogin" />
				</view>
				<view class="login-btn" @click="doLogin">进 入 系 统</view>
			</view>

			<text class="login-hint">演示账号 HaJi</text>
			<text class="login-error" v-if="error">{{ error }}</text>
		</view>
	</view>
</template>

<script>
const PASSWORD = '666'
const ACCOUNT = 'HaJi'

export default {
	data() {
		return { username: '', password: '', error: '' }
	},
	onLoad() {
		try {
			const t = parseInt(uni.getStorageSync('_kf_login_time'))
			const acc = uni.getStorageSync('_kf_account')
			if (t && acc && Date.now() - t < 10 * 60 * 1000) {
				setTimeout(() => { uni.reLaunch({ url: '/pages/index/index' }) }, 100)
			}
		} catch (e) {}
	},
	methods: {
		doLogin() {
			this.error = ''
			const acc = (this.username || '').trim()
			if (!acc) { this.error = '请输入账号'; return }
			if (!this.password) { this.error = '请输入密码'; return }
			if (this.password !== PASSWORD) { this.error = '密码错误'; return }
			if (acc.toLowerCase() !== ACCOUNT.toLowerCase()) { this.error = '账号错误，请使用演示账号 HaJi'; return }
			this.finishLogin(acc)
		},
		finishLogin(account) {
			try {
				uni.setStorageSync('_kf_login_time', String(Date.now()))
				uni.setStorageSync('_kf_account', account)
				uni.removeStorageSync('_kf_role')
				uni.removeStorageSync('_kf_patient_id')
			} catch (e) {}
			uni.reLaunch({ url: '/pages/index/index' })
		}
	}
}
</script>

<style scoped>
.login-page {
	position: relative; min-height: 100vh;
	display: flex; align-items: center; justify-content: center;
	background: linear-gradient(160deg, #e8f2fb 0%, #eef7f6 50%, #f6fbff 100%);
	padding: 48rpx;
}
.login-card {
	width: 100%; max-width: 600rpx; background: #fff; border-radius: 34rpx;
	padding: 56rpx 52rpx 44rpx; box-shadow: 0 18rpx 54rpx rgba(15, 40, 72, 0.12);
	display: flex; flex-direction: column; align-items: center;
}
.login-mark {
	width: 88rpx; height: 88rpx; border-radius: 26rpx;
	background: linear-gradient(145deg, #0f7fa8 0%, #087f8c 100%);
	color: #fff; font-size: 44rpx;
	display: flex; align-items: center; justify-content: center;
	box-shadow: 0 10rpx 26rpx rgba(8, 127, 140, 0.28);
	margin-bottom: 24rpx;
}
.login-title { font-size: 48rpx; font-weight: 800; color: #10263f; margin-bottom: 10rpx; letter-spacing: 2rpx; }
.login-subtitle { font-size: 24rpx; color: #7a8fa5; }
.login-tags { display: flex; gap: 14rpx; margin-top: 22rpx; margin-bottom: 38rpx; }
.login-tags text {
	font-size: 20rpx; color: #0f7fa8; background: #eaf4fc;
	padding: 6rpx 18rpx; border-radius: 16rpx;
}

.login-form { width: 100%; }
.input-row { display: flex; align-items: center; margin-bottom: 30rpx; border-bottom: 2rpx solid #e4edf4; padding-bottom: 16rpx; }
.input-label { font-size: 28rpx; color: #36536f; width: 80rpx; flex-shrink: 0; font-weight: 600; }
.input-field { flex: 1; font-size: 28rpx; height: 60rpx; color: #10263f; }
.login-btn {
	width: 100%; text-align: center;
	background: linear-gradient(135deg, #0d4d7a 0%, #087f8c 100%);
	color: #fff; font-size: 32rpx; font-weight: 700; padding: 22rpx 0;
	border-radius: 46rpx; margin-top: 34rpx; letter-spacing: 6rpx;
	box-shadow: 0 10rpx 24rpx rgba(8, 93, 120, 0.22);
}
.login-btn:active { opacity: 0.86; }
.login-hint { font-size: 21rpx; color: #9aaec0; margin-top: 24rpx; }
.login-error { font-size: 24rpx; color: #c62828; margin-top: 14rpx; }
</style>

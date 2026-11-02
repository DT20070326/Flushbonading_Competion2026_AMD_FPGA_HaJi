<template>
	<view class="ua-root">
		<view class="ua-badge" @click="openPopup">
			<image class="ua-avatar" src="/static/cat.png" mode="aspectFill"></image>
			<text class="ua-name">{{ account || '未登录' }}</text>
		</view>

		<view class="ua-overlay" v-if="showPopup" @click="showPopup = false">
			<view class="ua-card" @click.stop>
				<image class="ua-card-avatar" src="/static/cat.png" mode="aspectFill"></image>
				<text class="ua-card-name">{{ account || '未登录' }}</text>
				<text class="ua-card-role">设备操作员</text>

				<view class="ua-info">
					<view class="ua-info-row"><text class="ua-info-k">云平台</text><text class="ua-info-v">OneNET 物联网</text></view>
					<view class="ua-info-row"><text class="ua-info-k">产品ID</text><text class="ua-info-v">{{ productId }}</text></view>
					<view class="ua-info-row"><text class="ua-info-k">采集设备</text><text class="ua-info-v">KV260 仿生手</text></view>
					<view class="ua-info-row"><text class="ua-info-k">抓取档案</text><text class="ua-info-v">{{ archiveCount }} 项</text></view>
				</view>

				<view class="ua-btns">
					<view class="ua-btn logout" @click="logout">退出登录</view>
					<view class="ua-btn close" @click="showPopup = false">关闭</view>
				</view>
			</view>
		</view>
	</view>
</template>

<script>
import cloud from '@/utils/cloud.js'
import archive from '@/utils/archive.js'

export default {
	name: 'UserAvatar',
	data() {
		return {
			showPopup: false,
			account: '',
			productId: cloud.DEFAULT_PRODUCT_ID,
			archiveCount: 0
		}
	},
	mounted() { this.loadAccount() },
	methods: {
		loadAccount() {
			try { this.account = uni.getStorageSync('_kf_account') || '' }
			catch (e) { this.account = '' }
		},
		openPopup() {
			this.loadAccount()
			this.archiveCount = archive.stats().total
			this.showPopup = true
		},
		logout() {
			this.showPopup = false
			uni.showModal({
				title: '退出登录',
				content: '确定退出当前账号吗？',
				success: (res) => {
					if (!res.confirm) return
					try {
						uni.removeStorageSync('_kf_login_time')
						uni.removeStorageSync('_kf_role')
						uni.removeStorageSync('_kf_account')
						uni.removeStorageSync('_kf_patient_id')
					} catch (e) {}
					uni.reLaunch({ url: '/pages/login/login' })
				}
			})
		}
	}
}
</script>

<style scoped>
.ua-root { position: absolute; right: 0; top: -8rpx; z-index: 10; }
.ua-badge {
	display: flex; align-items: center; gap: 12rpx;
	padding: 6rpx 20rpx 6rpx 8rpx;
	background: rgba(255,255,255,0.9);
	border-radius: 40rpx;
	box-shadow: 0 4rpx 16rpx rgba(0,0,0,0.1);
}
.ua-avatar { width: 56rpx; height: 56rpx; border-radius: 50%; border: 3rpx solid #1565c0; flex-shrink: 0; overflow: hidden; }
.ua-name { font-size: 26rpx; color: #1565c0; font-weight: 600; white-space: nowrap; }

.ua-overlay {
	position: fixed; top: 0; left: 0; right: 0; bottom: 0;
	background: rgba(0,0,0,0.45); z-index: 1000;
	display: flex; align-items: center; justify-content: center;
}
.ua-card {
	width: 620rpx; max-height: 85vh; background: #fff; border-radius: 32rpx;
	padding: 56rpx 48rpx 40rpx; display: flex; flex-direction: column; align-items: center;
	box-shadow: 0 20rpx 60rpx rgba(0,0,0,0.2); overflow-y: auto;
}
.ua-card-avatar { width: 100rpx; height: 100rpx; border-radius: 50%; border: 4rpx solid #1565c0; margin-bottom: 16rpx; overflow: hidden; }
.ua-card-name { font-size: 40rpx; font-weight: 700; color: #0d47a1; }
.ua-card-role { font-size: 26rpx; color: #78909c; margin-top: 8rpx; padding: 6rpx 24rpx; background: #e3f2fd; border-radius: 20rpx; }
.ua-info { width: 100%; margin-top: 36rpx; padding-top: 20rpx; border-top: 1rpx solid #e0e0e0; }
.ua-info-row { display: flex; justify-content: space-between; padding: 16rpx 4rpx; }
.ua-info-k { font-size: 26rpx; color: #90a4ae; }
.ua-info-v { font-size: 26rpx; color: #263238; font-weight: 600; }

.ua-btns { display: flex; gap: 24rpx; width: 100%; margin-top: 36rpx; }
.ua-btn { flex: 1; text-align: center; padding: 22rpx 0; border-radius: 16rpx; font-size: 30rpx; }
.ua-btn.logout { background: #ffebee; color: #c62828; font-weight: 600; }
.ua-btn.close { background: #f5f5f5; color: #78909c; }
.ua-btn:active { opacity: 0.8; }
</style>

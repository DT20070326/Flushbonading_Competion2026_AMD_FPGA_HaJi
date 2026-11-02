<template>
	<view class="raw-page">
		<view class="page-header">
			<UserAvatar />📡 原始数据
		</view>
		<text class="device-sub">{{ deviceName }} · OneNET 设备级采样</text>

		<!-- 设备状态 + 存入 -->
		<view class="status-line">
			<view class="status-left">
				<view class="status-dot" :class="{ on: online }"></view>
				<text class="status-text">{{ online ? '设备在线' : '未连接' }}</text>
				<text class="status-hint">{{ updateTime ? ('· 最近存入 ' + updateTime) : '· 点击右侧按钮读取云端当前数据' }}</text>
			</view>
			<view class="save-sample" :class="{ busy: saving }" @click="sampleNow">
				{{ saving ? '存入中…' : '存入' }}
			</view>
		</view>

		<!-- 时间范围 -->
		<view class="time-range" v-if="samples.length > 0">
			<view class="range-item">📥 最新采样：<text class="range-val">{{ latestTime }}</text></view>
			<view class="range-item">📅 最早采样：<text class="range-val">{{ earliestTime }}</text></view>
		</view>

		<!-- 最后一次原始数据 -->
		<view class="raw-block" v-if="rawStr">
			<view class="raw-title-row">
				<text class="raw-title">📡 最后一次云平台返回原始数据</text>
				<view class="copy-btn" @click="copyRaw">📋 复制</view>
			</view>
			<scroll-view scroll-y class="raw-scroll">
				<text class="raw-json" selectable="true">{{ rawStr }}</text>
			</scroll-view>
		</view>

		<!-- 采样表格 -->
		<view class="section-head">
			<text>共 {{ samples.length }} 条采样</text>
			<view class="head-btns">
				<text class="export-btn" @click="exportExcel" v-if="samples.length > 0">📥 导出Excel</text>
				<text class="clear-btn" @click="clearAll" v-if="samples.length > 0">🗑 清除</text>
			</view>
		</view>

		<view class="table-wrap">
			<scroll-view scroll-x class="table-scroll" v-if="pageRows.length > 0">
				<view class="table">
					<view class="table-row table-head">
						<view class="col col-idx">#</view>
						<view class="col col-time">时间</view>
						<view class="col" v-for="col in columns" :key="col.key">{{ col.label }}</view>
					</view>
					<view class="table-row" v-for="(row, ri) in pageRows" :key="ri">
						<view class="col col-idx">{{ (page - 1) * pageSize + ri + 1 }}</view>
						<view class="col col-time">{{ fmtTime(row._sample_at) }}</view>
						<view class="col" v-for="col in columns" :key="col.key">{{ formatCell(row[col.key], col.key) }}</view>
					</view>
				</view>
			</scroll-view>
			<view class="empty" v-else>
				<text>暂无原始采样数据，点击上方「存入」从云端读取一条</text>
			</view>
		</view>

		<!-- 分页 -->
		<view class="pager" v-if="totalPage > 1">
			<text class="page-btn" :class="{ disabled: page <= 1 }" @click="goPage(page - 1)">‹</text>
			<text v-for="p in totalPage" :key="p" class="page-btn page-num" :class="{ active: p === page }" @click="goPage(p)">{{ p }}</text>
			<text class="page-btn" :class="{ disabled: page >= totalPage }" @click="goPage(page + 1)">›</text>
		</view>

		<view class="back-btn" @click="goBack">← 返回</view>
	</view>
</template>

<script>
import UserAvatar from '@/components/UserAvatar.vue'
import cloud from '@/utils/cloud.js'
import store from '@/utils/store.js'

const COLUMNS = [
	{ key: cloud.PROP_OBJECT, label: '识别物品' },
	{ key: cloud.PROP_GRASP_COUNT, label: '抓取次数' },
	{ key: cloud.PROP_OBJECT_CONF, label: '置信度' },
	{ key: cloud.CURRENT_KEYS[0], label: '大拇指' },
	{ key: cloud.CURRENT_KEYS[1], label: '食指' },
	{ key: cloud.CURRENT_KEYS[2], label: '中指' },
	{ key: cloud.CURRENT_KEYS[3], label: '无名指' },
	{ key: cloud.CURRENT_KEYS[4], label: '小指' }
]

export default {
	components: { UserAvatar },
	data() {
		return {
			deviceId: store.RAW_DEVICE_ID,
			deviceName: cloud.DEFAULT_DEVICE_NAME,
			online: false,
			saving: false,
			updateTime: '',
			columns: COLUMNS,
			samples: [],
			rawStr: '',
			page: 1,
			pageSize: 10,
			_loading: false
		}
	},
	computed: {
		latestTime() {
			const s = this.samples.length ? this.samples[this.samples.length - 1]._sample_at : ''
			return s ? s.slice(5, 16) : '--'
		},
		earliestTime() {
			const s = this.samples.length ? this.samples[0]._sample_at : ''
			return s ? s.slice(5, 16) : '--'
		},
		totalPage() { return Math.max(1, Math.ceil(this.samples.length / this.pageSize)) },
		pageRows() {
			const start = (this.page - 1) * this.pageSize
			return this.samples.slice().reverse().slice(start, start + this.pageSize)
		}
	},
	onLoad() { this.load() },
	onShow() { this.load() },
	methods: {
		load() {
			this.samples = store.loadSamples(this.deviceId)
			const dump = store.loadRawDump(this.deviceId)
			if (dump) {
				const lines = (dump.data || []).map(item => JSON.stringify(item))
				this.rawStr = `拉取时间：${dump.time}\n\n` + lines.join('\n')
			} else {
				this.rawStr = ''
			}
			if (this.samples.length) {
				const last = this.samples[this.samples.length - 1]._sample_at || ''
				this.updateTime = last.slice(11, 19) || last
			}
			if (this.page > this.totalPage) this.page = this.totalPage
		},
		async sampleNow() {
			if (this._loading) return
			this._loading = true
			this.saving = true
			try {
				const snap = await cloud.queryDeviceProperty()
				store.pushRawSample(this.deviceId, store.nowString(), snap, snap.raw)
				this.online = true
				this.page = 1
				this.load()
				uni.showToast({ title: '已存入当前采样', icon: 'success' })
			} catch (e) {
				this.online = false
				uni.showToast({ title: '读取失败：' + (e.message || e.errMsg || '网络错误'), icon: 'none', duration: 2600 })
			}
			this.saving = false
			this._loading = false
		},
		formatCell(v, key) {
			if (v === null || v === undefined || v === '') return '--'
			if (key === cloud.PROP_OBJECT_CONF) {
				const n = Number(v)
				if (isNaN(n)) return v
				return Math.round((n > 1 ? n / 100 : n) * 100) + '%'
			}
			return v
		},
		fmtTime(str) {
			if (!str) return '--'
			return str.slice(5, 16)
		},
		goPage(p) {
			if (p < 1 || p > this.totalPage) return
			this.page = p
		},
		copyRaw() {
			if (!this.rawStr) return
			uni.setClipboardData({ data: this.rawStr, success: () => uni.showToast({ title: '已复制', icon: 'success' }) })
		},
		clearAll() {
			uni.showModal({
				title: '确认清除',
				content: '将清除该设备的全部原始采样，确定？',
				success: (res) => {
					if (res.confirm) {
						store.clearSamples(this.deviceId)
						this.load()
						uni.showToast({ title: '已清除', icon: 'success' })
					}
				}
			})
		},
		exportExcel() {
			const headers = ['序号', '时间'].concat(COLUMNS.map(c => c.label))
			let csv = '﻿' + headers.join(',') + '\n'
			this.samples.forEach((row, i) => {
				const vals = [i + 1, `"${row._sample_at || ''}"`]
				COLUMNS.forEach(c => {
					const v = this.formatCell(row[c.key], c.key)
					if (v === '--' || v === '') vals.push('')
					else if (typeof v === 'number' || (!isNaN(Number(v)))) vals.push(v)
					else vals.push(`"${String(v).replace(/"/g, '""')}"`)
				})
				csv += vals.join(',') + '\n'
			})
			const filename = `原始数据_${this.deviceName}_${store.nowString().slice(0, 10)}.csv`
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
.raw-page { min-height: 100vh; background: #f2f6fb; padding: 30rpx 36rpx 60rpx; }
.page-header { position: relative; font-size: 38rpx; font-weight: 700; color: #10263f; margin-bottom: 6rpx; }
.device-sub { display: block; font-size: 21rpx; color: #8aa0b6; margin-bottom: 20rpx; padding-left: 4rpx; }

/* ===== 设备状态 / 存入 ===== */
.status-line {
	display: flex; align-items: center; justify-content: space-between; gap: 16rpx;
	background: #fff; border-radius: 20rpx; padding: 18rpx 22rpx;
	box-shadow: 0 6rpx 20rpx rgba(15,40,72,.05); margin-bottom: 22rpx;
}
.status-left { display: flex; align-items: center; gap: 8rpx; min-width: 0; flex-wrap: wrap; }
.status-dot { width: 12rpx; height: 12rpx; border-radius: 50%; background: #b0bec5; flex-shrink: 0; }
.status-dot.on { background: #27c29e; box-shadow: 0 0 10rpx rgba(39,194,158,.8); }
.status-text { font-size: 24rpx; font-weight: 700; color: #36536f; }
.status-hint { font-size: 18rpx; color: #9aaec0; }
.save-sample {
	flex-shrink: 0; font-size: 23rpx; font-weight: 700; color: #fff;
	padding: 15rpx 32rpx; border-radius: 18rpx;
	background: linear-gradient(135deg, #0f7fa8 0%, #087f8c 100%);
	box-shadow: 0 6rpx 16rpx rgba(8,127,140,.22);
}
.save-sample.busy { opacity: .6; }

.time-range {
	background: linear-gradient(135deg, #e3f2fd 0%, #e8f6f4 100%);
	border-radius: 16rpx; padding: 24rpx 28rpx; margin-bottom: 28rpx;
	display: flex; justify-content: space-around;
}
.range-item { font-size: 24rpx; color: #37474f; }
.range-val { font-weight: 600; color: #0d47a1; }

.raw-block { background: #fff; border-radius: 16rpx; padding: 24rpx; margin-bottom: 32rpx; box-shadow: 0 2rpx 12rpx rgba(0,0,0,0.04); }
.raw-title-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16rpx; }
.raw-title { font-size: 28rpx; font-weight: 600; color: #0d47a1; }
.copy-btn { font-size: 24rpx; color: #1565c0; padding: 8rpx 20rpx; border: 1rpx solid #1565c0; border-radius: 8rpx; }
.copy-btn:active { background: #1565c0; color: #fff; }
.raw-scroll { max-height: 400rpx; background: #263238; border-radius: 12rpx; padding: 20rpx 24rpx; }
.raw-json { font-size: 24rpx; font-family: monospace; color: #a5d6a7; line-height: 1.9; white-space: pre-wrap; word-break: break-all; }

.section-head { display: flex; justify-content: space-between; align-items: center; font-size: 24rpx; color: #9e9e9e; margin-bottom: 16rpx; }
.head-btns { display: flex; gap: 16rpx; }
.export-btn { color: #00897b; padding: 8rpx 20rpx; border: 1rpx solid #00897b; border-radius: 8rpx; }
.export-btn:active { background: #00897b; color: #fff; }
.clear-btn { color: #e53935; padding: 8rpx 20rpx; border: 1rpx solid #e53935; border-radius: 8rpx; }
.clear-btn:active { background: #e53935; color: #fff; }

.table-wrap { background: #fff; border-radius: 16rpx; box-shadow: 0 2rpx 12rpx rgba(0,0,0,0.04); overflow: hidden; }
.table-scroll { width: 100%; }
.table { display: table; width: max-content; min-width: 100%; border-collapse: collapse; }
.table-row { display: table-row; }
.table-head .col { background: #eceff1; font-weight: 600; color: #37474f; font-size: 22rpx; white-space: nowrap; }
.col { display: table-cell; padding: 14rpx 18rpx; font-size: 22rpx; color: #455a64; border-bottom: 1rpx solid #f0f0f0; text-align: center; white-space: nowrap; }
.col-idx { min-width: 56rpx; color: #bdbdbd; }
.col-time { font-family: monospace; color: #0d47a1; min-width: 130rpx; position: sticky; left: 0; background: #fff; z-index: 1; }
.table-head .col-time { background: #eceff1; z-index: 2; }
.table-row:last-child .col { border-bottom: none; }

.pager { display: flex; justify-content: center; align-items: center; gap: 20rpx; padding: 28rpx 0; }
.page-btn { font-size: 26rpx; color: #1565c0; padding: 10rpx 22rpx; border: 1rpx solid #1565c0; border-radius: 8rpx; }
.page-btn.disabled { color: #bdbdbd; border-color: #e0e0e0; }
.page-num { min-width: 52rpx; text-align: center; }
.page-num.active { background: #1565c0; color: #fff; border-color: #1565c0; }

.empty { padding: 80rpx 20rpx; text-align: center; font-size: 26rpx; color: #bdbdbd; line-height: 1.7; }
.back-btn { text-align: center; padding: 32rpx 0; font-size: 28rpx; color: #5c6bc0; }
</style>

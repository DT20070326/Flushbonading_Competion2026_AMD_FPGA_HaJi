<template>
	<view class="session-page">
		<view class="page-header">🖐 训练记录 · {{ patient.name || '—' }}</view>

		<!-- 本次训练汇总 -->
		<view class="summary">
			<view class="sum-item"><text class="sum-num">{{ info.duration }}</text><text class="sum-label">时长</text></view>
			<view class="sum-item"><text class="sum-num">{{ info.result }}</text><text class="sum-label">训练效果</text></view>
			<view class="sum-item"><text class="sum-num">{{ info.error }}</text><text class="sum-label">平均误差</text></view>
			<view class="sum-item"><text class="sum-num">{{ samples.length }}</text><text class="sum-label">采样条数</text></view>
		</view>

		<view class="time-range">
			<text class="range-val">{{ info.start }}</text>
			<text class="range-sep">~</text>
			<text class="range-val">{{ info.end }}</text>
		</view>

		<view class="section-head">
			<text>该时段全部舵机值 · 每根手指「目标 / 原始」（单位 °）</text>
		</view>

		<view class="table-wrap">
			<scroll-view scroll-x class="table-scroll" v-if="pageRows.length > 0">
				<view class="table">
					<view class="table-row table-head">
						<view class="col col-idx">#</view>
						<view class="col col-time">时间</view>
						<view class="col" v-for="col in columns" :key="col.k"
							:class="{ 'col-goal': col.k.charAt(0) === 'g' }">{{ col.label }}</view>
					</view>
					<view class="table-row" v-for="(row, ri) in pageRows" :key="ri">
						<view class="col col-idx">{{ (page - 1) * pageSize + ri + 1 }}</view>
						<view class="col col-time">{{ fmtTime(row.t) }}</view>
						<view class="col" v-for="col in columns" :key="col.k"
							:class="{ 'col-goal': col.k.charAt(0) === 'g' }">{{ cell(row, col.k) }}</view>
					</view>
				</view>
			</scroll-view>
			<view class="empty" v-else>
				<text>该次训练暂无舵机值数据</text>
			</view>
		</view>

		<view class="pager" v-if="totalPage > 1">
			<text class="page-btn" :class="{ disabled: page <= 1 }" @click="goPage(page - 1)">‹</text>
			<text v-for="p in totalPage" :key="p" class="page-btn page-num" :class="{ active: p === page }" @click="goPage(p)">{{ p }}</text>
			<text class="page-btn" :class="{ disabled: page >= totalPage }" @click="goPage(page + 1)">›</text>
		</view>

		<!-- 本次训练的 DeepSeek 单独分析 -->
		<view class="ai-card">
			<view class="ai-head">
				<text class="ai-title">🤖 DeepSeek 单独分析</text>
				<view class="ai-btn" :class="{ busy: analyzing }" @click="genReport">{{ analyzing ? '分析中…' : '重新生成' }}</view>
			</view>
			<view class="ai-content" v-if="analysis">{{ analysis }}</view>
			<view class="ai-content ai-loading" v-else>{{ analyzing ? '正在分析这一次训练…' : '暂无分析' }}</view>
		</view>

		<view class="back-btn" @click="goBack">← 返回</view>
	</view>
</template>

<script>
import cloud from '@/utils/cloud.js'
import store from '@/utils/store.js'
import ai from '@/utils/ai.js'

export default {
	data() {
		return {
			patientId: '',
			sessionId: '',
			patient: {},
			session: null,
			analysis: '',
			analyzing: false,
			fingerNames: cloud.FINGER_CN,
			page: 1,
			pageSize: 15
		}
	},
	computed: {
		samples() { return (this.session && this.session.angleSeries) || [] },
		// 每根手指拆成「目标」「原始」两列
		columns() {
			const cols = []
			this.fingerNames.forEach((n, f) => {
				cols.push({ k: 'g' + f, label: n + '·目标' })
				cols.push({ k: 'r' + f, label: n + '·原始' })
			})
			return cols
		},
		totalPage() { return Math.max(1, Math.ceil(this.samples.length / this.pageSize)) },
		pageRows() {
			const start = (this.page - 1) * this.pageSize
			return this.samples.slice(start, start + this.pageSize)
		},
		info() {
			const s = this.session
			if (!s) return { duration: '--', result: '--', error: '--', start: '--', end: '--' }
			const st = store.sessionStats(s)
			return {
				duration: store.fmtDuration(st.durationMs),
				result: st.resultWord,
				error: st.avgError === null ? '--' : st.avgError + '°',
				start: store.nowString(new Date(s.startTs)),
				end: store.nowString(new Date(s.endTs || s.startTs))
			}
		}
	},
	onLoad(options) {
		this.patientId = (options && options.pid) || ''
		this.sessionId = (options && options.sid) || ''
		this.patient = store.getPatient(this.patientId) || {}
		this.session = store.getSession(this.patientId, this.sessionId)
		this.analysis = (this.session && this.session.aiReport) || ''
		this.maybeGenReport()
	},
	methods: {
		// 点进来就分析一次；已缓存过的不重复请求
		maybeGenReport() {
			if (this.analysis || this.analyzing) return
			if (!this.session || !(this.session.angleSeries || []).length) return
			this.genReport()
		},
		async genReport() {
			if (this.analyzing || !this.session) return
			this.analyzing = true
			try {
				const text = await ai.sessionReport(this.patient, this.session)
				if (text) {
					this.analysis = text
					store.updateSession(this.patientId, this.sessionId, { aiReport: text })
				} else {
					this.analysis = '分析暂不可用'
				}
			} catch (e) {
				this.analysis = '分析暂不可用'
			}
			this.analyzing = false
		},
		cell(row, k) {
			const f = parseInt(k.slice(1), 10)
			const arr = k.charAt(0) === 'g' ? row.goals : row.angles
			const v = arr ? arr[f] : null
			return (v === null || v === undefined) ? '--' : String(Math.round(v))
		},
		fmtTime(t) {
			if (!t) return '--'
			const d = new Date(t)
			const pad = n => String(n).padStart(2, '0')
			return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
		},
		goPage(p) {
			if (p < 1 || p > this.totalPage) return
			this.page = p
		},
		goBack() { uni.navigateBack() }
	}
}
</script>

<style scoped>
.session-page { min-height: 100vh; background: #f5f5f5; padding: 30rpx 36rpx 60rpx; }
.page-header { font-size: 38rpx; font-weight: 700; color: #263238; margin-bottom: 24rpx; }

.summary {
	display: flex; background: #fff; border-radius: 24rpx; padding: 28rpx 0;
	box-shadow: 0 4rpx 18rpx rgba(0,0,0,0.05); margin-bottom: 20rpx;
}
.sum-item { flex: 1; display: flex; flex-direction: column; align-items: center; }
.sum-num { font-size: 36rpx; font-weight: 800; color: #1565c0; }
.sum-label { font-size: 22rpx; color: #90a4ae; margin-top: 6rpx; }

.time-range {
	display: flex; align-items: center; justify-content: center; gap: 16rpx;
	background: linear-gradient(135deg, #e3f2fd 0%, #e8f6f4 100%);
	border-radius: 16rpx; padding: 20rpx; margin-bottom: 28rpx;
}
.range-val { font-size: 26rpx; font-family: monospace; color: #0d47a1; font-weight: 600; }
.range-sep { font-size: 26rpx; color: #90a4ae; }

.section-head { font-size: 24rpx; color: #78909c; margin-bottom: 14rpx; }

.table-wrap { background: #fff; border-radius: 16rpx; box-shadow: 0 2rpx 12rpx rgba(0,0,0,0.04); overflow: hidden; }
.table-scroll { width: 100%; }
.table { display: table; width: max-content; min-width: 100%; border-collapse: collapse; }
.table-row { display: table-row; }
.table-head .col { background: #eceff1; font-weight: 600; color: #37474f; font-size: 22rpx; white-space: nowrap; }
.col { display: table-cell; padding: 14rpx 20rpx; font-size: 22rpx; color: #455a64; border-bottom: 1rpx solid #f0f0f0; text-align: center; white-space: nowrap; }
.col-idx { min-width: 56rpx; color: #bdbdbd; }
.col-time { font-family: monospace; color: #0d47a1; min-width: 130rpx; position: sticky; left: 0; background: #fff; z-index: 1; }
.table-head .col-time { background: #eceff1; z-index: 2; }
.col-goal { color: #e65100; background: #fff8f2; }
.table-head .col-goal { background: #fdece0; }
.table-row:last-child .col { border-bottom: none; }

.pager { display: flex; justify-content: center; align-items: center; gap: 20rpx; padding: 28rpx 0; }
.page-btn { font-size: 26rpx; color: #1565c0; padding: 10rpx 22rpx; border: 1rpx solid #1565c0; border-radius: 8rpx; }
.page-btn.disabled { color: #bdbdbd; border-color: #e0e0e0; }
.page-num { min-width: 52rpx; text-align: center; }
.page-num.active { background: #1565c0; color: #fff; border-color: #1565c0; }

.empty { padding: 80rpx 0; text-align: center; font-size: 28rpx; color: #bdbdbd; }

/* ===== DeepSeek 单独分析 ===== */
.ai-card {
	background: #fff; border-radius: 20rpx; padding: 26rpx 28rpx; margin-top: 28rpx;
	box-shadow: 0 4rpx 18rpx rgba(0,0,0,0.05); border-left: 10rpx solid #7e57c2;
}
.ai-head { display: flex; align-items: center; justify-content: space-between; margin-bottom: 14rpx; }
.ai-title { font-size: 30rpx; font-weight: 700; color: #4527a0; }
.ai-btn { font-size: 24rpx; color: #7e57c2; padding: 8rpx 24rpx; border: 1rpx solid #7e57c2; border-radius: 24rpx; }
.ai-btn.busy { opacity: 0.5; }
.ai-btn:active { opacity: 0.7; }
.ai-content { font-size: 27rpx; color: #455a64; line-height: 1.9; }
.ai-loading { color: #b0bec5; }

.back-btn { text-align: center; padding: 32rpx 0; font-size: 28rpx; color: #5c6bc0; }
</style>

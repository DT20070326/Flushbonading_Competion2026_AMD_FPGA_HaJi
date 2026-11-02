<template>
	<view class="archive-page">
		<!-- 顶部 -->
		<view class="archive-hero">
			<view class="hero-head">
				<view>
					<text class="eyebrow">VISION → ACTION</text>
					<text class="page-title">抓取档案</text>
					<text class="page-sub">物品图片 · 五指舵机序列 · 抓取次数</text>
				</view>
				<UserAvatar />
			</view>

			<view class="stat-row">
				<view class="stat">
					<text class="stat-num">{{ stats.total }}</text>
					<text class="stat-label">档案总数</text>
				</view>
				<view class="stat">
					<text class="stat-num ok">{{ stats.recorded }}</text>
					<text class="stat-label">已记录</text>
				</view>
				<view class="stat">
					<text class="stat-num warn">{{ stats.pending }}</text>
					<text class="stat-label">待识别</text>
				</view>
				<view class="stat">
					<text class="stat-num accent">{{ stats.grabs }}</text>
					<text class="stat-label">累计抓取</text>
				</view>
			</view>
		</view>

		<!-- 云平台状态 -->
		<view class="cloud-banner" :class="{ on: cloudOnline }">
			<view class="cloud-icon">☁</view>
			<view class="cloud-copy">
				<text class="cloud-title">{{ cloudOnline ? 'OneNET 云端档案已连接' : '当前显示本地缓存档案' }}</text>
				<text class="cloud-sub">{{ cloudOnline ? ('最近同步 ' + lastSyncText) : '点击同步读取云端属性；未连接时仍可浏览本地档案' }}</text>
			</view>
			<view class="cloud-btn" :class="{ busy: syncing }" @click="syncCloud(true)">{{ syncing ? '同步中…' : '同步' }}</view>
		</view>

		<!-- 搜索 / 新增 -->
		<view class="search-row">
			<view class="search-box">
				<text class="search-icon">⌕</text>
				<input class="search-input" v-model="q" confirm-type="search"
					placeholder="搜索物品名称、类别或抓握方式" placeholder-class="search-ph" />
				<text class="search-clear" v-if="q" @click="q = ''">✕</text>
			</view>
			<view class="add-btn" :class="{ busy: importing }" @click="importImages">
				<text class="add-plus">{{ importing ? '…' : '＋' }}</text>
				<text class="add-text">{{ importing ? '处理中' : '存入' }}</text>
			</view>
		</view>

		<!-- 分类 -->
		<view class="tabs">
			<view class="tab" :class="{ active: tab === 'all' }" @click="tab = 'all'">
				<text>全部</text><text class="tab-num">{{ stats.total }}</text>
			</view>
			<view class="tab" :class="{ active: tab === 'recorded' }" @click="tab = 'recorded'">
				<text>已记录</text><text class="tab-num">{{ stats.recorded }}</text>
			</view>
			<view class="tab" :class="{ active: tab === 'unrecorded' }" @click="tab = 'unrecorded'">
				<text>未记录</text><text class="tab-num">{{ stats.pending }}</text>
			</view>
			<view class="settings-btn" :class="{ configured: !visionDemo }" @click="openSettings">
				<text>⚙</text>
				<view class="cfg-dot" v-if="!visionDemo"></view>
			</view>
		</view>

		<view class="result-head">
			<text>{{ filtered.length }} 个物品</text>
			<view class="head-actions">
				<text class="export" @click="exportData">导出 CSV</text>
				<text class="hint" v-if="q">搜索「{{ q }}」</text>
			</view>
		</view>

		<!-- 档案网格 -->
		<view class="obj-grid" v-if="filtered.length">
			<view class="obj-card" v-for="o in filtered" :key="o.id"
				:class="{ pending: o.status !== 'recorded' }" @click="openDetail(o)">
				<view class="thumb">
					<image class="thumb-img" :src="o.image || defaultImage" mode="aspectFill"></image>
					<view class="thumb-mask"></view>
					<view class="status-badge" :class="o.status === 'recorded' ? 'done' : 'wait'">
						<view class="badge-dot"></view>
						<text>{{ o.status === 'recorded' ? '已记录' : '待识别' }}</text>
					</view>
					<view class="grab-badge" v-if="o.status === 'recorded'">
						<text class="grab-badge-num">{{ o.count || 0 }}</text>
						<text class="grab-badge-label">次</text>
					</view>
				</view>
				<view class="card-body">
					<view class="name-row">
						<text class="obj-name">{{ displayName(o) }}</text>
						<text class="obj-conf" v-if="o.status === 'recorded' && o.confidence">{{ pct(o.confidence) }}</text>
					</view>
					<text class="obj-cat">{{ o.status === 'recorded' ? (o.category || '未分类') : '等待 AI 识别' }}</text>

					<view class="servo-mini">
						<view class="mini-cell" v-for="(n, i) in shortNames" :key="i">
							<text class="mini-label">{{ n }}</text>
							<text class="mini-val" :class="{ nodata: servoOf(o, i) === null }">{{ servoText(o, i) }}</text>
						</view>
					</view>

					<view class="card-foot">
						<text class="source-text">{{ sourceText(o) }}</text>
						<view class="ai-btn" v-if="o.status !== 'recorded'" @click.stop="recognize(o)">
							{{ recognizingId === o.id ? '识别中…' : 'AI 识别' }}
						</view>
						<text class="detail-arrow" v-else>详情 ›</text>
					</view>
				</view>
			</view>
		</view>

		<view class="grid-empty" v-else>
			<view class="empty-orb">⌕</view>
			<text class="empty-title">{{ q ? '没有找到匹配的物品' : '还没有物品档案' }}</text>
			<text class="empty-sub">{{ q ? '换一个关键词试试' : '点击右上角「入库」，加入抓拍图片' }}</text>
			<view class="empty-btn" v-if="!q" @click="importImages">＋ 存入抓拍图片</view>
		</view>

		<view class="page-footer">
			<text>已记录档案优先读取 OneNET 云端；未记录图片调用视觉 API 识别后入库。</text>
			<text class="back" @click="goBack">← 返回主界面</text>
		</view>

		<!-- ===== 详情 ===== -->
		<view class="overlay" v-if="showDetail && detail" @click="showDetail = false">
			<view class="detail-sheet" @click.stop>
				<view class="detail-visual">
					<image class="detail-img" :src="detail.image || defaultImage" mode="aspectFill"></image>
					<view class="detail-shade"></view>
					<view class="detail-close" @click="showDetail = false">✕</view>
					<view class="detail-topline">
						<view class="status-badge" :class="detail.status === 'recorded' ? 'done' : 'wait'">
							<view class="badge-dot"></view>
							<text>{{ detail.status === 'recorded' ? '已记录' : '待识别' }}</text>
						</view>
						<view class="detail-count">
							<text class="dc-num">{{ detail.count || 0 }}</text>
							<text class="dc-label">次抓取</text>
						</view>
					</view>
				</view>

				<scroll-view scroll-y class="detail-scroll">
					<view class="detail-head">
						<text class="detail-name">{{ displayName(detail) }}</text>
						<text class="detail-cat">{{ detail.status === 'recorded' ? (detail.category || '未分类') : '等待 AI 识别后入库' }}</text>
						<view class="detail-tags">
							<text class="d-tag">{{ sourceText(detail) }}</text>
							<text class="d-tag" v-if="detail.graspType">{{ detail.graspType }}</text>
							<text class="d-tag" v-if="detail.confidence">置信度 {{ pct(detail.confidence) }}</text>
						</view>
						<text class="detail-desc" v-if="detail.description">{{ detail.description }}</text>
					</view>

					<view class="detail-block">
						<view class="block-head">
							<text class="block-title">五指舵机序列</text>
							<text class="block-sub">0–180°</text>
						</view>
						<view class="detail-servo">
							<view class="ds-card" v-for="(n, i) in fingerNames" :key="i">
								<text class="ds-name">{{ n }}</text>
								<text class="ds-val">{{ servoText(detail, i) }}<text class="ds-deg" v-if="servoOf(detail, i) !== null">°</text></text>
								<view class="ds-bar"><view class="ds-fill" :style="{ width: servoPct(detail, i) + '%', background: fingerColors[i] }"></view></view>
							</view>
						</view>
					</view>

					<view class="detail-block meta-block">
						<view class="meta-row"><text class="meta-k">数据来源</text><text class="meta-v">{{ sourceText(detail) }}</text></view>
						<view class="meta-row"><text class="meta-k">最近抓取</text><text class="meta-v">{{ detail.lastGrabAt ? archive.relativeTime(detail.lastGrabAt) : '尚未抓取' }}</text></view>
						<view class="meta-row"><text class="meta-k">入库时间</text><text class="meta-v">{{ archive.fmtFullTime(detail.createdAt) }}</text></view>
						<view class="meta-row" v-if="detail.cloudSyncedAt"><text class="meta-k">云端同步</text><text class="meta-v">{{ archive.fmtFullTime(detail.cloudSyncedAt) }}</text></view>
						<view class="meta-row" v-if="detail.providerLabel"><text class="meta-k">识别服务</text><text class="meta-v">{{ detail.providerLabel }}</text></view>
					</view>
				</scroll-view>

				<view class="detail-actions">
					<view class="d-act ghost" @click="removeDetail">删除</view>
					<view class="d-act ai" v-if="detail.status !== 'recorded'" @click="recognize(detail)">AI 识别</view>
					<view class="d-act primary" v-else :class="{ busy: grabbing }" @click="grabDetail">
						{{ grabbing ? '复现中…' : '复现抓取' }}
					</view>
				</view>
			</view>
		</view>

		<!-- ===== AI 识别结果 ===== -->
		<view class="overlay" v-if="showResult" @click="showResult = false">
			<view class="result-sheet" @click.stop>
				<view class="result-title-row">
					<view>
						<text class="result-kicker">VISION API RESULT</text>
						<text class="result-title">识别结果确认</text>
					</view>
					<text class="result-close" @click="showResult = false">✕</text>
				</view>

				<view class="result-top">
					<image class="result-img" :src="resultObj && resultObj.image || defaultImage" mode="aspectFill"></image>
					<view class="result-top-right">
						<text class="result-name">{{ resultName || '待命名物品' }}</text>
						<view class="result-badges">
							<text class="result-provider">{{ result.providerLabel || '视觉识别' }}</text>
							<text class="result-conf" v-if="result.confidence">置信度 {{ pct(result.confidence) }}</text>
						</view>
						<text class="result-desc">{{ result.description || '选择抓握方式后，系统会生成五指舵机序列。' }}</text>
					</view>
				</view>

				<view class="result-note" v-if="result.demo">
					当前为演示识别结果，仅用于交互演示；在右上角 ⚙ 接入真实视觉 API 后，会返回图片对应的识别结果。
				</view>

				<view class="form-row">
					<view class="form-field">
						<text class="form-label">物品名称</text>
						<input class="form-input" v-model="resultName" placeholder="请输入物品名称" />
					</view>
					<view class="form-field">
						<text class="form-label">类别</text>
						<input class="form-input" v-model="resultCategory" placeholder="如：生活用品" />
					</view>
				</view>

				<view class="grasp-row">
					<text class="form-label">抓握方式</text>
					<text class="grasp-pill">{{ result.graspType || '未识别' }}</text>
				</view>

				<view class="servo-edit">
					<view class="servo-edit-head">
						<text class="form-label">舵机序列（拇指 / 食指 / 中指 / 无名指 / 小指）</text>
					</view>
					<view class="servo-edit-grid">
						<view class="se-cell" v-for="(n, i) in shortNames" :key="i">
							<text class="se-label">{{ n }}</text>
							<input class="se-input" type="number" :value="resultServo[i]"
								@input="onServoInput(i, $event)" />
						</view>
					</view>
				</view>

				<view class="result-btns">
					<view class="result-btn cancel" @click="showResult = false">取消</view>
					<view class="result-btn ok" :class="{ busy: saving }" @click="confirmResult">{{ saving ? '存入中…' : '存入档案' }}</view>
				</view>
			</view>
		</view>

		<!-- ===== 识别设置 ===== -->
		<view class="overlay" v-if="showSettings" @click="showSettings = false">
			<view class="settings-sheet" @click.stop>
				<view class="result-title-row">
					<view>
						<text class="result-kicker">VISION SERVICE</text>
						<text class="result-title">识别服务设置</text>
					</view>
					<text class="result-close" @click="showSettings = false">✕</text>
				</view>

				<text class="settings-tip">未记录的抓拍图片会调用这里的视觉接口。Key 只保存在本机。</text>

				<view class="provider-grid">
					<view class="provider" v-for="p in providerList" :key="p.key"
						:class="{ active: draft.provider === p.key }" @click="selectProvider(p.key)">
						<view class="provider-top">
							<text class="provider-short">{{ p.short }}</text>
							<view class="provider-check" v-if="draft.provider === p.key">✓</view>
						</view>
						<text class="provider-name">{{ p.label }}</text>
						<text class="provider-model">{{ p.model || '自定义模型' }}</text>
					</view>
				</view>

				<text class="provider-tip">{{ providerTip }}</text>

				<view class="form-field full" v-if="draft.provider !== 'demo'">
					<text class="form-label">API Key</text>
					<input class="form-input" v-model="draft.key" password placeholder="粘贴视觉服务 API Key" />
				</view>

				<view class="form-field full" v-if="draft.provider === 'custom'">
					<text class="form-label">接口地址（OpenAI 兼容 /chat/completions）</text>
					<input class="form-input" v-model="draft.url" placeholder="https://example.com/v1/chat/completions" />
				</view>

				<view class="form-field full" v-if="draft.provider !== 'demo'">
					<text class="form-label">模型名称</text>
					<input class="form-input" v-model="draft.model" :placeholder="providerModelPlaceholder" />
				</view>

				<view class="settings-note">
					<text class="settings-note-title">接入说明</text>
					<text class="settings-note-text">智谱：open.bigmodel.cn 创建 API Key，默认 glm-4v-flash。通义：阿里云百炼创建 DashScope Key，默认 qwen-vl-plus。自定义接口需兼容 OpenAI 的 image_url 消息格式；H5 若遇跨域限制，请通过自有后端转发后填自定义地址。</text>
				</view>

				<view class="result-btns">
					<view class="result-btn cancel" @click="showSettings = false">取消</view>
					<view class="result-btn ok" @click="saveSettings">保存设置</view>
				</view>
			</view>
		</view>
	</view>
</template>

<script>
import UserAvatar from '@/components/UserAvatar.vue'
import archive from '@/utils/archive.js'
import vision from '@/utils/vision.js'
import media from '@/utils/media.js'

const SHORT_NAMES = ['拇', '食', '中', '无', '小']
const FINGER_NAMES = ['拇指', '食指', '中指', '无名指', '小指']
const FINGER_COLORS = ['#ef5350', '#ab47bc', '#ffa726', '#26a69a', '#42a5f5']

export default {
	components: { UserAvatar },
	data() {
		return {
			objects: [],
			stats: { total: 0, recorded: 0, pending: 0, grabs: 0 },
			tab: 'all',
			q: '',
			syncing: false,
			importing: false,
			recognizingId: '',
			cloudOnline: false,
			lastSyncText: '尚未同步',
			defaultImage: archive.DEFAULT_IMAGE,
			shortNames: SHORT_NAMES,
			fingerNames: FINGER_NAMES,
			fingerColors: FINGER_COLORS,
			archive,
			showDetail: false,
			detail: null,
			grabbing: false,
			showResult: false,
			resultObj: null,
			result: {},
			resultName: '',
			resultCategory: '',
			resultServo: [0, 0, 0, 0, 0],
			saving: false,
			showSettings: false,
			draft: vision.getConfig(),
			visionDemo: vision.isDemo()
		}
	},
	computed: {
		filtered() {
			const key = this.q.trim().toLowerCase()
			let list = this.objects.filter(o => {
				if (this.tab === 'recorded' && o.status !== 'recorded') return false
				if (this.tab === 'unrecorded' && o.status !== 'unrecorded') return false
				if (!key) return true
				const hay = [o.name, o.category, o.graspType, o.description].join(' ').toLowerCase()
				return hay.indexOf(key) >= 0
			})
			list = list.slice()
			list.sort((a, b) => {
				if (a.status !== b.status) return a.status === 'recorded' ? -1 : 1
				return (b.lastGrabAt || b.updatedAt || 0) - (a.lastGrabAt || a.updatedAt || 0)
			})
			return list
		},
		providerList() {
			return Object.keys(vision.PROVIDERS).map(k => Object.assign({ key: k }, vision.PROVIDERS[k]))
		},
		providerTip() {
			const p = vision.PROVIDERS[this.draft.provider] || vision.PROVIDERS.demo
			return p.tip
		},
		providerModelPlaceholder() {
			const p = vision.PROVIDERS[this.draft.provider] || vision.PROVIDERS.custom
			return p.model || '填写模型名称'
		}
	},
	onLoad() {
		this.reload()
		this.syncCloud(false)
		uni.$off('grasp-archive-changed', this.onAutoRecognized)
		uni.$on('grasp-archive-changed', this.onAutoRecognized)
	},
	onUnload() { uni.$off('grasp-archive-changed', this.onAutoRecognized) },
	onShow() { this.reload() },
	methods: {
		onAutoRecognized() { this.reload() },
		reload() {
			this.objects = archive.loadObjects()
			this.stats = archive.stats()
			const meta = archive.cloudMeta()
			this.cloudOnline = meta.cloudOnline
			this.lastSyncText = meta.lastCloudText
		},
		displayName(o) { return (o && o.name) ? o.name : '待识别物品' },
		sourceText(o) { return o && o.status === 'recorded' ? archive.sourceLabel(o.source) : '设备抓拍 · 未识别' },
		pct(v) {
			const n = Number(v) || 0
			return Math.round((n > 1 ? n / 100 : n) * 100) + '%'
		},
		servoOf(o, i) {
			if (!o || !o.servo) return null
			const v = o.servo[i]
			return (v === null || v === undefined || isNaN(Number(v))) ? null : Number(v)
		},
		servoText(o, i) {
			const v = this.servoOf(o, i)
			return v === null ? '--' : String(Math.round(v))
		},
		servoPct(o, i) {
			const v = this.servoOf(o, i)
			return v === null ? 0 : Math.max(0, Math.min(100, v / 180 * 100))
		},
		async syncCloud(manual) {
			if (this.syncing) return
			this.syncing = true
			try {
				const res = await archive.syncCloud()
				this.reload()
				if (manual) {
					let msg = '云端已同步'
					if (res.added || res.updated) msg = `云端同步 · 新增 ${res.added} / 更新 ${res.updated}`
					else if (res.snap && res.snap.objectName) msg = '已同步当前识别物品'
					else msg = '已读取云端五指属性'
					uni.showToast({ title: msg, icon: 'none', duration: 2200 })
				}
			} catch (e) {
				archive.markCloudOffline()
				this.reload()
				if (manual) {
					uni.showToast({ title: '云端读取失败：' + (e.message || e.errMsg || '网络错误'), icon: 'none', duration: 2800 })
				}
			}
			this.syncing = false
		},
		async importImages() {
			if (this.importing) return
			this.importing = true
			try {
				const paths = await media.chooseImages(9)
				if (!paths.length) { this.importing = false; return }
				let saved = 0
				for (let i = 0; i < paths.length; i++) {
					let image = paths[i]
					try { image = await media.compressImage(paths[i], 1000, 0.78) } catch (e) {}
					try {
						archive.addPending({ image })
						saved++
					} catch (e) {
						uni.showToast({ title: e.message || '图片保存失败', icon: 'none', duration: 2600 })
						break
					}
				}
				this.reload()
				if (saved) uni.showToast({ title: `已加入 ${saved} 张待识别图片`, icon: 'none' })
			} catch (e) {
				const msg = String((e && (e.errMsg || e.message)) || '')
				if (msg.indexOf('cancel') < 0) {
					uni.showToast({ title: '选择图片失败', icon: 'none' })
				}
			}
			this.importing = false
		},
		openDetail(o) {
			const fresh = archive.getObject(o.id) || o
			this.detail = Object.assign({}, fresh)
			this.showDetail = true
		},
		async recognize(o) {
			if (this.recognizingId) return
			this.showDetail = false
			this.recognizingId = o.id
			try {
				const res = await vision.recognize(o.image, o.hint)
				this.resultObj = o
				this.result = res
				this.resultName = res.name || ''
				this.resultCategory = res.category || ''
				// 已经抓到的五路舵机值优先，识别结果只补名称/类别
				const captured = (o.servo && o.servo.length >= 5 && o.servo.every(v => v !== null && v !== undefined))
					? o.servo : null
				this.resultServo = (captured || res.servo || [64, 74, 78, 76, 70]).slice(0, 5).map(v => {
					const n = Number(v)
					return isNaN(n) ? 0 : Math.max(0, Math.min(180, Math.round(n)))
				})
				this.showResult = true
			} catch (e) {
				uni.showToast({ title: '识别失败：' + (e.message || e.errMsg || '请检查识别设置'), icon: 'none', duration: 3000 })
			}
			this.recognizingId = ''
		},
		onServoInput(i, e) {
			const raw = e && e.detail ? e.detail.value : ''
			let n = Number(raw)
			if (isNaN(n)) n = 0
			n = Math.max(0, Math.min(180, Math.round(n)))
			this.resultServo.splice(i, 1, n)
		},
		confirmResult() {
			if (this.saving || !this.resultObj) return
			const name = String(this.resultName || '').trim()
			if (!name) {
				uni.showToast({ title: '请填写物品名称', icon: 'none' })
				return
			}
			this.saving = true
			try {
				const payload = Object.assign({}, this.result, {
					name,
					category: String(this.resultCategory || '').trim() || '未分类',
					servo: this.resultServo.slice()
				})
				archive.applyRecognition(this.resultObj.id, payload, this.resultServo)
				this.showResult = false
				this.reload()
				uni.showToast({ title: '已存入档案', icon: 'success' })
			} catch (e) {
				uni.showToast({ title: e.message || '入库失败', icon: 'none' })
			}
			this.saving = false
		},
		grabDetail() {
			if (this.grabbing || !this.detail) return
			if (this.detail.status !== 'recorded') return
			this.grabbing = true
			try { uni.vibrateShort({ type: 'light' }) } catch (e) {}
			setTimeout(() => {
				const updated = archive.incrementGrab(this.detail.id)
				if (updated) this.detail = Object.assign({}, updated)
				this.reload()
				this.grabbing = false
				uni.showToast({ title: '已复现抓取动作', icon: 'none' })
			}, 600)
		},
		removeDetail() {
			if (!this.detail) return
			const name = this.displayName(this.detail)
			uni.showModal({
				title: '删除档案',
				content: `确定删除「${name}」及其抓取记录吗？`,
				success: res => {
					if (!res.confirm) return
					archive.removeObject(this.detail.id)
					this.showDetail = false
					this.detail = null
					this.reload()
					uni.showToast({ title: '已删除', icon: 'success' })
				}
			})
		},
		openSettings() {
			this.draft = Object.assign({}, vision.getConfig())
			this.showSettings = true
		},
		selectProvider(key) {
			this.draft.provider = key
			const p = vision.PROVIDERS[key] || {}
			if (key === 'custom') this.draft.model = ''
			else if (p.model) this.draft.model = p.model
		},
		saveSettings() {
			const d = this.draft
			if (d.provider !== 'demo' && !String(d.key || '').trim()) {
				uni.showToast({ title: '请填写 API Key', icon: 'none' })
				return
			}
			if (d.provider === 'custom' && !String(d.url || '').trim()) {
				uni.showToast({ title: '请填写接口地址', icon: 'none' })
				return
			}
			vision.saveConfig({
				provider: d.provider,
				key: String(d.key || '').trim(),
				url: String(d.url || '').trim(),
				model: String(d.model || '').trim()
			})
			this.visionDemo = vision.isDemo()
			this.showSettings = false
			uni.showToast({ title: '识别设置已保存', icon: 'success' })
		},
		exportData() {
			const csv = archive.exportCsv()
			// #ifdef H5
			const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' })
			const url = URL.createObjectURL(blob)
			const a = document.createElement('a')
			a.href = url
			a.download = '仿生手抓取档案_' + archive.fmtFullTime(Date.now()).slice(0, 10) + '.csv'
			document.body.appendChild(a)
			a.click()
			document.body.removeChild(a)
			URL.revokeObjectURL(url)
			uni.showToast({ title: '已导出 CSV', icon: 'success' })
			// #endif
			// #ifndef H5
			uni.setClipboardData({
				data: csv,
				success: () => uni.showToast({ title: 'CSV 已复制到剪贴板', icon: 'none', duration: 2600 })
			})
			// #endif
		},
		goBack() { uni.navigateBack() }
	}
}
</script>

<style scoped>
.archive-page {
	min-height: 100vh;
	background: #f2f6fb;
	padding: 0 30rpx 60rpx;
}

/* ===== 顶部 ===== */
.archive-hero {
	margin: 0 -30rpx;
	padding: 40rpx 30rpx 30rpx;
	background: linear-gradient(135deg, #081b33 0%, #0d3556 55%, #0c4457 100%);
	border-radius: 0 0 38rpx 38rpx;
	position: relative;
	overflow: hidden;
}
.archive-hero::after {
	content: ''; position: absolute; width: 340rpx; height: 340rpx; right: -120rpx; top: -160rpx;
	border-radius: 50%; background: rgba(34,211,238,.18); filter: blur(30rpx);
}
.hero-head { position: relative; display: flex; align-items: flex-start; justify-content: space-between; z-index: 2; }
.eyebrow { display: block; font-size: 18rpx; letter-spacing: 3rpx; color: rgba(125,230,255,.7); font-weight: 600; }
.page-title { display: block; font-size: 50rpx; font-weight: 800; color: #f7fbff; letter-spacing: 3rpx; margin-top: 4rpx; }
.page-sub { display: block; font-size: 21rpx; color: rgba(200,225,245,.6); margin-top: 8rpx; }
.stat-row {
	position: relative; z-index: 2; display: flex; margin-top: 30rpx;
	background: rgba(255,255,255,.06); border: 1rpx solid rgba(255,255,255,.1);
	border-radius: 22rpx; padding: 20rpx 0;
}
.stat { flex: 1; display: flex; flex-direction: column; align-items: center; border-right: 1rpx solid rgba(255,255,255,.08); }
.stat:last-child { border-right: none; }
.stat-num { font-size: 38rpx; font-weight: 800; color: #fff; line-height: 1.1; }
.stat-num.ok { color: #6bf0c2; }
.stat-num.warn { color: #ffcc80; }
.stat-num.accent { color: #7de6ff; }
.stat-label { font-size: 18rpx; color: rgba(200,225,245,.55); margin-top: 5rpx; }

/* ===== 云平台 ===== */
.cloud-banner {
	display: flex; align-items: center; gap: 14rpx;
	background: #fff; border-radius: 20rpx; padding: 18rpx 20rpx;
	margin-top: 24rpx; box-shadow: 0 8rpx 22rpx rgba(15,40,72,.06);
	border-left: 6rpx solid #b0bec5;
}
.cloud-banner.on { border-left-color: #27c29e; }
.cloud-icon { font-size: 30rpx; color: #90a4ae; }
.cloud-banner.on .cloud-icon { color: #27c29e; }
.cloud-copy { flex: 1; min-width: 0; display: flex; flex-direction: column; }
.cloud-title { font-size: 23rpx; font-weight: 700; color: #16324e; }
.cloud-sub { font-size: 17rpx; color: #8aa0b6; margin-top: 3rpx; }
.cloud-btn {
	font-size: 20rpx; color: #0f7fa8; background: #eaf4fc; padding: 10rpx 24rpx; border-radius: 20rpx; flex-shrink: 0;
}
.cloud-btn.busy { opacity: .55; }

/* ===== 搜索 ===== */
.search-row { display: flex; gap: 14rpx; margin-top: 22rpx; }
.search-box {
	flex: 1; display: flex; align-items: center; gap: 10rpx;
	background: #fff; border-radius: 22rpx; padding: 0 20rpx; height: 82rpx;
	box-shadow: 0 8rpx 22rpx rgba(15,40,72,.05);
}
.search-icon { font-size: 34rpx; color: #9aaec0; }
.search-input { flex: 1; font-size: 24rpx; color: #16324e; height: 82rpx; }
.search-ph { color: #b0bfcd; }
.search-clear { font-size: 22rpx; color: #b0bec5; padding: 10rpx; }
.add-btn {
	width: 108rpx; height: 82rpx; border-radius: 22rpx; flex-shrink: 0;
	background: linear-gradient(135deg, #0d4d7a 0%, #087f8c 100%);
	display: flex; flex-direction: column; align-items: center; justify-content: center;
	box-shadow: 0 10rpx 24rpx rgba(8,93,120,.22);
}
.add-btn.busy { opacity: .6; }
.add-plus { font-size: 26rpx; color: #fff; line-height: 1; }
.add-text { font-size: 18rpx; color: rgba(220,245,255,.8); margin-top: 3rpx; }

/* ===== tabs ===== */
.tabs { display: flex; align-items: center; gap: 12rpx; margin-top: 22rpx; }
.tab {
	display: flex; align-items: center; gap: 8rpx;
	font-size: 22rpx; color: #7a8fa5; background: #fff;
	padding: 12rpx 20rpx; border-radius: 20rpx;
	box-shadow: 0 4rpx 14rpx rgba(15,40,72,.04);
}
.tab.active { color: #fff; background: #16324e; font-weight: 700; box-shadow: 0 8rpx 20rpx rgba(22,50,78,.18); }
.tab-num { font-size: 17rpx; padding: 1rpx 9rpx; border-radius: 10rpx; background: #eef3f8; color: #7a8fa5; }
.tab.active .tab-num { background: rgba(255,255,255,.16); color: #fff; }
.settings-btn {
	position: relative; margin-left: auto; width: 56rpx; height: 56rpx;
	display: flex; align-items: center; justify-content: center;
	background: #fff; border-radius: 18rpx; font-size: 27rpx; color: #7a8fa5;
	box-shadow: 0 4rpx 14rpx rgba(15,40,72,.04);
}
.cfg-dot { position: absolute; top: 8rpx; right: 8rpx; width: 10rpx; height: 10rpx; border-radius: 50%; background: #27c29e; }

.result-head {
	display: flex; align-items: center; justify-content: space-between;
	margin: 18rpx 4rpx 14rpx; font-size: 19rpx; color: #9aaec0;
}
.head-actions { display: flex; align-items: center; gap: 14rpx; }
.export { color: #0f7fa8; }
.hint { color: #b0bfcd; }

/* ===== 卡片 ===== */
.obj-grid { display: flex; flex-wrap: wrap; gap: 18rpx; }
.obj-card {
	flex: 0 0 calc((100% - 18rpx) / 2); min-width: 0; box-sizing: border-box;
	background: #fff; border-radius: 24rpx; overflow: hidden;
	box-shadow: 0 10rpx 26rpx rgba(15,40,72,.07);
}
.obj-card:active { transform: scale(.99); opacity: .94; }
.obj-card.pending { border: 1rpx dashed #cfe0ee; }
.thumb { position: relative; height: 206rpx; background: #e8eef5; overflow: hidden; }
.thumb-img { width: 100%; height: 100%; }
.thumb-mask { position: absolute; left: 0; right: 0; bottom: 0; height: 70rpx; background: linear-gradient(180deg, transparent, rgba(8,27,51,.35)); }
.status-badge {
	position: absolute; left: 12rpx; top: 12rpx; display: flex; align-items: center; gap: 6rpx;
	font-size: 16rpx; padding: 5rpx 12rpx; border-radius: 14rpx;
	background: rgba(8,27,51,.55); color: #fff; backdrop-filter: blur(6px);
}
.badge-dot { width: 7rpx; height: 7rpx; border-radius: 50%; background: #cfd8dc; }
.status-badge.done .badge-dot { background: #6bf0c2; box-shadow: 0 0 8rpx #6bf0c2; }
.status-badge.wait { background: rgba(255,152,0,.18); border: 1rpx solid rgba(255,183,77,.5); color: #ffd596; }
.status-badge.wait .badge-dot { background: #ffb74d; }
.grab-badge {
	position: absolute; right: 12rpx; top: 12rpx;
	display: flex; align-items: baseline; gap: 3rpx;
	background: rgba(8,27,51,.55); border-radius: 14rpx; padding: 4rpx 12rpx;
	backdrop-filter: blur(6px);
}
.grab-badge-num { font-size: 24rpx; font-weight: 800; color: #fff; }
.grab-badge-label { font-size: 14rpx; color: rgba(255,255,255,.7); }
.card-body { padding: 16rpx 16rpx 14rpx; }
.name-row { display: flex; align-items: center; justify-content: space-between; gap: 6rpx; }
.obj-name { font-size: 27rpx; font-weight: 800; color: #10263f; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.obj-conf { font-size: 15rpx; color: #0f9d80; background: #e8f8f2; padding: 2rpx 9rpx; border-radius: 10rpx; flex-shrink: 0; }
.obj-cat { display: block; font-size: 17rpx; color: #8aa0b6; margin-top: 3rpx; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.servo-mini {
	display: flex; justify-content: space-between; gap: 2rpx;
	background: #f7fafd; border-radius: 14rpx; padding: 10rpx 6rpx; margin-top: 12rpx;
}
.mini-cell { flex: 1; display: flex; flex-direction: column; align-items: center; min-width: 0; }
.mini-label { font-size: 14rpx; color: #a8b8c8; }
.mini-val { font-size: 19rpx; font-weight: 700; color: #36536f; margin-top: 2rpx; }
.mini-val.nodata { color: #c5d1dc; }
.card-foot { display: flex; align-items: center; justify-content: space-between; margin-top: 12rpx; gap: 6rpx; }
.source-text { font-size: 15rpx; color: #9aaec0; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.ai-btn {
	font-size: 17rpx; font-weight: 700; color: #fff; flex-shrink: 0;
	background: linear-gradient(135deg, #0f7fa8 0%, #087f8c 100%);
	padding: 7rpx 16rpx; border-radius: 16rpx;
	box-shadow: 0 4rpx 12rpx rgba(8,127,140,.22);
}
.detail-arrow { font-size: 17rpx; color: #b0bfcd; flex-shrink: 0; }

/* ===== 空状态 ===== */
.grid-empty { display: flex; flex-direction: column; align-items: center; padding: 110rpx 20rpx 90rpx; }
.empty-orb {
	width: 120rpx; height: 120rpx; border-radius: 40rpx; background: #fff;
	display: flex; align-items: center; justify-content: center;
	font-size: 54rpx; color: #c5d1dc; box-shadow: 0 12rpx 30rpx rgba(15,40,72,.06);
}
.empty-title { font-size: 27rpx; font-weight: 700; color: #36536f; margin-top: 24rpx; }
.empty-sub { font-size: 20rpx; color: #9aaec0; margin-top: 8rpx; }
.empty-btn { margin-top: 26rpx; font-size: 23rpx; color: #fff; background: #0d4d7a; padding: 16rpx 34rpx; border-radius: 24rpx; }

.page-footer { display: flex; flex-direction: column; align-items: center; gap: 14rpx; padding: 48rpx 20rpx 10rpx; }
.page-footer text { font-size: 17rpx; color: #a8b8c8; text-align: center; line-height: 1.6; }
.page-footer .back { color: #5c7b98; font-size: 21rpx; }

/* ===== 通用弹层 ===== */
.overlay {
	position: fixed; left: 0; top: 0; right: 0; bottom: 0; z-index: 999;
	background: rgba(4, 13, 24, .58); backdrop-filter: blur(4px);
	display: flex; align-items: center; justify-content: center; padding: 28rpx;
}
.detail-sheet {
	width: 100%; max-width: 680rpx; max-height: 90vh; background: #fff;
	border-radius: 32rpx; overflow: hidden; display: flex; flex-direction: column;
	box-shadow: 0 30rpx 80rpx rgba(0,0,0,.3);
}
.detail-visual { position: relative; height: 330rpx; background: #e8eef5; flex-shrink: 0; }
.detail-img { width: 100%; height: 100%; }
.detail-shade { position: absolute; left: 0; right: 0; bottom: 0; height: 150rpx; background: linear-gradient(180deg, transparent, rgba(5,18,32,.62)); }
.detail-close {
	position: absolute; right: 18rpx; top: 18rpx; width: 56rpx; height: 56rpx; border-radius: 50%;
	background: rgba(5,18,32,.45); color: #fff; display: flex; align-items: center; justify-content: center; font-size: 24rpx;
}
.detail-topline { position: absolute; left: 18rpx; right: 18rpx; bottom: 16rpx; display: flex; align-items: flex-end; justify-content: space-between; }
.detail-topline .status-badge { position: static; }
.detail-count { display: flex; align-items: baseline; gap: 5rpx; }
.dc-num { font-size: 46rpx; font-weight: 800; color: #fff; }
.dc-label { font-size: 17rpx; color: rgba(255,255,255,.72); }
.detail-scroll { flex: 1; min-height: 0; padding: 24rpx 26rpx 10rpx; box-sizing: border-box; }
.detail-head { padding-bottom: 18rpx; border-bottom: 1rpx solid #eef3f8; }
.detail-name { display: block; font-size: 40rpx; font-weight: 800; color: #10263f; }
.detail-cat { display: block; font-size: 20rpx; color: #8aa0b6; margin-top: 4rpx; }
.detail-tags { display: flex; flex-wrap: wrap; gap: 8rpx; margin-top: 12rpx; }
.d-tag { font-size: 17rpx; color: #4a6b88; background: #f1f6fa; padding: 4rpx 12rpx; border-radius: 10rpx; }
.detail-desc { display: block; font-size: 21rpx; color: #6d849b; line-height: 1.7; margin-top: 12rpx; }

.detail-block { padding: 20rpx 0 10rpx; border-bottom: 1rpx solid #eef3f8; }
.block-head { display: flex; align-items: center; justify-content: space-between; margin-bottom: 14rpx; }
.block-title { font-size: 24rpx; font-weight: 700; color: #16324e; }
.block-sub { font-size: 17rpx; color: #a8b8c8; }
.detail-servo { display: flex; gap: 8rpx; }
.ds-card { flex: 1; min-width: 0; background: #f7fafd; border-radius: 14rpx; padding: 12rpx 5rpx 10rpx; display: flex; flex-direction: column; align-items: center; }
.ds-name { font-size: 15rpx; color: #8aa0b6; }
.ds-val { font-size: 25rpx; font-weight: 800; color: #16324e; margin-top: 3rpx; }
.ds-deg { font-size: 14rpx; color: #9aaec0; font-weight: 400; }
.ds-bar { width: 76%; height: 5rpx; background: #e4ebf3; border-radius: 4rpx; margin-top: 8rpx; overflow: hidden; }
.ds-fill { height: 100%; border-radius: 4rpx; }

.meta-block { padding-bottom: 16rpx; border-bottom: none; }
.meta-row { display: flex; justify-content: space-between; padding: 9rpx 2rpx; font-size: 20rpx; }
.meta-k { color: #9aaec0; }
.meta-v { color: #36536f; font-weight: 600; text-align: right; }

.detail-actions {
	display: flex; gap: 14rpx; padding: 18rpx 26rpx 24rpx; border-top: 1rpx solid #eef3f8; background: #fff;
}
.d-act { text-align: center; padding: 18rpx 0; border-radius: 18rpx; font-size: 24rpx; font-weight: 700; }
.d-act.ghost { flex: .7; background: #f5f8fb; color: #7a8fa5; }
.d-act.ai { flex: 1; background: #eaf4fc; color: #0f7fa8; }
.d-act.primary { flex: 1.6; background: linear-gradient(135deg, #0d4d7a 0%, #087f8c 100%); color: #fff; box-shadow: 0 8rpx 20rpx rgba(8,93,120,.2); }
.d-act.primary.busy { opacity: .65; }

/* ===== 识别结果 ===== */
.result-sheet, .settings-sheet {
	width: 100%; max-width: 680rpx; max-height: 90vh; background: #fff;
	border-radius: 30rpx; padding: 30rpx 28rpx 28rpx; box-sizing: border-box;
	display: flex; flex-direction: column; overflow-y: auto;
	box-shadow: 0 30rpx 80rpx rgba(0,0,0,.3);
}
.result-title-row { display: flex; align-items: flex-start; justify-content: space-between; margin-bottom: 20rpx; }
.result-kicker { display: block; font-size: 15rpx; letter-spacing: 2rpx; color: #0f9d80; font-weight: 700; }
.result-title { display: block; font-size: 34rpx; font-weight: 800; color: #10263f; margin-top: 3rpx; }
.result-close { font-size: 26rpx; color: #9aaec0; padding: 8rpx; }
.result-top { display: flex; gap: 18rpx; }
.result-img { width: 190rpx; height: 190rpx; border-radius: 22rpx; background: #e8eef5; flex-shrink: 0; }
.result-top-right { flex: 1; min-width: 0; display: flex; flex-direction: column; }
.result-name { font-size: 30rpx; font-weight: 800; color: #16324e; }
.result-badges { display: flex; flex-wrap: wrap; gap: 7rpx; margin-top: 8rpx; }
.result-provider { font-size: 16rpx; color: #0f7fa8; background: #eaf4fc; padding: 3rpx 10rpx; border-radius: 9rpx; }
.result-conf { font-size: 16rpx; color: #0f9d80; background: #e8f8f2; padding: 3rpx 10rpx; border-radius: 9rpx; }
.result-desc { font-size: 20rpx; color: #7a8fa5; line-height: 1.6; margin-top: 10rpx; }
.result-note {
	font-size: 19rpx; line-height: 1.65; color: #8a6d3b; background: #fff8e8;
	border: 1rpx solid #f3e2bd; border-radius: 14rpx; padding: 14rpx 16rpx; margin-top: 16rpx;
}

.form-row { display: flex; gap: 14rpx; margin-top: 18rpx; }
.form-field { flex: 1; min-width: 0; }
.form-field.full { width: 100%; margin-top: 16rpx; }
.form-label { display: block; font-size: 18rpx; color: #7a8fa5; margin-bottom: 7rpx; font-weight: 600; }
.form-input {
	width: 100%; height: 68rpx; border: 1rpx solid #e2eaf2; border-radius: 14rpx;
	background: #f8fbfd; padding: 0 16rpx; font-size: 22rpx; color: #16324e; box-sizing: border-box;
}
.grasp-row { display: flex; align-items: center; justify-content: space-between; margin-top: 18rpx; }
.grasp-pill { font-size: 19rpx; color: #7a5bb5; background: #f1ecfb; padding: 6rpx 16rpx; border-radius: 12rpx; }
.servo-edit { margin-top: 20rpx; }
.servo-edit-grid { display: flex; gap: 9rpx; margin-top: 8rpx; }
.se-cell { flex: 1; min-width: 0; background: #f7fafd; border-radius: 12rpx; padding: 9rpx 4rpx; display: flex; flex-direction: column; align-items: center; }
.se-label { font-size: 15rpx; color: #8aa0b6; }
.se-input { width: 100%; text-align: center; font-size: 24rpx; font-weight: 700; color: #16324e; height: 42rpx; }
.result-btns { display: flex; gap: 14rpx; margin-top: 24rpx; }
.result-btn { flex: 1; text-align: center; padding: 18rpx 0; border-radius: 18rpx; font-size: 24rpx; font-weight: 700; }
.result-btn.cancel { background: #f5f8fb; color: #7a8fa5; }
.result-btn.ok { background: linear-gradient(135deg, #0d4d7a 0%, #087f8c 100%); color: #fff; box-shadow: 0 8rpx 20rpx rgba(8,93,120,.2); }
.result-btn.ok.busy { opacity: .6; }

/* ===== 设置 ===== */
.settings-tip { font-size: 20rpx; color: #8aa0b6; line-height: 1.65; margin-bottom: 16rpx; }
.provider-grid { display: flex; flex-wrap: wrap; gap: 12rpx; }
.provider {
	flex: 0 0 calc((100% - 12rpx) / 2); box-sizing: border-box;
	background: #f8fbfd; border: 2rpx solid #e8f0f7; border-radius: 18rpx; padding: 14rpx 16rpx;
	display: flex; flex-direction: column;
}
.provider.active { border-color: #0f7fa8; background: #f0f9fc; box-shadow: 0 0 0 2rpx rgba(15,127,168,.08); }
.provider-top { display: flex; align-items: center; justify-content: space-between; }
.provider-short { font-size: 16rpx; font-weight: 800; color: #0f7fa8; letter-spacing: 1rpx; }
.provider-check { font-size: 17rpx; color: #0f9d80; }
.provider-name { font-size: 21rpx; font-weight: 700; color: #16324e; margin-top: 7rpx; }
.provider-model { font-size: 17rpx; color: #9aaec0; margin-top: 3rpx; font-family: monospace; }
.provider-tip { font-size: 18rpx; color: #7a8fa5; margin-top: 12rpx; line-height: 1.6; }
.settings-note { background: #f7fafd; border-radius: 14rpx; padding: 14rpx 16rpx; margin-top: 18rpx; }
.settings-note-title { display: block; font-size: 18rpx; font-weight: 700; color: #36536f; }
.settings-note-text { display: block; font-size: 19rpx; color: #8aa0b6; line-height: 1.7; margin-top: 5rpx; }
</style>

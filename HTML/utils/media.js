// 图片入库辅助：选图 -> base64 压缩，方便存进本地档案
import vision from '@/utils/vision.js'

function compressOnH5(dataUrl, maxSide, quality) {
	return new Promise(resolve => {
		// #ifdef H5
		const img = new Image()
		img.onload = () => {
			let w = img.width
			let h = img.height
			const scale = Math.min(1, maxSide / Math.max(w, h))
			w = Math.max(1, Math.round(w * scale))
			h = Math.max(1, Math.round(h * scale))
			const canvas = document.createElement('canvas')
			canvas.width = w
			canvas.height = h
			const ctx = canvas.getContext('2d')
			ctx.fillStyle = '#ffffff'
			ctx.fillRect(0, 0, w, h)
			ctx.drawImage(img, 0, 0, w, h)
			try { resolve(canvas.toDataURL('image/jpeg', quality)) }
			catch (e) { resolve(dataUrl) }
		}
		img.onerror = () => resolve(dataUrl)
		img.src = dataUrl
		return
		// #endif

		// #ifndef H5
		resolve(dataUrl)
		// #endif
	})
}

async function compressImage(path, maxSide, quality) {
	const dataUrl = await vision.toDataUrl(path)
	if (/^https?:/i.test(dataUrl)) return dataUrl
	return compressOnH5(dataUrl, maxSide || 1000, quality || 0.78)
}

function chooseImages(count) {
	return new Promise((resolve, reject) => {
		uni.chooseImage({
			count: count || 9,
			sizeType: ['compressed'],
			sourceType: ['album', 'camera'],
			success: res => resolve((res && res.tempFilePaths) || []),
			fail: reject
		})
	})
}

export { compressImage, chooseImages }
export default { compressImage, chooseImages }

// OneNET 鉴权 token 生成
// 算法：sign = Base64(HMAC-SHA1(access_key, et + "\n" + method + "\n" + res + "\n" + version))
// access_key 先做 Base64 解码，作为 HMAC 的密钥。
const crypto = require('crypto')

const DEFAULT_VERSION = '2022-05-01'

function createToken(params) {
	const access_key = Buffer.from(params.access_key, 'base64')
	const version = params.version || DEFAULT_VERSION
	const method = 'sha1'
	const res = params.res
	const et = Math.ceil((Date.now() + 365 * 24 * 3600 * 1000) / 1000)

	const stringToSign = et + "\n" + method + "\n" + res + "\n" + version
	const sign = crypto.createHmac(method, access_key).update(stringToSign).digest().toString('base64')

	return `version=${version}&res=${encodeURIComponent(res)}&et=${et}&method=${method}&sign=${encodeURIComponent(sign)}`
}

// 产品级 token：res = products/{product_id}，可查询该产品下任意设备
function createProductToken(params) {
	return createToken({
		access_key: params.access_key,
		version: params.version,
		res: 'products/' + params.product_id
	})
}

module.exports = {
	createToken,
	createProductToken
}

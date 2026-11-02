#include "OneNetClient.h"
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <ArduinoJson.h>

// ── OneNET 参数：与前端 HTML/utils/cloud.js 完全一致 ──────────────
// 产品级 ACCESS_KEY 从 include/secrets.h 读取（该文件不提交到仓库）
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "缺少 ESP32/include/secrets.h，ACCESS_KEY 使用占位值，请复制 secrets.example.h 后填写"
#define ONENET_ACCESS_KEY "YOUR_ONENET_PRODUCT_ACCESS_KEY"
#endif

const char* OneNetClient::MQTT_HOST     = "mqtts.heclouds.com";
const int   OneNetClient::MQTT_PORT     = 1883;
const char* OneNetClient::PRODUCT_ID    = "9OX8NAt415";
const char* OneNetClient::DEVICE_NAME   = "Fpga";
const char* OneNetClient::ACCESS_KEY    = ONENET_ACCESS_KEY;
const char* OneNetClient::TOKEN_VERSION = "2022-05-01";
const char* OneNetClient::TOKEN_METHOD  = "sha1";

// 与前端 cloud.js 一致的属性标识符（大小写必须一致）
static const char* CURRENT_KEYS[HAND_FINGERS] = {
    "Damuzhi", "shizhi", "Zhongzhi", "wumingzhi", "xiaomuzhi"
};
static const char* GOAL_KEYS[HAND_FINGERS] = {
    "Damuzhi_Goal", "Shizhi_Goal", "Zhongzhi_Goal", "Wumingzhi_Goal", "Xiaomuzhi_Goal"
};

OneNetClient* OneNetClient::s_self = nullptr;

HandState::HandState() : mode(0), time(-1), error(-1), result(-1) {
    for (int i = 0; i < HAND_FINGERS; i++) {
        current[i] = 90;   // 上电中位
        goal[i]    = -1;
    }
}

// ═══════════════════════════════════════════════════
//  构造 / begin
// ═══════════════════════════════════════════════════

OneNetClient::OneNetClient()
    : _mqtt(_wifi)
    , _lastReconnectMs(0)
    , _pending(false)
{
    s_self = this;
}

void OneNetClient::begin() {
    _mqtt.setServer(MQTT_HOST, MQTT_PORT);
    _mqtt.setBufferSize(1024);
    _mqtt.setCallback(mqttCallback);
}

// ═══════════════════════════════════════════════════
//  loop / 连接维护
// ═══════════════════════════════════════════════════

void OneNetClient::loop() {
    if (_mqtt.connected()) {
        _mqtt.loop();
        return;
    }

    unsigned long now = millis();
    if (now - _lastReconnectMs < 10000) return;
    _lastReconnectMs = now;

    if (tryConnect()) {
        subscribe();
    }
}

bool OneNetClient::isConnected() {
    return _mqtt.connected();
}

void OneNetClient::subscribe() {
    String base = String("$sys/") + PRODUCT_ID + "/" + DEVICE_NAME;
    _mqtt.subscribe((base + "/thing/property/set").c_str());        // 云端属性设置
    _mqtt.subscribe((base + "/thing/property/set_reply").c_str());
    _mqtt.subscribe((base + "/thing/property/post/reply").c_str()); // 上报回执
    Serial.println("[OneNET] 已订阅属性下发");
}

// ═══════════════════════════════════════════════════
//  MQTT 连接
// ═══════════════════════════════════════════════════

bool OneNetClient::tryConnect() {
    String cid1 = String(DEVICE_NAME);
    String cid2 = String(PRODUCT_ID) + "&" + DEVICE_NAME;

    // 主用前端同款 2022-05-01 + SHA1；连不上再退回 2018-10-31 + SHA1
    for (int round = 0; round < 2; round++) {
        String token = (round == 0)
            ? generateToken(TOKEN_VERSION, TOKEN_METHOD)
            : generateToken("2018-10-31", "sha1");
        if (token.length() == 0) continue;

        if (_mqtt.connect(cid1.c_str(), PRODUCT_ID, token.c_str())) {
            Serial.printf("[OneNET] MQTT 已连接 (clientId=%s)\n", cid1.c_str());
            return true;
        }
        delay(300);
        if (_mqtt.connect(cid2.c_str(), PRODUCT_ID, token.c_str())) {
            Serial.printf("[OneNET] MQTT 已连接 (clientId=%s)\n", cid2.c_str());
            return true;
        }
        delay(300);
    }

    Serial.printf("[OneNET] MQTT 连接失败 (rc=%d)\n", _mqtt.state());
    return false;
}

// ═══════════════════════════════════════════════════
//  OneNET MQTT Token 生成（与前端 key.js 相同算法）
//  sign = Base64(HMAC-SHA1(Base64解码(access_key),
//         et + "\n" + method + "\n" + res + "\n" + version))
// ═══════════════════════════════════════════════════

String OneNetClient::generateToken(const char *version, const char *method) {
    time_t now;
    time(&now);
    if (now < 1000000000) return "";          // NTP 未同步

    long et = (long)now + 3600;               // 1 小时有效，重连时自动刷新
    String res = String("products/") + PRODUCT_ID + "/devices/" + DEVICE_NAME;
    String signStr = String(et) + "\n" + method + "\n" + res + "\n" + version;

    // Base64 解码产品密钥
    unsigned char keyBuf[96];
    size_t keyLen = 0;
    if (mbedtls_base64_decode(keyBuf, sizeof(keyBuf), &keyLen,
            (const unsigned char *)ACCESS_KEY, strlen(ACCESS_KEY)) != 0) return "";

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (md == nullptr) return "";

    unsigned char hmac[20];
    if (mbedtls_md_hmac(md, keyBuf, keyLen,
            (const unsigned char *)signStr.c_str(), signStr.length(), hmac) != 0) return "";

    // Base64 编码签名
    unsigned char signB64[64];
    size_t signB64Len = 0;
    if (mbedtls_base64_encode(signB64, sizeof(signB64), &signB64Len, hmac, sizeof(hmac)) != 0) return "";

    String sign;
    sign.reserve(signB64Len + 1);
    for (size_t i = 0; i < signB64Len; i++) {
        char c = (char)signB64[i];
        if (c != '\n' && c != '\r') sign += c;
    }

    String token;
    token.reserve(320);
    token  = "version=" + String(version);
    token += "&res="    + urlEncode(res);
    token += "&et="     + String(et);
    token += "&method=" + String(method);
    token += "&sign="   + urlEncode(sign);
    return token;
}

// ═══════════════════════════════════════════════════
//  上报：物模型标识符与前端 cloud.js 一致
// ═══════════════════════════════════════════════════

static void appendParam(String &payload, bool &first, const char *key, long value) {
    if (!first) payload += ',';
    first = false;
    payload += '"';
    payload += key;
    payload += "\":{\"value\":";
    payload += String(value);
    payload += '}';
}

bool OneNetClient::upload() {
    if (!_mqtt.connected()) return false;

    String payload;
    payload.reserve(768);
    payload = "{\"id\":\"" + String(millis()) + "\",\"version\":\"1.0\",\"params\":{";
    bool first = true;

    for (int i = 0; i < HAND_FINGERS; i++) {
        appendParam(payload, first, CURRENT_KEYS[i], _state.current[i]);
    }
    for (int i = 0; i < HAND_FINGERS; i++) {
        if (_state.goal[i] >= 0) appendParam(payload, first, GOAL_KEYS[i], _state.goal[i]);
    }
    if (_state.mode   >= 0) appendParam(payload, first, "mode", _state.mode);
    if (_state.time   >= 0) appendParam(payload, first, "time", _state.time);
    if (_state.error  >= 0) appendParam(payload, first, "Wuchayuanshizhi", _state.error);
    if (_state.result >= 0) appendParam(payload, first, "kangfu", _state.result);

    payload += "}}";

    String topic = String("$sys/") + PRODUCT_ID + "/" + DEVICE_NAME + "/thing/property/post";
    bool ok = _mqtt.publish(topic.c_str(), payload.c_str());
    Serial.printf("[OneNET] 上报%s: %s\n", ok ? "成功" : "失败", payload.c_str());
    return ok;
}

// ═══════════════════════════════════════════════════
//  云端下发解析
// ═══════════════════════════════════════════════════

bool OneNetClient::takeUpdate(HandState &out) {
    if (!_pending) return false;
    out = _state;
    _pending = false;
    return true;
}

// 标识符归一化：shizhi_goal -> 食指目标；zhong 兼容旧设备写法的 Zhong
static int fingerIndexFromKey(const String &k, bool *isGoal) {
    static const char *names[HAND_FINGERS] = {
        "damuzhi", "shizhi", "zhongzhi", "wumingzhi", "xiaomuzhi"
    };
    *isGoal = false;
    String base = k;
    if (base.endsWith("_goal")) {
        *isGoal = true;
        base = base.substring(0, base.length() - 5);
    }
    for (int i = 0; i < HAND_FINGERS; i++) {
        if (base == names[i]) return i;
    }
    if (base == "zhong") return 2;      // 兼容前端 KEY_ALIASES 里的 zhong
    return -1;
}

void OneNetClient::parsePropertySet(const String &msg) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, msg)) {
        Serial.println("[OneNET] 下发 JSON 解析失败");
        return;
    }

    JsonVariant params = doc["params"];
    if (!params.is<JsonObject>()) return;

    bool changed = false;

    for (JsonPair kv : params.as<JsonObject>()) {
        String key = kv.key().c_str();
        key.trim();

        JsonVariant v = kv.value();
        if (v.is<JsonObject>()) v = v["value"];       // 兼容 {"value":90} 写法
        if (v.isNull()) continue;

        long n = v.is<const char*>() ? atol(v.as<const char*>()) : v.as<long>();

        String k = key;
        k.toLowerCase();

        bool isGoal = false;
        int idx = fingerIndexFromKey(k, &isGoal);
        if (idx >= 0) {
            n = constrain(n, 0L, 180L);
            if (isGoal) {
                _state.goal[idx] = (int)n;
                _state.current[idx] = (int)n;         // 无回传时用目标值回显
            } else {
                _state.current[idx] = (int)n;
            }
            changed = true;
            continue;
        }

        if (k == "mode") {
            _state.mode = (int)constrain(n, 0L, 2L);
            changed = true;
        } else if (k == "time") {
            _state.time = n;
            changed = true;
        } else if (k == "wuchayuanshizhi") {
            _state.error = (int)constrain(n, 0L, 180L);
            changed = true;
        } else if (k == "kangfu") {
            _state.result = (int)constrain(n, 0L, 3L);
            changed = true;
        }
        // wupin / wupin_img / wupin_conf / grab_count / ObjectArchive
        // 属于物品档案，由云端/前端维护，ESP32 不处理
    }

    if (changed) {
        _pending = true;
        Serial.println("[OneNET] 已接收云端下发");
    }
}

void OneNetClient::handleMessage(char *topic, byte *payload, unsigned int length) {
    String msg;
    msg.reserve(length + 1);
    for (unsigned i = 0; i < length; i++) msg += (char)payload[i];

    String t = String(topic);
    Serial.printf("[OneNET] %s -> %s\n", topic, msg.c_str());

    if (t.endsWith("/thing/property/set")) {
        parsePropertySet(msg);
    }
}

void OneNetClient::mqttCallback(char *topic, byte *payload, unsigned int length) {
    if (s_self != nullptr) s_self->handleMessage(topic, payload, length);
}

// ═══════════════════════════════════════════════════
//  URL 编码
// ═══════════════════════════════════════════════════

String OneNetClient::urlEncode(const String &s) {
    String out;
    out.reserve(s.length() * 3);
    for (unsigned i = 0; i < s.length(); i++) {
        char c = s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            out += c;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            out += hex;
        }
    }
    return out;
}

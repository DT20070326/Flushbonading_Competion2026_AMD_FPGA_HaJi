#include <Arduino.h>
#include <WiFi.h>
#include "OneNetClient.h"

// ========== WiFi / OneNET 凭据 ==========
// 真实值放在 include/secrets.h（已 gitignore，不会提交到仓库）
// 首次使用请复制 include/secrets.example.h 为 include/secrets.h 并填写
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "缺少 ESP32/include/secrets.h，当前使用占位凭据，请复制 secrets.example.h 后填写真实值"
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

const char *ssid     = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// ========== 与 STM32 的串口（沿用原 KV260 链路）==========
// STM32 USART1: PA9=TX, PA10=RX，9600 8N1
// ESP32-S3: RX=18, TX=17
#define STM32_RX_PIN 18
#define STM32_TX_PIN 17
#define STM32_BAUD   9600

// ========== 心跳上报间隔（毫秒）==========
// 前端每 5 秒轮询一次；收到云端下发时会立即补发一次
const unsigned long UPLOAD_INTERVAL = 3000;

// ========== 全局对象 ==========
OneNetClient oneNet;
unsigned long lastUploadMs = 0;

// ========== 函数声明 ==========
void connectToWiFi();
void syncTime();
void sendToSTM32(const HandState &state);
void uploadState();

// ============================================================
//                        setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    // 串口：云端下发的五指角度从这里发给 STM32
    Serial1.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

    connectToWiFi();
    syncTime();

    oneNet.begin();
    lastUploadMs = millis();

    // 上电先让手回到中位（90°），与前端默认显示一致
    sendToSTM32(oneNet.state());

    Serial.println();
    Serial.println("====================================");
    Serial.println("  ESP32 OneNET 桥接终端");
    Serial.println("  云端下发五指角度 -> 串口 -> STM32");
    Serial.println("  手部状态 -> OneNET（与前端物模型一致）");
    Serial.println("====================================");
    Serial.println();
}

// ============================================================
//                        loop
// ============================================================
void loop() {
    // ── 1. OneNET MQTT 连接维护 ──
    oneNet.loop();

    // ── 2. 云端下发：解析后立即转发给 STM32，并回传一次 ──
    HandState remote;
    if (oneNet.takeUpdate(remote)) {
        sendToSTM32(remote);
        uploadState();
        lastUploadMs = millis();
    }

    // ── 3. 定时上报，并重发一次串口帧，保证 STM32 重启后也能同步 ──
    if (millis() - lastUploadMs >= UPLOAD_INTERVAL) {
        lastUploadMs = millis();
        sendToSTM32(oneNet.state());
        uploadState();
    }

    delay(20);
}

// ============================================================
//  上报当前状态到 OneNET
// ============================================================
void uploadState() {
    if (!oneNet.upload()) {
        Serial.println("[OneNET] 上报失败（MQTT 未连接？）");
    }
}

// ============================================================
//  云端数据 -> STM32
//  沿用 KV260 的 14 字节帧，STM32 的 KV260_Receive 可直接解析：
//  0xFF 'D'拇指 'S'食指 'Z'中指 'W'无名指 'X'小指 'Y'模式 0x59
//  有目标角度时下发目标角度，否则下发当前角度
// ============================================================
void sendToSTM32(const HandState &s) {
    int angle[HAND_FINGERS];
    for (int i = 0; i < HAND_FINGERS; i++) {
        angle[i] = (s.goal[i] >= 0) ? s.goal[i] : s.current[i];
        angle[i] = constrain(angle[i], 0, 180);
    }

    uint8_t frame[14];
    frame[0]  = 0xFF;
    frame[1]  = 'D';
    frame[2]  = (uint8_t)angle[0];              // 大拇指
    frame[3]  = 'S';
    frame[4]  = (uint8_t)angle[1];              // 食指
    frame[5]  = 'Z';
    frame[6]  = (uint8_t)angle[2];              // 中指
    frame[7]  = 'W';
    frame[8]  = (uint8_t)angle[3];              // 无名指
    frame[9]  = 'X';
    frame[10] = (uint8_t)angle[4];              // 小指
    frame[11] = 'Y';
    frame[12] = (uint8_t)constrain(s.mode >= 0 ? s.mode : 0, 0, 2);  // 模式，预留字节
    frame[13] = 0x59;

    Serial1.write(frame, sizeof(frame));

    Serial.printf("[串口] 下发 -> STM32: 拇指%d 食指%d 中指%d 无名指%d 小指%d, mode=%d\n",
                  angle[0], angle[1], angle[2], angle[3], angle[4],
                  (int)constrain(s.mode >= 0 ? s.mode : 0, 0, 2));
}

// ============================================================
//             连接 WiFi
// ============================================================
void connectToWiFi() {
    Serial.print("正在连接 WiFi...");
    WiFi.begin(ssid, password);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 40) {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("连接成功");
        Serial.print("IP 地址: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi 连接失败，请检查网络配置");
    }
}

// ============================================================
//          同步网络时间（生成 token 必须）
// ============================================================
void syncTime() {
    Serial.print("正在同步网络时间...");
    configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp.ntsc.ac.cn");

    int retry = 0;
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo) && retry < 20) {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (getLocalTime(&timeinfo)) {
        Serial.println("同步成功");
    } else {
        Serial.println("同步失败");
    }
}

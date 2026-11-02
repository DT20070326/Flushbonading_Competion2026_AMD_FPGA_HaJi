#ifndef ONENET_CLIENT_H
#define ONENET_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>

// OneNET MQTT token 可能超过 256 字节，必须加大
#define MQTT_MAX_PACKET_SIZE 1024
#include <PubSubClient.h>

// 与前端 HTML/utils/cloud.js 的物模型标识符保持一致
#define HAND_FINGERS 5

// 五指状态，-1 表示该字段暂无数据（上报时跳过）
struct HandState {
    int  current[HAND_FINGERS];   // 当前角度（Damuzhi/shizhi/Zhongzhi/wumingzhi/xiaomuzhi）
    int  goal[HAND_FINGERS];      // 目标角度（*_Goal），-1=非训练
    int  mode;                    // 0=正常 / 1=训练 / 2=录入
    long time;                    // 上次训练时长（秒）
    int  error;                   // 误差（度，Wuchayuanshizhi）
    int  result;                  // 训练效果 0~3（kangfu）

    HandState();
};

class OneNetClient {
public:
    OneNetClient();
    void begin();
    void loop();
    bool isConnected();

    // 把当前五指状态上报到 OneNET（标识符与前端一致）
    bool upload();

    // 取出云端最新下发；有新的下发返回 true
    bool takeUpdate(HandState &out);

    const HandState &state() const { return _state; }

private:
    WiFiClient     _wifi;
    PubSubClient   _mqtt;
    unsigned long  _lastReconnectMs;
    HandState      _state;
    volatile bool  _pending;      // 云端有下发待处理

    static OneNetClient *s_self;

    bool   tryConnect();
    void   subscribe();
    String generateToken(const char *version, const char *method);
    String urlEncode(const String &s);

    void handleMessage(char *topic, byte *payload, unsigned int length);
    void parsePropertySet(const String &msg);
    static void mqttCallback(char *topic, byte *payload, unsigned int length);

    static const char *MQTT_HOST;
    static const int   MQTT_PORT;
    static const char *PRODUCT_ID;
    static const char *DEVICE_NAME;
    static const char *ACCESS_KEY;    // 产品级密钥（与前端 utils/cloud.js 相同）
    static const char *TOKEN_VERSION;
    static const char *TOKEN_METHOD;
};

#endif

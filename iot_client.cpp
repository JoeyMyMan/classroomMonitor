#include "iot_client.h"
#include "config.h"
#include <ArduinoJson.h>
#include <esp_log.h>

static const char* TAG = "IoTClient";

IoTClient::IoTClient() : mqtt_client_(wifi_client_) {}

IoTClient::~IoTClient() {}

IoTClient& IoTClient::GetInstance() {
  static IoTClient instance;
  return instance;
}

void IoTClient::Init() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  ESP_LOGI(TAG, "Connecting to WiFi: %s ...", WIFI_SSID);

  mqtt_client_.setServer(IOT_BROKER, IOT_PORT);
  mqtt_client_.setBufferSize(2048);  // 扩容缓冲区以接收完整排行榜 JSON
  mqtt_client_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
    this->OnMqttMessage(topic, payload, length);
  });
}

bool IoTClient::IsMqttConnected() {
  return mqtt_client_.connected();
}

void IoTClient::CheckWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t now = millis();
    if (now - last_wifi_check_ms_ > 10000) {
      last_wifi_check_ms_ = now;
      ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
      WiFi.reconnect();
    }
  }
}

void IoTClient::CheckMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!mqtt_client_.connected()) {
    uint32_t now = millis();
    if (now - last_mqtt_reconnect_attempt_ms_ > 5000) {
      last_mqtt_reconnect_attempt_ms_ = now;
      ESP_LOGI(TAG, "Attempting MQTT connection to %s:%d ...", IOT_BROKER, IOT_PORT);

      bool connected = false;
      const char* user = strlen(IOT_USERNAME) > 0 ? IOT_USERNAME : nullptr;
      const char* pass = strlen(IOT_PASSWORD) > 0 ? IOT_PASSWORD : nullptr;

      connected = mqtt_client_.connect(IOT_CLIENT_ID, user, pass);

      if (connected) {
        ESP_LOGI(TAG, "MQTT connected successfully!");
#if USE_EASY_IOT
        mqtt_client_.subscribe(IOT_TOPIC_ALERT);
        mqtt_client_.subscribe(IOT_TOPIC_RANK);
        ESP_LOGI(TAG, "Subscribed to Easy IoT topics: %s, %s", IOT_TOPIC_ALERT, IOT_TOPIC_RANK);
#else
        mqtt_client_.subscribe(IOT_TOPIC_EVENT);
        ESP_LOGI(TAG, "Subscribed to MQTT topic: %s", IOT_TOPIC_EVENT);
#endif
        // 发布上线就绪消息
        PublishStatus("{\"status\":\"online\",\"device\":\"AI-VOX\",\"classroom\":\"" CLASSROOM_NAME "\"}");
      } else {
        ESP_LOGE(TAG, "MQTT connection failed, state: %d", mqtt_client_.state());
      }
    }
  }
}

void IoTClient::PublishStatus(const char* status_payload) {
  if (mqtt_client_.connected()) {
#if !USE_EASY_IOT
    mqtt_client_.publish(IOT_TOPIC_STATUS, status_payload);
#endif
    ESP_LOGI(TAG, "Status published: %s", status_payload);
  }
}

void IoTClient::OnMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  // 确保字符串结尾空字符
  std::string msg((char*)payload, length);
  ESP_LOGI(TAG, "Message received on topic [%s]: %s", topic, msg.c_str());

  // 尝试解析 JSON
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);

  if (err) {
    ESP_LOGW(TAG, "JSON parse error: %s, checking raw message", err.c_str());
    // 兼容简单非 JSON 纯文本指令
    if (msg.find("fight") != std::string::npos || msg.find("打架") != std::string::npos) {
      if (alert_cb_) alert_cb_("未知区域", "检测到肢体冲突！");
    } else if (msg.find("clear") != std::string::npos || msg.find("消警") != std::string::npos) {
      if (clear_cb_) clear_cb_();
    }
    return;
  }

  std::string type = doc["type"] | "";
  std::string event = doc["event"] | "";

  // 1. 打架冲突预警
  if (type == "alert" || event == "fight" || type == "fight") {
    std::string location = doc["location"] | "教室后排";
    std::string detail = doc["message"] | "检测到疑似打架推搡！";
    if (alert_cb_) {
      alert_cb_(location, detail);
    }
  }
  // 2. 流行语排名更新
  else if (type == "ranking" || type == "slang") {
    std::string title = doc["title"] | "四年级热词榜";
    std::vector<SlangItem> items;

    JsonArray arr = doc["items"].as<JsonArray>();
    for (JsonObject obj : arr) {
      SlangItem item;
      item.rank = obj["rank"] | (int)(items.size() + 1);
      item.word = obj["word"] | "";
      item.count = obj["count"] | 0;
      if (!item.word.empty()) {
        items.push_back(item);
      }
    }

    if (ranking_cb_) {
      ranking_cb_(title, items);
    }
  }
  // 3. 远程消警/复位
  else if (type == "clear" || type == "reset") {
    if (clear_cb_) {
      clear_cb_();
    }
  }
}

void IoTClient::Loop() {
  CheckWiFi();
  CheckMqtt();
  if (mqtt_client_.connected()) {
    mqtt_client_.loop();
  }
}

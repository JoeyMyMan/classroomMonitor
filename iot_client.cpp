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
  std::string msg((char*)payload, length);
  ESP_LOGI(TAG, "Message received on topic [%s]: %s", topic, msg.c_str());

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);

  if (err) {
    ESP_LOGW(TAG, "JSON parse error: %s, checking raw message", err.c_str());
    if (msg.find("fight") != std::string::npos || msg.find("打架") != std::string::npos) {
      if (alert_cb_) alert_cb_("监控区域", "检测到肢体冲突！");
    } else if (msg.find("clear") != std::string::npos || msg.find("消警") != std::string::npos || msg.find("normal") != std::string::npos) {
      if (clear_cb_) clear_cb_();
    }
    return;
  }

  // =========================================================================
  // 1. 打架冲突识别 (兼容 Leo6662233/meme 的 FightEngine state 与标准格式)
  // Leo6662233/meme 输出: {"fighting": true/false, "persons": N, "running": true}
  // =========================================================================
  if (doc.containsKey("fighting")) {
    bool is_fighting = doc["fighting"].as<bool>();
    int persons = doc["persons"] | 2;

    if (is_fighting) {
      std::string location = doc["location"] | "视觉监控区";
      std::string detail = doc["message"] | ("检测到 " + std::to_string(persons) + " 人推搡打架！");
      if (alert_cb_) {
        alert_cb_(location, detail);
      }
    } else {
      // 打架已平息或当前正常
      std::string status = "一切正常（检测到 " + std::to_string(persons) + " 个人）";
      if (idle_status_cb_) {
        idle_status_cb_(status);
      }
    }
    return;
  }

  std::string type = doc["type"] | "";
  std::string event = doc["event"] | "";

  // 标准打架预警
  if (type == "alert" || event == "fight" || type == "fight") {
    int persons = doc["persons"] | 2;
    std::string location = doc["location"] | "视觉监控区";
    std::string detail = doc["message"] | ("检测到 " + std::to_string(persons) + " 人推搡打架！");
    if (alert_cb_) {
      alert_cb_(location, detail);
    }
    return;
  }

  // =========================================================================
  // 2. 流行梗排行榜 (完全适配 Leo6662233/meme 的 /api/ranking 格式)
  // Leo6662233/meme 输出: {"ranking": [{"name": "你个老六", "count": 12, ...}]}
  // =========================================================================
  if (doc.containsKey("ranking") || doc.containsKey("items") || type == "ranking" || type == "meme" || type == "slang") {
    std::string title = doc["title"] | "四年级热梗排行榜";
    std::vector<SlangItem> items;

    JsonArray arr;
    if (doc["ranking"].is<JsonArray>()) {
      arr = doc["ranking"].as<JsonArray>();
    } else if (doc["items"].is<JsonArray>()) {
      arr = doc["items"].as<JsonArray>();
    } else if (doc.is<JsonArray>()) {
      arr = doc.as<JsonArray>();
    }

    for (JsonObject obj : arr) {
      SlangItem item;
      item.rank = obj["rank"] | (int)(items.size() + 1);
      // Leo6662233/meme 中梗名为 'name'，同时兼容 'word'
      const char* name_str = obj["name"] | (obj["word"] | "");
      item.word = name_str ? name_str : "";
      item.count = obj["count"] | 0;

      if (!item.word.empty()) {
        items.push_back(item);
      }
      if (items.size() >= 5) break;  // 屏幕展示前5名
    }

    if (!items.empty() && ranking_cb_) {
      ranking_cb_(title, items);
    }
    return;
  }

  // 3. 远程消警/复位
  if (type == "clear" || type == "reset" || doc["action"] == "dismiss") {
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

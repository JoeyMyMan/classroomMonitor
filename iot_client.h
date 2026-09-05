#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <functional>
#include <vector>
#include <string>
#include "display_ui.h"

class IoTClient {
 public:
  using AlertCallback = std::function<void(const std::string& location, const std::string& message)>;
  using RankingCallback = std::function<void(const std::string& title, const std::vector<SlangItem>& items)>;
  using ClearCallback = std::function<void()>;

  static IoTClient& GetInstance();

  void Init();
  void Loop();

  void SetAlertCallback(AlertCallback cb) { alert_cb_ = cb; }
  void SetRankingCallback(RankingCallback cb) { ranking_cb_ = cb; }
  void SetClearCallback(ClearCallback cb) { clear_cb_ = cb; }

  bool IsWiFiConnected() const { return WiFi.status() == WL_CONNECTED; }
  bool IsMqttConnected();

  void PublishStatus(const char* status_payload);

 private:
  IoTClient();
  ~IoTClient();

  void CheckWiFi();
  void CheckMqtt();
  void OnMqttMessage(char* topic, uint8_t* payload, unsigned int length);

  WiFiClient wifi_client_;
  PubSubClient mqtt_client_;

  AlertCallback alert_cb_;
  RankingCallback ranking_cb_;
  ClearCallback clear_cb_;

  uint32_t last_wifi_check_ms_ = 0;
  uint32_t last_mqtt_reconnect_attempt_ms_ = 0;
};

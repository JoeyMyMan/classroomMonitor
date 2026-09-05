#include <Arduino.h>
#include <ai_vox_engine.h>
#include "config.h"
#include "display_ui.h"
#include "audio_player.h"
#include "iot_client.h"
#include "led_strip.h"

static const char* TAG = "MainApp";

static led_strip_handle_t s_led_strip = nullptr;
static uint32_t s_state_start_ms = 0;
static uint32_t s_last_alert_beep_ms = 0;
static uint32_t s_last_led_blink_ms = 0;
static bool s_led_blink_state = false;

static bool s_last_wifi_connected = false;
static bool s_last_mqtt_connected = false;

void InitLed() {
  led_strip_config_t strip_cfg = {
      .strip_gpio_num = (gpio_num_t)kWs2812LedPin,
      .max_leds = 1,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      .flags = {
          .invert_out = false,
      },
  };

  led_strip_rmt_config_t rmt_cfg = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
      .mem_block_symbols = 0,
      .flags = {
          .with_dma = 0,
      },
  };

  if (led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led_strip) == ESP_OK) {
    led_strip_clear(s_led_strip);
    // 初始呼吸蓝
    led_strip_set_pixel(s_led_strip, 0, 0, 30, 80);
    led_strip_refresh(s_led_strip);
  }
}

void SetLed(uint8_t r, uint8_t g, uint8_t b) {
  if (s_led_strip) {
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
  }
}

void ResetToIdle() {
  DisplayUI::GetInstance().ShowIdle(
      IoTClient::GetInstance().IsWiFiConnected(),
      IoTClient::GetInstance().IsMqttConnected(),
      "班级守护中 · 秩序良好");
  SetLed(0, 40, 10);  // 柔和绿色
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("  AI-VOX 教室学生行为管理终端启动中...  ");
  Serial.println("==========================================");

  // 1. 初始化按键 (BOOT 键 GPIO 0)
  pinMode(kButtonBoot, INPUT_PULLUP);

  // 2. 初始化板载 RGB 灯
  InitLed();

  // 3. 初始化扬声器音频
  AudioPlayer::GetInstance().Init(DEFAULT_SPEAKER_VOL);

  // 4. 初始化 ST7789 屏幕与 LVGL UI
  DisplayUI::GetInstance().Init();
  DisplayUI::GetInstance().ShowBoot("系统初始化完成", "正在接入校园网络...");

  // 5. 注册 IoT 事件回调
  IoTClient::GetInstance().SetAlertCallback([](const std::string& loc, const std::string& msg) {
    Serial.printf("[ALERT] 打架冲突警报! 地点: %s, 详情: %s\n", loc.c_str(), msg.c_str());

    DisplayUI::GetInstance().ShowAlert(loc, msg);
    AudioPlayer::GetInstance().PlaySound(SoundType::kAlert);
    SetLed(255, 0, 0);  // 高亮红色警告

    s_state_start_ms = millis();
    s_last_alert_beep_ms = millis();
  });

  IoTClient::GetInstance().SetRankingCallback([](const std::string& title, const std::vector<SlangItem>& items) {
    Serial.printf("[RANKING] 收到流行语热度榜单: %s, 数量: %d\n", title.c_str(), (int)items.size());

    DisplayUI::GetInstance().ShowRanking(title, items);
    AudioPlayer::GetInstance().PlaySound(SoundType::kChime);
    SetLed(0, 120, 200);  // 活力青蓝色

    s_state_start_ms = millis();
  });

  IoTClient::GetInstance().SetClearCallback([]() {
    Serial.println("[CLEAR] 收到远程消警指令");
    AudioPlayer::GetInstance().PlaySound(SoundType::kDismiss);
    ResetToIdle();
  });

  // 6. 启动 WiFi 与 IoT 客户端
  IoTClient::GetInstance().Init();

  Serial.println("初始化全部完成，进入事件主循环！");
}

void loop() {
  // 1. 驱动 IoT 通信 (WiFi & MQTT 重连与消息收取)
  IoTClient::GetInstance().Loop();

  bool wifi_ok = IoTClient::GetInstance().IsWiFiConnected();
  bool mqtt_ok = IoTClient::GetInstance().IsMqttConnected();
  uint32_t now = millis();

  // 2. 网络状态变化检查与提示音
  if (wifi_ok && mqtt_ok && (!s_last_wifi_connected || !s_last_mqtt_connected)) {
    Serial.println("WiFi 与 MQTT 均已在线就绪！");
    AudioPlayer::GetInstance().PlaySound(SoundType::kConnect);
    if (DisplayUI::GetInstance().GetCurrentState() == UIState::kBoot) {
      ResetToIdle();
    }
  }
  s_last_wifi_connected = wifi_ok;
  s_last_mqtt_connected = mqtt_ok;

  UIState state = DisplayUI::GetInstance().GetCurrentState();

  // 3. 教师按键交互 (BOOT 键按下消警)
  if (digitalRead(kButtonBoot) == LOW) {
    delay(50);  // 简单防抖
    if (digitalRead(kButtonBoot) == LOW) {
      Serial.println("[BUTTON] 检测到教师按下 BOOT 按键！");
      if (state == UIState::kAlert) {
        Serial.println("老师现场确认并解除打架警报！");
        IoTClient::GetInstance().PublishStatus("{\"event\":\"alert_dismissed_by_teacher\",\"location\":\"local_button\"}");
        AudioPlayer::GetInstance().PlaySound(SoundType::kDismiss);
        ResetToIdle();
      } else if (state == UIState::kRanking) {
        // 在榜单状态按下可提前切回守护主屏
        ResetToIdle();
      }
      // 等待按键释放
      while (digitalRead(kButtonBoot) == LOW) {
        delay(10);
      }
    }
  }

  // 4. 冲突警报状态下的持续警觉逻辑 (声光循环与自动超时)
  if (state == UIState::kAlert) {
    // 红灯急促闪烁 (每 250ms 反转一次)
    if (now - s_last_led_blink_ms > 250) {
      s_last_led_blink_ms = now;
      s_led_blink_state = !s_led_blink_state;
      if (s_led_blink_state) {
        SetLed(255, 0, 0);
      } else {
        SetLed(60, 0, 0);
      }
    }

    // 每隔 6 秒再次急促鸣响警报
    if (now - s_last_alert_beep_ms > 6000) {
      s_last_alert_beep_ms = now;
      AudioPlayer::GetInstance().PlaySound(SoundType::kAlert);
    }

    // 超时自动消警 (默认 30 秒无操作恢复，避免死循环打扰课堂)
    if (now - s_state_start_ms > (ALERT_TIMEOUT_SEC * 1000)) {
      Serial.println("打架警报超时自动归档复位");
      ResetToIdle();
    }
  }

  // 5. 流行语榜单展示超时自动切回主屏
  else if (state == UIState::kRanking) {
    if (now - s_state_start_ms > (RANKING_DISPLAY_SEC * 1000)) {
      Serial.println("排行榜展示完毕，切回日常守护状态");
      ResetToIdle();
    }
  }

  delay(10);
}

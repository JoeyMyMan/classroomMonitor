#pragma once

#include <Arduino.h>

// ==========================================
// 1. 物联网平台配置 (IoT Platform Configuration)
// ==========================================
// 模式选择:
// 0: 使用 EMQX 公共 MQTT 服务器 (免注册账号，开箱即用)
// 1: 使用 DFRobot Easy IoT 物联网平台 (iot.dfrobot.com.cn)
#define USE_EASY_IOT 0

// WiFi 网络配置 (请修改为您现场的 WiFi 名称和密码)
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#if USE_EASY_IOT
  // DFRobot Easy IoT 平台配置 (网页: https://iot.dfrobot.com.cn/)
  #define IOT_BROKER       "iot.dfrobot.com.cn"
  #define IOT_PORT         1883
  #define IOT_CLIENT_ID    "ai_vox_grade4_terminal"
  #define IOT_USERNAME     "your_easy_iot_id"        // 替换为您的 Easy IoT 用户名
  #define IOT_PASSWORD     "your_easy_iot_password"  // 替换为您的 Easy IoT 密码
  #define IOT_TOPIC_ALERT  "your_topic_id_1"         // 用于接收冲突打架预警的主题
  #define IOT_TOPIC_RANK   "your_topic_id_2"         // 用于接收流行语排行版的主题
#else
  // 标准 MQTT (EMQX 公共服务器，可直接免费使用，也可填自建服务器)
  #define IOT_BROKER       "broker.emqx.io"
  #define IOT_PORT         1883
  #define IOT_CLIENT_ID    "aivox_grade4_classroom_01"
  #define IOT_USERNAME     ""
  #define IOT_PASSWORD     ""
  #define IOT_TOPIC_EVENT  "classroom/grade4/events" // 统一事件主题 (打架预警/流行语榜单)
  #define IOT_TOPIC_STATUS "classroom/grade4/status" // 终端心跳与消警反馈主题
#endif

// ==========================================
// 2. 硬件引脚配置 (Hardware Pinout Configuration)
// ==========================================
// 开发板型号: AI-VOX 3 (ESP32-S3)
// 屏幕: 240x240 ST7789 IPS LCD
constexpr int kLcdBacklightPin = 16;
constexpr int kSt7789Sda        = 21;  // SPI MOSI
constexpr int kSt7789Scl        = 17;  // SPI SCLK
constexpr int kSt7789Csx        = 15;  // SPI CS
constexpr int kSt7789Dcx        = 14;  // SPI DC
constexpr int kDisplayWidth     = 240;
constexpr int kDisplayHeight    = 240;

// 音频编解码芯片: ES8311 (I2C 控制 + I2S 音频)
constexpr int kI2cScl           = 12;  // ES8311 CCLK
constexpr int kI2cSda           = 13;  // ES8311 CDATA
constexpr int kEs8311I2cAddress = 0x30;

constexpr int kEs8311Mclk       = 11;  // I2S MCLK
constexpr int kEs8311Sclk       = 10;  // I2S BCLK / SCLK
constexpr int kEs8311Lrck       = 8;   // I2S WS / LRCK
constexpr int kEs8311Dsdin      = 7;   // I2S DOUT (DAC 数据输入)
constexpr int kEs8311Asdout     = 9;   // I2S DIN  (ADC 麦克风数据输出)

// 板载状态指示与交互
constexpr int kWs2812LedPin     = 41;  // 板载 WS2812 RGB LED
constexpr int kButtonBoot       = 0;   // 板载 BOOT 键 (用于消警)

// ==========================================
// 3. 业务参数配置 (Business Settings)
// ==========================================
#define CLASSROOM_NAME        "四年级(1)班 智慧教室"
#define DEFAULT_SPEAKER_VOL   85     // 扬声器初始音量 (0~100)
#define ALERT_TIMEOUT_SEC     30     // 打架警报超时自动复位时间 (秒)
#define RANKING_DISPLAY_SEC   15     // 流行语排行榜展示后切回守护界面的时间 (秒)

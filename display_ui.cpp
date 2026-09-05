#include "display_ui.h"
#include "config.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>
#include <esp_log.h>

#include "esp_lvgl_port.h"

static const char* TAG = "DisplayUI";

LV_FONT_DECLARE(font_puhui_16_4);

static esp_lcd_panel_io_handle_t s_panel_io = nullptr;
static esp_lcd_panel_handle_t s_panel = nullptr;
static lv_display_t* s_disp = nullptr;

DisplayUI::DisplayUI() : initialized_(false), current_state_(UIState::kBoot) {}

DisplayUI::~DisplayUI() {}

DisplayUI& DisplayUI::GetInstance() {
  static DisplayUI instance;
  return instance;
}

bool DisplayUI::Init() {
  if (initialized_) return true;

  // 1. 初始化屏幕背光引脚
  pinMode(kLcdBacklightPin, OUTPUT);
  digitalWrite(kLcdBacklightPin, HIGH);

  // 2. 初始化 SPI 总线 (SPI3_HOST)
  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = (gpio_num_t)kSt7789Sda;
  buscfg.miso_io_num = GPIO_NUM_NC;
  buscfg.sclk_io_num = (gpio_num_t)kSt7789Scl;
  buscfg.quadwp_io_num = GPIO_NUM_NC;
  buscfg.quadhd_io_num = GPIO_NUM_NC;
  buscfg.data4_io_num = GPIO_NUM_NC;
  buscfg.data5_io_num = GPIO_NUM_NC;
  buscfg.data6_io_num = GPIO_NUM_NC;
  buscfg.data7_io_num = GPIO_NUM_NC;
  buscfg.data_io_default_level = false;
  buscfg.max_transfer_sz = kDisplayWidth * kDisplayHeight * sizeof(uint16_t);
  buscfg.flags = 0;
  buscfg.isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO;
  buscfg.intr_flags = 0;

  esp_err_t err = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPI bus: %d", err);
    return false;
  }

  // 3. 配置 Panel IO
  esp_lcd_panel_io_spi_config_t io_cfg = {};
  io_cfg.cs_gpio_num = (gpio_num_t)kSt7789Csx;
  io_cfg.dc_gpio_num = (gpio_num_t)kSt7789Dcx;
  io_cfg.spi_mode = 0;
  io_cfg.pclk_hz = 40 * 1000 * 1000;
  io_cfg.trans_queue_depth = 10;
  io_cfg.lcd_cmd_bits = 8;
  io_cfg.lcd_param_bits = 8;

  err = esp_lcd_new_panel_io_spi(SPI3_HOST, &io_cfg, &s_panel_io);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create panel IO: %d", err);
    return false;
  }

  // 4. 配置 ST7789 驱动
  esp_lcd_panel_dev_config_t panel_cfg = {};
  panel_cfg.reset_gpio_num = -1;
  panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel_cfg.bits_per_pixel = 16;

  err = esp_lcd_new_panel_st7789(s_panel_io, &panel_cfg, &s_panel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create ST7789 panel: %d", err);
    return false;
  }

  esp_lcd_panel_reset(s_panel);
  esp_lcd_panel_init(s_panel);
  esp_lcd_panel_invert_color(s_panel, true);
  esp_lcd_panel_swap_xy(s_panel, false);
  esp_lcd_panel_mirror(s_panel, false, false);
  esp_lcd_panel_disp_on_off(s_panel, true);

  // 5. 初始化 LVGL 端口
  lv_init();
  lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
  port_cfg.task_priority = 3;
  port_cfg.timer_period_ms = 20;
  lvgl_port_init(&port_cfg);

  const lvgl_port_display_cfg_t disp_cfg = {
      .io_handle = s_panel_io,
      .panel_handle = s_panel,
      .control_handle = nullptr,
      .buffer_size = static_cast<uint32_t>(kDisplayWidth * 20),
      .double_buffer = false,
      .trans_size = 0,
      .hres = static_cast<uint32_t>(kDisplayWidth),
      .vres = static_cast<uint32_t>(kDisplayHeight),
      .monochrome = false,
      .rotation = {
          .swap_xy = false,
          .mirror_x = false,
          .mirror_y = false,
      },
      .color_format = LV_COLOR_FORMAT_RGB565,
      .flags = {
          .buff_dma = 1,
          .buff_spiram = 0,
          .sw_rotate = 0,
          .swap_bytes = 1,
          .full_refresh = 0,
      },
  };

  s_disp = lvgl_port_add_disp(&disp_cfg);
  if (!s_disp) {
    ESP_LOGE(TAG, "Failed to add LVGL display");
    return false;
  }

  initialized_ = true;
  ShowBoot("正在启动系统...", "WiFi连接中");
  return true;
}

void DisplayUI::ClearCurrentView() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_clean(scr);
}

void DisplayUI::ShowBoot(const char* message, const char* subtext) {
  if (!initialized_) return;
  if (!lvgl_port_lock(100)) return;

  current_state_ = UIState::kBoot;
  ClearCurrentView();

  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // 标题
  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "🏫 智慧教室终端");
  lv_obj_set_style_text_font(title, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 45);

  // 中心加载框
  lv_obj_t* card = lv_obj_create(scr);
  lv_obj_set_size(card, 200, 85);
  lv_obj_align(card, LV_ALIGN_CENTER, 0, 15);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x334155), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 12, 0);

  lv_obj_t* msg_label = lv_label_create(card);
  lv_label_set_text(msg_label, message ? message : "正在启动...");
  lv_obj_set_style_text_font(msg_label, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(msg_label, lv_color_white(), 0);
  lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 5);

  lv_obj_t* sub_label = lv_label_create(card);
  lv_label_set_text(sub_label, subtext ? subtext : "请稍候");
  lv_obj_set_style_text_font(sub_label, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(sub_label, lv_color_hex(0x94A3B8), 0);
  lv_obj_align(sub_label, LV_ALIGN_BOTTOM_MID, 0, -5);

  lvgl_port_unlock();
}

void DisplayUI::ShowIdle(bool wifi_ok, bool mqtt_ok, const char* status_msg) {
  if (!initialized_) return;
  if (!lvgl_port_lock(100)) return;

  current_state_ = UIState::kIdle;
  ClearCurrentView();

  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // 顶部状态栏
  lv_obj_t* top_bar = lv_obj_create(scr);
  lv_obj_set_size(top_bar, 220, 32);
  lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_border_width(top_bar, 0, 0);
  lv_obj_set_style_radius(top_bar, 8, 0);
  lv_obj_set_style_pad_all(top_bar, 4, 0);

  lv_obj_t* class_label = lv_label_create(top_bar);
  lv_label_set_text(class_label, "🏫 四年级(1)班");
  lv_obj_set_style_text_font(class_label, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(class_label, lv_color_hex(0x38BDF8), 0);
  lv_obj_align(class_label, LV_ALIGN_LEFT_MID, 6, 0);

  lv_obj_t* net_label = lv_label_create(top_bar);
  char net_str[32];
  snprintf(net_str, sizeof(net_str), "%s %s",
           wifi_ok ? "●WIFI" : "○WIFI",
           mqtt_ok ? "●IOT" : "○IOT");
  lv_label_set_text(net_label, net_str);
  lv_obj_set_style_text_font(net_label, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(net_label, (wifi_ok && mqtt_ok) ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
  lv_obj_align(net_label, LV_ALIGN_RIGHT_MID, -6, 0);

  // 中间日常守护卡片
  lv_obj_t* main_card = lv_obj_create(scr);
  lv_obj_set_size(main_card, 220, 135);
  lv_obj_align(main_card, LV_ALIGN_CENTER, 0, 6);
  lv_obj_set_style_bg_color(main_card, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_border_color(main_card, lv_color_hex(0x334155), 0);
  lv_obj_set_style_border_width(main_card, 1, 0);
  lv_obj_set_style_radius(main_card, 12, 0);

  // 状态图标/表情
  lv_obj_t* emoji = lv_label_create(main_card);
  lv_label_set_text(emoji, "😊");
  lv_obj_set_style_text_font(emoji, &font_puhui_16_4, 0);
  lv_obj_align(emoji, LV_ALIGN_TOP_MID, 0, 10);

  // 状态标题
  lv_obj_t* status_title = lv_label_create(main_card);
  lv_label_set_text(status_title, status_msg ? status_msg : "班级守护中 · 秩序良好");
  lv_obj_set_style_text_font(status_title, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(status_title, lv_color_hex(0x4ADE80), 0);
  lv_obj_align(status_title, LV_ALIGN_CENTER, 0, 5);

  // 副说明
  lv_obj_t* sub_desc = lv_label_create(main_card);
  lv_label_set_text(sub_desc, "待命监测冲突与热词");
  lv_obj_set_style_text_font(sub_desc, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(sub_desc, lv_color_hex(0x94A3B8), 0);
  lv_obj_align(sub_desc, LV_ALIGN_BOTTOM_MID, 0, -8);

  // 底部提示
  lv_obj_t* footer = lv_label_create(scr);
  lv_label_set_text(footer, "AI-VOX 智慧班级管家");
  lv_obj_set_style_text_font(footer, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(footer, lv_color_hex(0x64748B), 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);

  lvgl_port_unlock();
}

void DisplayUI::ShowAlert(const std::string& location, const std::string& detail) {
  if (!initialized_) return;
  if (!lvgl_port_lock(100)) return;

  current_state_ = UIState::kAlert;
  ClearCurrentView();

  lv_obj_t* scr = lv_screen_active();
  // 警戒红背景
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x991B1B), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // 警报横幅
  lv_obj_t* header = lv_label_create(scr);
  lv_label_set_text(header, "⚠️ 紧急冲突预警 ⚠️");
  lv_obj_set_style_text_font(header, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(header, lv_color_white(), 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 14);

  // 白色中央告警卡片
  lv_obj_t* card = lv_obj_create(scr);
  lv_obj_set_size(card, 220, 125);
  lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);
  lv_obj_set_style_bg_color(card, lv_color_white(), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xFECACA), 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_radius(card, 12, 0);

  // 位置信息
  lv_obj_t* loc_label = lv_label_create(card);
  std::string loc_str = "【" + (location.empty() ? std::string("教室后排") : location) + "】";
  lv_label_set_text(loc_label, loc_str.c_str());
  lv_obj_set_style_text_font(loc_label, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(loc_label, lv_color_hex(0xDC2626), 0);
  lv_obj_align(loc_label, LV_ALIGN_TOP_MID, 0, 5);

  // 冲突详情
  lv_obj_t* detail_label = lv_label_create(card);
  lv_label_set_text(detail_label, detail.empty() ? "发现学生推搡打架！" : detail.c_str());
  lv_obj_set_style_text_font(detail_label, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(detail_label, lv_color_hex(0x1F2937), 0);
  lv_obj_align(detail_label, LV_ALIGN_CENTER, 0, -4);

  // 提醒老师操作
  lv_obj_t* act_label = lv_label_create(card);
  lv_label_set_text(act_label, "❗ 请当堂老师即刻前往 ❗");
  lv_obj_set_style_text_font(act_label, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(act_label, lv_color_hex(0xB91C1C), 0);
  lv_obj_align(act_label, LV_ALIGN_BOTTOM_MID, 0, -5);

  // 底部消警提示
  lv_obj_t* btn_hint = lv_label_create(scr);
  lv_label_set_text(btn_hint, "👉 按下 BOOT 键解除警报");
  lv_obj_set_style_text_font(btn_hint, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(btn_hint, lv_color_hex(0xFEF08A), 0);
  lv_obj_align(btn_hint, LV_ALIGN_BOTTOM_MID, 0, -10);

  lvgl_port_unlock();
}

void DisplayUI::ShowRanking(const std::string& title, const std::vector<SlangItem>& items) {
  if (!initialized_) return;
  if (!lvgl_port_lock(100)) return;

  current_state_ = UIState::kRanking;
  ClearCurrentView();

  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // 顶部榜单标题
  lv_obj_t* header = lv_label_create(scr);
  std::string full_title = "🔥 " + (title.empty() ? std::string("四年级热词榜") : title);
  lv_label_set_text(header, full_title.c_str());
  lv_obj_set_style_text_font(header, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(header, lv_color_hex(0xFB923C), 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);

  // 排行榜列表容器
  lv_obj_t* list_cont = lv_obj_create(scr);
  lv_obj_set_size(list_cont, 224, 180);
  lv_obj_align(list_cont, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_set_style_bg_color(list_cont, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_border_color(list_cont, lv_color_hex(0x334155), 0);
  lv_obj_set_style_border_width(list_cont, 1, 0);
  lv_obj_set_style_radius(list_cont, 10, 0);
  lv_obj_set_style_pad_all(list_cont, 4, 0);
  lv_obj_set_scrollbar_mode(list_cont, LV_SCROLLBAR_MODE_OFF);

  size_t count = std::min(items.size(), (size_t)5);
  for (size_t i = 0; i < count; ++i) {
    lv_obj_t* row = lv_obj_create(list_cont);
    lv_obj_set_size(row, 212, 30);
    lv_obj_set_pos(row, 0, (int32_t)(i * 33));
    lv_obj_set_style_bg_color(row, (i % 2 == 0) ? lv_color_hex(0x1E293B) : lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 2, 0);

    // 排名徽章 (金银铜特显)
    lv_obj_t* rank_badge = lv_label_create(row);
    char rank_num[8];
    snprintf(rank_num, sizeof(rank_num), "%d.", (int)(i + 1));
    lv_label_set_text(rank_badge, rank_num);
    lv_obj_set_style_text_font(rank_badge, &font_puhui_16_4, 0);
    if (i == 0) {
      lv_obj_set_style_text_color(rank_badge, lv_color_hex(0xFBBF24), 0); // 金
    } else if (i == 1) {
      lv_obj_set_style_text_color(rank_badge, lv_color_hex(0x94A3B8), 0); // 银
    } else if (i == 2) {
      lv_obj_set_style_text_color(rank_badge, lv_color_hex(0xF97316), 0); // 铜
    } else {
      lv_obj_set_style_text_color(rank_badge, lv_color_hex(0x64748B), 0);
    }
    lv_obj_align(rank_badge, LV_ALIGN_LEFT_MID, 6, 0);

    // 流行语文字
    lv_obj_t* word_label = lv_label_create(row);
    lv_label_set_text(word_label, items[i].word.c_str());
    lv_obj_set_style_text_font(word_label, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(word_label, lv_color_white(), 0);
    lv_obj_align(word_label, LV_ALIGN_LEFT_MID, 28, 0);

    // 次数统计
    lv_obj_t* count_label = lv_label_create(row);
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d次", items[i].count);
    lv_label_set_text(count_label, count_str);
    lv_obj_set_style_text_font(count_label, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(count_label, LV_ALIGN_RIGHT_MID, -6, 0);
  }

  // 底部轮播说明
  lv_obj_t* footer = lv_label_create(scr);
  lv_label_set_text(footer, "班级热词实时同步");
  lv_obj_set_style_text_font(footer, &font_puhui_16_4, 0);
  lv_obj_set_style_text_color(footer, lv_color_hex(0x64748B), 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -4);

  lvgl_port_unlock();
}

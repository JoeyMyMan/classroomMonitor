#include "audio_player.h"
#include "config.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <cmath>
#include <vector>

#include <ai_vox_engine.h>
#include "audio_device/audio_device_es8311.h"

static const char* TAG = "AudioPlayer";

static i2c_master_bus_handle_t s_i2c_bus = nullptr;
static std::shared_ptr<ai_vox::AudioDeviceEs8311> s_audio_device;

AudioPlayer::AudioPlayer() : initialized_(false), volume_(DEFAULT_SPEAKER_VOL) {}

AudioPlayer::~AudioPlayer() {}

AudioPlayer& AudioPlayer::GetInstance() {
  static AudioPlayer instance;
  return instance;
}

bool AudioPlayer::Init(uint8_t volume) {
  if (initialized_) {
    return true;
  }
  volume_ = volume;

  // 1. 初始化 I2C 总线用于 ES8311 控制
  const i2c_master_bus_config_t i2c_cfg = {
      .i2c_port = I2C_NUM_1,
      .sda_io_num = (gpio_num_t)kI2cSda,
      .scl_io_num = (gpio_num_t)kI2cScl,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .intr_priority = 0,
      .trans_queue_depth = 0,
      .flags = {
          .enable_internal_pullup = 1,
          .allow_pd = 0,
      },
  };

  esp_err_t err = i2c_new_master_bus(&i2c_cfg, &s_i2c_bus);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize I2C master bus: %d", err);
    return false;
  }

  // 2. 初始化 ES8311 编解码器
  s_audio_device = std::make_shared<ai_vox::AudioDeviceEs8311>(
      s_i2c_bus,
      kEs8311I2cAddress,
      I2C_NUM_1,
      16000,
      (gpio_num_t)kEs8311Mclk,
      (gpio_num_t)kEs8311Sclk,
      (gpio_num_t)kEs8311Lrck,
      (gpio_num_t)kEs8311Asdout,
      (gpio_num_t)kEs8311Dsdin);

  if (!s_audio_device) {
    ESP_LOGE(TAG, "Failed to create ES8311 audio device");
    return false;
  }

  s_audio_device->OpenOutput(16000);
  s_audio_device->set_volume(volume_);
  initialized_ = true;
  ESP_LOGI(TAG, "ES8311 Audio initialized successfully, volume: %d", volume_);
  return true;
}

void AudioPlayer::SetVolume(uint8_t volume) {
  volume_ = volume > 100 ? 100 : volume;
  if (s_audio_device) {
    s_audio_device->set_volume(volume_);
  }
}

uint8_t AudioPlayer::GetVolume() const {
  return volume_;
}

void AudioPlayer::PlayTone(float freq, uint32_t duration_ms, float volume_scale) {
  if (!initialized_ || !s_audio_device) return;

  constexpr uint32_t kSampleRate = 16000;
  const size_t total_samples = (kSampleRate * duration_ms) / 1000;
  constexpr size_t kChunkSize = 256;
  int16_t buffer[kChunkSize];

  float phase_step = 2.0f * M_PI * freq / kSampleRate;
  float current_phase = 0.0f;
  const float max_amp = 32767.0f * volume_scale;

  size_t samples_written = 0;
  while (samples_written < total_samples) {
    size_t chunk = std::min(kChunkSize, total_samples - samples_written);
    for (size_t i = 0; i < chunk; ++i) {
      // 添加平滑包络防破音
      float env = 1.0f;
      size_t global_idx = samples_written + i;
      if (global_idx < 100) {
        env = (float)global_idx / 100.0f;
      } else if (total_samples - global_idx < 100) {
        env = (float)(total_samples - global_idx) / 100.0f;
      }
      buffer[i] = (int16_t)(max_amp * env * sinf(current_phase));
      current_phase += phase_step;
      if (current_phase >= 2.0f * M_PI) {
        current_phase -= 2.0f * M_PI;
      }
    }
    s_audio_device->Write(buffer, chunk);
    samples_written += chunk;
  }
}

void AudioPlayer::PlaySilence(uint32_t duration_ms) {
  if (!initialized_ || !s_audio_device) return;
  constexpr uint32_t kSampleRate = 16000;
  const size_t total_samples = (kSampleRate * duration_ms) / 1000;
  constexpr size_t kChunkSize = 256;
  int16_t buffer[kChunkSize] = {0};

  size_t samples_written = 0;
  while (samples_written < total_samples) {
    size_t chunk = std::min(kChunkSize, total_samples - samples_written);
    s_audio_device->Write(buffer, chunk);
    samples_written += chunk;
  }
}

void AudioPlayer::PlaySound(SoundType type) {
  if (!initialized_) return;

  switch (type) {
    case SoundType::kAlert:
      // 急促警报音: 880Hz 与 1760Hz 快速交替 3 遍
      for (int i = 0; i < 3; ++i) {
        PlayTone(880.0f, 120, 0.8f);
        PlaySilence(20);
        PlayTone(1760.0f, 150, 0.9f);
        PlaySilence(40);
      }
      break;

    case SoundType::kChime:
      // 欢快热词上榜和弦音 (C5, E5, G5, C6)
      PlayTone(523.25f, 90, 0.5f);   // C5
      PlayTone(659.25f, 90, 0.6f);   // E5
      PlayTone(783.99f, 110, 0.7f);  // G5
      PlayTone(1046.5f, 250, 0.8f);  // C6
      break;

    case SoundType::kDismiss:
      // 教师消警提示音 (温和下行音: 880Hz -> 587Hz)
      PlayTone(880.0f, 100, 0.6f);
      PlaySilence(20);
      PlayTone(587.33f, 180, 0.5f);
      break;

    case SoundType::kConnect:
      // 开机/网络连接就绪提示音 (双音连击: 600Hz -> 900Hz)
      PlayTone(600.0f, 80, 0.5f);
      PlaySilence(30);
      PlayTone(900.0f, 140, 0.6f);
      break;

    default:
      break;
  }
}

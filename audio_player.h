#pragma once

#include <Arduino.h>
#include <memory>
#include <cstdint>

enum class SoundType {
  kNone,
  kAlert,      // 打架冲突警报音
  kChime,      // 流行语榜单更新提示音
  kDismiss,    // 老师按键消警提示音
  kConnect     // 网络/MQTT连接成功音
};

class AudioPlayer {
 public:
  static AudioPlayer& GetInstance();

  bool Init(uint8_t volume = 85);
  void PlaySound(SoundType type);
  void SetVolume(uint8_t volume);
  uint8_t GetVolume() const;

 private:
  AudioPlayer();
  ~AudioPlayer();

  void PlayTone(float freq, uint32_t duration_ms, float volume_scale = 0.6f);
  void PlaySilence(uint32_t duration_ms);

  bool initialized_ = false;
  uint8_t volume_ = 85;
};

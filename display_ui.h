#pragma once

#include <Arduino.h>
#include <string>
#include <vector>

enum class UIState {
  kBoot,
  kIdle,
  kAlert,
  kRanking
};

struct SlangItem {
  int rank;
  std::string word;
  int count;
};

class DisplayUI {
 public:
  static DisplayUI& GetInstance();

  bool Init();
  void ShowBoot(const char* message, const char* subtext);
  void ShowIdle(bool wifi_ok, bool mqtt_ok, const char* status_msg = "班级守护中 · 秩序良好");
  void ShowAlert(const std::string& location, const std::string& detail);
  void ShowRanking(const std::string& title, const std::vector<SlangItem>& items);
  
  UIState GetCurrentState() const { return current_state_; }

 private:
  DisplayUI();
  ~DisplayUI();

  void ClearCurrentView();

  bool initialized_ = false;
  UIState current_state_ = UIState::kBoot;
};

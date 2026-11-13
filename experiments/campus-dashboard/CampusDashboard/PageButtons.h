#pragma once
#include <Arduino.h>

// 按钮按下时将 GPIO 拉低。内部上拉让松开时保持高电平。
class PageButtons {
 public:
  static constexpr uint8_t count = 2;

  void begin() {
    const uint32_t now = millis();
    for (auto& button : buttons) {
      pinMode(button.pin, INPUT_PULLUP);
      button.raw = button.stable = digitalRead(button.pin);
      button.changedAt = now;
    }
  }

  // 上一页 -1，下一页 +1，没有按键事件时返回 0。
  // 只识别稳定的按下边沿，长按不重复触发。
  int direction(uint32_t now) {
    int selected = 0;
    for (uint8_t i = 0; i < count; ++i) {
      auto& button = buttons[i];
      const bool reading = digitalRead(button.pin);
      if (reading != button.raw) {
        button.raw = reading;
        button.changedAt = now;
      }
      if (button.stable != button.raw && now - button.changedAt >= debounceMs) {
        button.stable = button.raw;
        if (button.stable == LOW && selected == 0) selected = i == 0 ? -1 : 1;
      }
    }
    return selected;
  }

 private:
  static constexpr uint32_t debounceMs = 30;
  struct Button {
    uint8_t pin;
    bool raw = HIGH;
    bool stable = HIGH;
    uint32_t changedAt = 0;
  };
  Button buttons[count] = {{6}, {7}};
};

#pragma once
#include <Arduino.h>

class Buttons {
 public:
  enum class Key : uint8_t { Back, Action, Up, Down, None };

  void begin() {
    for (auto& key : keys) {
      pinMode(key.pin, INPUT_PULLUP);
      key.raw = key.stable = digitalRead(key.pin);
    }
  }

  bool playHeld() const { return digitalRead(keys[3].pin) == LOW; }

  void consumeHeldPlay() {
    // 这次低电平已经判作抢跑，松手后才接受下一次按下。
    keys[3].raw = keys[3].stable = LOW;
    keys[3].changed = micros();
  }

  Key poll(uint32_t& pressedUs) {
    const uint32_t now = micros();
    Key event = Key::None;
    for (uint8_t i = 0; i < 4; ++i) {
      auto& key = keys[i];
      const bool raw = digitalRead(key.pin);
      if (raw != key.raw) {
        key.raw = raw;
        key.changed = now;
      }
      if (key.raw != key.stable && now - key.changed >= 30000UL) {
        key.stable = key.raw;
        if (key.stable == LOW && event == Key::None) {
          event = static_cast<Key>(i);
          // 消抖验证之后仍使用首次检测到低电平的时间。
          pressedUs = key.changed;
        }
      }
    }
    return event;
  }

 private:
  struct State {
    uint8_t pin;
    bool raw = HIGH;
    bool stable = HIGH;
    uint32_t changed = 0;
  };
  State keys[4] = {{4}, {5}, {6}, {7}};
};

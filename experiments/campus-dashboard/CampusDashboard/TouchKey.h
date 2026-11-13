#pragma once
#include "Config.h"

class TouchKey {
 public:
  void begin() {
    uint64_t sum = 0;
    for (int i = 0; i < 32; ++i) {
      sum += touchRead(Config::touchPin);
      delay(10);
    }
    baseline = sum / 32;
  }

  bool pressed(uint32_t now) {
    reading = touchRead(Config::touchPin);
    if (!baseline) return false;

    // 使用不同的按下和松开阈值，避免在临界点来回触发。
    const uint8_t percent = down ? Config::touchOffPercent : Config::touchOnPercent;
    const uint32_t threshold = baseline + baseline * percent / 100;
    const bool candidate = reading > threshold;
    if (candidate == down) {
      pending = false;
      return false;
    }
    if (!pending) {
      pending = true;
      since = now;
    }
    if (now - since < 40) return false;

    down = candidate;
    pending = false;
    return down;  // 只在按下边沿产生一次翻页事件。
  }

  void log() const {
    Serial.printf("GPIO4 raw=%lu baseline=%lu\n",
                  (unsigned long)reading, (unsigned long)baseline);
  }

 private:
  uint32_t baseline = 0, reading = 0, since = 0;
  bool down = false, pending = false;
};

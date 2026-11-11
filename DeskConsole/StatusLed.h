#pragma once
#include <Arduino.h>
#include "Config.h"

// 每个提示按时间步推进，不用 delay()，让主循环继续读键。
class StatusLed {
 public:
  enum class Cue { Boot, Connected, Scan, Saved, Result, FalseStart, Timeout, Record };

  void begin() {
    rgbLedWrite(Config::rgbPin, 0, 0, 0);
    play(Cue::Boot);
  }

  void play(Cue next) {
    cue = next;
    started = millis();
    active = true;
    update();
  }

  void stop() {
    active = false;
    write(0, 0, 0);
  }

  void update() {
    if (!active) return;
    constexpr uint8_t b = Config::rgbBrightness;
    const uint32_t ms = millis() - started;
    switch (cue) {
      case Cue::Boot:
      case Cue::Scan:
        if (ms < 280) return write(0, b / 2, b);
        break;
      case Cue::Connected:
      case Cue::Result:
        if (ms < 280) return write(0, b, 0);
        break;
      case Cue::Saved:
        if (ms < 250) return write(0, b, b);
        break;
      case Cue::FalseStart:
        if (ms < 480) return write(ms % 240 < 120 ? b : 0, 0, 0);
        break;
      case Cue::Timeout:
        if (ms < 450) return write(b, b / 2, 0);
        break;
      case Cue::Record: {
        if (ms >= 1300) break;
        const uint8_t step = (ms / 130) % 5;
        constexpr uint8_t colors[][3] = {
          {b, 0, 0}, {b, b / 2, 0}, {0, b, 0}, {0, b / 2, b}, {b / 2, 0, b}
        };
        return write(colors[step][0], colors[step][1], colors[step][2]);
      }
    }
    stop();
  }

 private:
  void write(uint8_t r, uint8_t g, uint8_t b) {
    if (r == red && g == green && b == blue) return;
    red = r;
    green = g;
    blue = b;
    rgbLedWrite(Config::rgbPin, red, green, blue);
  }

  Cue cue = Cue::Boot;
  uint32_t started = 0;
  bool active = false;
  uint8_t red = 0, green = 0, blue = 0;
};

// 统一的非阻塞动画时间轴。frame 返回本次需要绘制的帧序号，返回 -1 表示可以跳过绘制；经过时间使用无符号减法处理 millis 回绕。

#pragma once
#include <stdint.h>

// 跳过已错过的帧，动画时长由单调时钟决定，不阻塞按键扫描。
class FrameTimeline {
 public:
  void start(uint32_t now, uint16_t duration, uint8_t steps) {
    started = now;
    length = duration;
    total = steps;
    previous = 0;
    running = true;
  }
  int frame(uint32_t now) {
    if (!running) return -1;
    const uint32_t elapsed = now - started;
    const int next = elapsed >= length ? total : elapsed * total / length;
    if (next == previous) return -1;
    previous = next;
    if (next == total) stop();
    return next;
  }
  float progress(int step) const { return static_cast<float>(step) / total; }
  bool active() const { return running; }
  void stop() { running = false; }
 private:
  uint32_t started = 0;
  uint16_t length = 1;
  uint8_t total = 1, previous = 0;
  bool running = false;
};

inline float smoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

// The dim 270-degree track stays in place. Update only the arc segments
// crossed by the reading and let field() cache the text independently.
#include "Interface.h"
#include <math.h>

void Interface::environmentGauge(uint8_t index, float value) {
  constexpr int segments = 90, cy = 130, outer = 58, inner = 53;
  const bool temperature = index == 0;
  const int cx = temperature ? 96 : 224;
  const float low = temperature ? 0.0f : 950.0f;
  const float high = temperature ? 50.0f : 1050.0f;
  const uint16_t activeColor = temperature ? 0xFD68 : 0x5EFF;
  constexpr uint16_t trackColor = 0x4A49;
  const bool valid = isfinite(value);
  const int progress = valid ? lroundf(constrain((value - low) / (high - low), 0.0f, 1.0f) * segments) : 0;

  int& previous = environmentProgress[index];
  const int first = previous < 0 ? 0 : min(previous, progress);
  const int end = previous < 0 ? segments : max(previous, progress);
  auto point = [&](int step, int radius, int& x, int& y) {
    const float angle = (135 + step * 3) * (PI / 180.0f);
    x = cx + lroundf(radius * cosf(angle));
    y = cy + lroundf(radius * sinf(angle));
  };
  for (int segment = first; segment < end; ++segment) {
    int ax, ay, bx, by, ix, iy, jx, jy;
    point(segment, outer, ax, ay);
    point(segment + 1, outer, bx, by);
    point(segment, inner, ix, iy);
    point(segment + 1, inner, jx, jy);
    const uint16_t color = segment < progress ? activeColor : trackColor;
    screen.fillTriangle(ax, ay, bx, by, ix, iy, color);
    screen.fillTriangle(bx, by, ix, iy, jx, jy, color);
  }
  previous = progress;

  const uint8_t slot = index * 3;
  field(slot, cx - 45, 106, 90, 27, valid ? String(value, 1) : String("--"), 1, ILI9341_WHITE, true);
  field(slot + 1, cx - 34, 136, 68, 18, temperature ? "\x01" "C" : "hPa", 0, activeColor, true);
  field(slot + 2, cx - 35, 153, 70, 16, temperature ? "Temperature" : "Pressure", 4, muted, true);
}

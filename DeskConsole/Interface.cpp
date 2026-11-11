#include "Interface.h"
#include "NunitoFonts.h"
#include <WiFi.h>
#include <new>
#include <esp_ota_ops.h>
#include <time.h>

namespace {
constexpr uint16_t muted = 0xA69A;
constexpr uint16_t cardColor = 0x1928;
constexpr uint16_t cardOutline = 0x3A4E;
constexpr char degree = '\x01';
constexpr uint16_t sunColor = 0xFDC0;
constexpr uint16_t rainColor = 0x54FF;
constexpr int fireworkX = 40, fireworkY = 66, fireworkW = 240, fireworkH = 82;

void smallIcon(Adafruit_GFX& gfx, int x, int y, char kind, uint16_t color) {
  switch (kind) {
    case 'b':  // 蓝牙标志
      gfx.drawFastVLine(x, y - 8, 17, color);
      gfx.drawLine(x, y - 8, x + 5, y - 4, color);
      gfx.drawLine(x + 5, y - 4, x - 5, y + 5, color);
      gfx.drawLine(x - 5, y - 5, x + 5, y + 4, color);
      gfx.drawLine(x + 5, y + 4, x, y + 8, color);
      break;
    case 'w':  // Wi-Fi: 三道同心圆弧
      for (int r : {5, 8, 11}) {
        for (int dx = -r * 3 / 4; dx <= r * 3 / 4; ++dx) {
          gfx.drawPixel(x + dx, y + 5 - lroundf(sqrtf(r * r - dx * dx)), color);
        }
      }
      gfx.fillCircle(x, y + 5, 1, color);
      break;
    case 'c':  // 云
      gfx.fillCircle(x - 4, y, 4, color);
      gfx.fillCircle(x + 2, y - 3, 5, color);
      gfx.fillRoundRect(x - 8, y, 17, 5, 2, color);
      break;
    case 'h':  // 水滴
      gfx.drawLine(x, y - 7, x - 5, y, color);
      gfx.drawLine(x, y - 7, x + 5, y, color);
      gfx.drawRoundRect(x - 5, y - 1, 11, 8, 4, color);
      break;
    case 'v':  // 风
      gfx.drawFastHLine(x - 7, y - 4, 13, color);
      gfx.drawFastHLine(x - 5, y, 13, color);
      gfx.drawFastHLine(x - 8, y + 4, 11, color);
      break;
    case 'p':  // 压力
      gfx.drawCircle(x, y, 6, color);
      gfx.drawLine(x, y, x + 3, y - 3, color);
      break;
    case 't':  // 温度计
      gfx.drawRoundRect(x - 2, y - 7, 5, 11, 2, color);
      gfx.fillCircle(x, y + 5, 3, color);
      break;
    case 's':  // 统计 / 信号
      for (int i = 0; i < 3; ++i) gfx.fillRect(x - 6 + 5 * i, y + 5 - 4 * i, 3, 3 + 4 * i, color);
      break;
    case 'f':  // 方格旗
      gfx.drawFastVLine(x - 6, y - 7, 15, color);
      gfx.drawRect(x - 5, y - 7, 12, 9, color);
      gfx.fillRect(x - 4, y - 6, 4, 3, color);
      gfx.fillRect(x, y - 3, 4, 3, color);
      break;
    default:  // 芯片 / 存储
      gfx.drawRect(x - 5, y - 5, 11, 11, color);
      for (int dy : {-4, 0, 4}) {
        gfx.drawFastHLine(x - 8, y + dy, 3, color);
        gfx.drawFastHLine(x + 6, y + dy, 3, color);
      }
      break;
  }
}

void weatherIcon(Adafruit_GFX& gfx, int x, int y, int code) {
  if (code < 0) return;
  if (code <= 2) {
    const int sx = code == 0 ? x : x - 8;
    gfx.fillCircle(sx, y - 6, 8, sunColor);
    for (int d : {-1, 1}) {
      gfx.drawFastHLine(sx + (d < 0 ? -14 : 11), y - 6, 4, sunColor);
      gfx.drawFastVLine(sx, y - 6 + (d < 0 ? -14 : 11), 4, sunColor);
      gfx.drawLine(sx + d * 8, y - 14, sx + d * 11, y - 17, sunColor);
    }
  }
  if (code == 0) return;
  if (code == 45 || code == 48) {
    for (int i = -1; i <= 1; ++i) {
      gfx.fillRoundRect(x - 14 + (i + 1) * 2, y + i * 6, 24, 3, 1, 0xCE79);
    }
    return;
  }
  gfx.fillCircle(x - 9, y, 7, ILI9341_WHITE);
  gfx.fillCircle(x + 1, y - 4, 9, ILI9341_WHITE);
  gfx.fillCircle(x + 10, y + 1, 6, ILI9341_WHITE);
  gfx.fillRoundRect(x - 15, y, 30, 8, 4, ILI9341_WHITE);
  const bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  const bool storm = code >= 95;
  if (snow) {
    for (int dx : {-8, 0, 8}) gfx.fillCircle(x + dx, y + 13, 2, ILI9341_WHITE);
  } else if (storm || (code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    for (int dx : {-10, 9}) {
      gfx.drawLine(x + dx, y + 10, x + dx - 3, y + 17, rainColor);
    }
  }
  if (storm) {
    gfx.fillTriangle(x + 2, y + 5, x - 4, y + 13, x + 4, y + 13, sunColor);
    gfx.fillTriangle(x, y + 11, x + 6, y + 11, x - 1, y + 19, sunColor);
  }
}

String number(float value, int decimals = 0) {
  return isnan(value) ? String("--") : String(value, decimals);
}

String temperature(float value) {
  return number(value, 1) + degree + "C";
}

String duration(uint32_t us) {
  return us ? String(us / 1000.0f, 1) + " ms" : String("--");
}

const char* condition(int code) {
  if (code < 0) return "Weather pending";
  if (code == 0) return "Clear";
  if (code <= 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code <= 48) return "Fog";
  if (code <= 57) return "Drizzle";
  if (code <= 67) return "Rain";
  if (code <= 77 || code == 85 || code == 86) return "Snow";
  if (code <= 82) return "Showers";
  if (code >= 95) return "Thunderstorm";
  return "Weather unknown";
}
}

void Interface::begin() {
  SPI.begin(Config::sck, -1, Config::mosi, Config::cs);
  screen.begin(10000000);
  screen.setRotation(Config::rotation == 3 ? 3 : 1);
  screen.setTextWrap(false);
}

void Interface::setDisplayOn(bool on) {
  // ILI9341 保留显存。LED 背光脚独立供电，不受这条命令控制。
  screen.sendCommand(on ? ILI9341_DISPON : ILI9341_DISPOFF);
}

void Interface::beginPage(const char* title, uint16_t base, uint16_t color) {
  fireworks.reset();
  // 真正切页才清屏。换页后所有绘制缓存需要重新建立。
  background = base;
  highlight = color;
  for (auto& text : cache) text = "";
  lastLights = -1;
  lastSelection = -1;
  launcherTop = 0;
  panelsReady = false;
  for (auto& bar : deviceBars) bar = -1;
  lastWeatherCode = -100;
  for (auto& code : forecastIcons) code = -100;
  for (auto& label : tabs) label = nullptr;
  screen.fillScreen(background);
  if (*title) {
    if (strcmp(title, "Apps") == 0) {
      for (int dx : {0, 8}) {
        for (int dy : {0, 8}) screen.drawRoundRect(47 + dx, 13 + dy, 6, 6, 2, highlight);
      }
      field(9, 71, 6, 180, 30, "APPS", 1, highlight);
    } else {
      const char icon = strcmp(title, "Wi-Fi") == 0 ? 'w' :
                        strcmp(title, "Bluetooth") == 0 ? 'b' :
                        strncmp(title, "Weather", 7) == 0 ? 'c' :
                        strcmp(title, "F1 REACTION") == 0 ? 'f' :
                        strcmp(title, "F1 Statistics") == 0 ? 's' : 'm';
      smallIcon(screen, 54, 22, icon, highlight);
      field(9, 72, 7, 208, 29, title, 1, highlight);
    }
  } else {
    screen.drawFastHLine(84, 160, 152, muted);
  }
}

void Interface::tab(bool right, bool lower, const char* label) {
  tabs[(right ? 2 : 0) + (lower ? 1 : 0)] = label;
}

void Interface::animateTabs(bool entering, uint8_t mask) {
  bool any = false;
  for (uint8_t i = 0; i < 4; ++i) any |= (mask & (1U << i)) && tabs[i];
  if (!any) return;

  constexpr int width = 24, body = 21, maxHeight = 64;
  GFXcanvas16 patch(width, maxHeight);
  GFXcanvas1 letters(48, 14);
  if (!patch.getBuffer() || !letters.getBuffer()) return;
  letters.setTextWrap(false);
  letters.setTextColor(1);
  letters.setFont(&NunitoTab);

  // 退场先慢后快。入场略过目标位置，再回弹至最终位置。
  constexpr int slideOut[] = {0, 1, 3, 7, 12, 18, 21};
  constexpr int slideIn[] = {21, 15, 8, 2, -2, -1, 0};
  for (int frame = 0; frame < 7; ++frame) {
    const int offset = entering ? slideIn[frame] : slideOut[frame];
    for (uint8_t i = 0; i < 4; ++i) {
      if (!(mask & (1U << i))) continue;
      const char* label = tabs[i];
      if (!label) continue;
      const bool right = i >= 2;
      const bool arrow = label[0] == '^' || label[0] == 'v';
      const bool minus = strcmp(label, "-") == 0;
      const bool apps = strcmp(label, "APPS") == 0;
      const bool back = strcmp(label, "BACK") == 0;
      const bool hasIcon = apps || back;
      int16_t textX, textY;
      uint16_t length, textHeight;
      letters.getTextBounds(label, 0, 0, &textX, &textY, &length, &textHeight);
      const int h = arrow || minus ? 22 : length + (hasIcon ? 20 : 8);
      const int x = right ? width - body + offset : -offset;
      const int cx = x + body / 2;
      const int center = h / 2;
      patch.fillScreen(background);
      if (offset < body) {
        patch.drawRoundRect(right ? x : x - 7, 0, body + 7, h, 7, highlight);
        if (arrow) {
          const int direction = label[0] == '^' ? -1 : 1;
          patch.drawLine(cx - 5, center - direction * 3, cx, center + direction * 3, ILI9341_WHITE);
          patch.drawLine(cx, center + direction * 3, cx + 5, center - direction * 3, ILI9341_WHITE);
        } else if (minus) {
          patch.drawFastHLine(cx - 4, center, 9, ILI9341_WHITE);
        } else {
          if (apps) {
            for (int dy : {-4, 1}) {
              for (int dx : {-4, 1}) patch.drawRect(cx + dx, 9 + dy, 3, 3, highlight);
            }
          } else if (back) {
            patch.drawFastHLine(cx - 4, 9, 9, highlight);
            patch.drawLine(cx - 4, 9, cx, 6, highlight);
            patch.drawLine(cx - 4, 9, cx, 12, highlight);
          }
          letters.fillScreen(0);
          letters.setCursor(-textX, -textY);
          letters.print(label);
          const int middle = center + (hasIcon ? 6 : 0);
          for (int u = 0; u < length; ++u) {
            for (int v = 0; v < textHeight; ++v) {
              if (letters.getPixel(u, v)) {
                patch.drawPixel(cx + (right ? textHeight / 2 - v : v - textHeight / 2),
                                middle + (right ? u - length / 2 : length / 2 - u),
                                ILI9341_WHITE);
              }
            }
          }
        }
      }
      const int y = (i % 2 ? 179 : 58) - h / 2;
      // 只传输本标签的矩形像素，中央内容保持原样。
      screen.drawRGBBitmap(right ? 320 - width : 0, y, patch.getBuffer(), width, h);
    }
    if (frame < 6) delay(12);
  }
  if (!entering) {
    for (uint8_t i = 0; i < 4; ++i) {
      if (mask & (1U << i)) tabs[i] = nullptr;
    }
  }
}

void Interface::launcher(uint8_t selected, const char* const (&labels)[5]) {
  constexpr int cardX = 46, firstY = 49, width = 228, height = 36, pitch = 43;
  constexpr int viewportEnd = firstY + 4 * pitch;
  constexpr int count = 5;
  if (lastSelection == selected) return;
  int nextTop = launcherTop;
  if (selected < nextTop) nextTop = selected;
  if (selected >= nextTop + 4) nextTop = selected - 3;

  GFXcanvas16 frame(width, pitch);
  GFXcanvas1 shape(width, height);
  GFXcanvas1 ink(width, height);
  if (!frame.getBuffer() || !shape.getBuffer() || !ink.getBuffer()) return;
  shape.fillScreen(0);
  shape.fillRoundRect(0, 0, width, height, 8, 1);
  ink.setTextWrap(false);
  ink.setTextColor(1);

  auto paint = [&](int logicalRow, int highlightY, int scroll) {
    const int y = firstY + logicalRow * pitch;
    const int screenY = y - scroll;
    const int top = max(screenY, firstY);
    const int end = min(screenY + pitch, viewportEnd);
    if (top >= end) return;
    const int row = (logicalRow + count) % count;
    ink.fillScreen(0);
    constexpr int x = 21, cy = 18;
    if (row == 0) {
      ink.drawFastVLine(x - 6, cy - 7, 15, 1);
      ink.drawRect(x - 5, cy - 7, 13, 9, 1);
      ink.fillRect(x - 4, cy - 6, 4, 3, 1);
      ink.fillRect(x, cy - 3, 4, 3, 1);
    } else if (row == 1) {
      ink.fillCircle(x - 5, cy, 5, 1);
      ink.fillCircle(x + 2, cy - 3, 6, 1);
      ink.fillRoundRect(x - 8, cy, 19, 6, 3, 1);
    } else if (row == 2) {
      // 三道同心圆弧，圆心在 Wi-Fi 圆点处。
      constexpr int dotY = cy + 7;
      for (int radius : {6, 10, 14}) {
        const int limit = radius * 3 / 4;
        for (int dx = -limit; dx <= limit; ++dx) {
          const int arcY = dotY - lroundf(sqrtf(radius * radius - dx * dx));
          ink.drawFastVLine(x + dx, arcY, 2, 1);
        }
      }
      ink.fillCircle(x, dotY, 2, 1);
    } else if (row == 3) {
      smallIcon(ink, x, cy, 'b', 1);
    } else {
      ink.drawRect(x - 6, cy - 6, 13, 13, 1);
      for (int n : {-4, 0, 4}) {
        ink.drawFastHLine(x - 9, cy + n, 3, 1);
        ink.drawFastHLine(x + 7, cy + n, 3, 1);
      }
    }
    ink.setFont(&NunitoMedium);
    ink.setCursor(47, 25);
    ink.print(labels[row]);

    // 高亮在整条列表上移动，卡片形状充当遮罩。字和图标用同一个遮罩反色。
    uint16_t* pixels = frame.getBuffer();
    for (int py = 0; py < pitch; ++py) {
      const int highlightRow = y + py - highlightY;
      for (int px = 0; px < width; ++px) {
        const bool lit = highlightRow >= 0 && highlightRow < height && shape.getPixel(px, highlightRow);
        pixels[py * width + px] = py >= height || !shape.getPixel(px, py) ? background :
            ink.getPixel(px, py) ? (lit ? ILI9341_BLACK : ILI9341_WHITE) :
            lit ? highlight : 0x2167;
      }
    }
    // 逐行裁掉视口以外的部分，标题和两侧按键不参与滚动。
    screen.drawRGBBitmap(cardX, top, pixels + (top - screenY) * width, width, end - top);
  };

  if (lastSelection < 0) {
    for (int row = 0; row < count; ++row) paint(row, firstY + selected * pitch, nextTop * pitch);
  } else {
    const int from = firstY + lastSelection * pitch;
    const bool wrapDown = lastSelection == count - 1 && selected == 0;
    const bool wrapUp = lastSelection == 0 && selected == count - 1;
    const int cycle = wrapDown ? count : wrapUp ? -count : 0;
    const int distance = static_cast<int>(selected) + cycle - lastSelection;
    const int scrollTarget = nextTop + cycle;
    const bool scrolling = scrollTarget != launcherTop;
    const int steps = cycle ? 8 : scrolling ? 5 : 9;
    for (int step = 1; step <= steps; ++step) {
      const float t = static_cast<float>(step) / steps;
      const float eased = t * t * (3.0f - 2.0f * t);
      const int pos = from + lroundf(distance * pitch * eased);
      const int scroll = lroundf((launcherTop + (scrollTarget - launcherTop) * eased) * pitch);
      if (scrolling) {
        // 跨首尾时借用相邻一轮的卡片，保持滚动方向与按键一致。
        const int start = wrapUp ? -count : 0;
        const int end = wrapDown ? 2 * count : count;
        for (int row = start; row < end; ++row) paint(row, pos, scroll);
      } else {
        paint(lastSelection, pos, scroll);
        paint(selected, pos, scroll);
      }
    }
  }
  lastSelection = selected;
  launcherTop = nextTop;
}

void Interface::field(uint8_t slot, int x, int y, int w, int h, String value,
                      uint8_t size, uint16_t color, bool centered, bool onCard) {
  // 动态字段只能写在两侧按键标签之间。
  const int end = min(x + w, 288);
  x = max(x, 32);
  w = end - x;
  if (w <= 0) return;
  if (cache[slot] == value) return;
  cache[slot] = value;
  const GFXfont* font = size == 4 ? &NunitoHint : size == 3 ? &NunitoTab : size == 2 ? &NunitoClock :
                       size == 1 ? &NunitoMedium : &NunitoSmall;
  screen.setFont(font);
  String measured = value;
  measured.replace(degree, 'o');
  int16_t x1, y1;
  uint16_t tw, th;
  screen.getTextBounds(measured.c_str(), 0, 0, &x1, &y1, &tw, &th);
  while (tw > w && value.length()) {
    value.remove(value.length() - 1);
    measured.remove(measured.length() - 1);
    screen.getTextBounds(measured.c_str(), 0, 0, &x1, &y1, &tw, &th);
  }
  screen.fillRect(x, y, w, h, onCard ? cardColor : background);
  screen.setTextColor(color);
  screen.setCursor(x - x1 + (centered ? (w - tw) / 2 : 0), y - y1 + (h - th) / 2);
  const int marker = value.indexOf(degree);
  if (marker < 0) {
    screen.print(value);
    return;
  }
  screen.print(value.substring(0, marker));
  const int px = screen.getCursorX();
  const int baseline = screen.getCursorY();
  const GFXglyph& glyph = font->glyph['o' - font->first];
  const GFXglyph& cap = font->glyph['C' - font->first];
  screen.drawCircle(px + glyph.xAdvance / 2, baseline + cap.yOffset + 2, 2, color);
  screen.setCursor(px + glyph.xAdvance, baseline);
  screen.print(value.substring(marker + 1));
}

void Interface::home(const WeatherData& data) {
  tm local;
  char clock[9] = "--:--:--";
  char date[28] = "Waiting for time";
  if (getLocalTime(&local, 10)) {
    strftime(clock, sizeof(clock), "%H:%M:%S", &local);
    strftime(date, sizeof(date), "%a, %d %b %Y", &local);
  }
  // 固定时分秒的文字区，秒变化时只刷新右边一格。
  field(0, 44, 45, 62, 71, String(clock).substring(0, 2), 2, ILI9341_WHITE, true);
  field(1, 110, 45, 18, 71, ":", 2, ILI9341_WHITE, true);
  field(2, 129, 45, 62, 71, String(clock).substring(3, 5), 2, ILI9341_WHITE, true);
  field(3, 192, 45, 18, 71, ":", 2, ILI9341_WHITE, true);
  field(4, 214, 45, 62, 71, String(clock).substring(6, 8), 2, ILI9341_WHITE, true);
  field(5, 18, 123, 284, 31, date, 1, ILI9341_WHITE, true);
  if (lastWeatherCode != data.code) {
    screen.fillRect(46, 171, 41, 40, background);
    weatherIcon(screen, 68, 191, data.code);
    lastWeatherCode = data.code;
  }
  field(6, 89, 172, 176, 35, String(condition(data.code)) + "  |  " + temperature(data.temp),
        0, highlight, true);
}

void Interface::game(const Game& game) {
  using State = Game::State;
  if (fireworks && game.state != State::Result) stopFireworks();
  if (lastLights != game.lights) {
    for (int i = 0; i < 5; ++i) {
      const int x = 42 + i * 48;
      if (lastLights < 0) {
        screen.fillRoundRect(x, 68, 44, 78, 9, 0x2104);
        screen.drawRoundRect(x, 68, 44, 78, 9, 0x39E7);
        screen.fillCircle(x + 22, 88, 13, 0x4A49);
      }
      if (lastLights < 0 || (i < game.lights) != (i < lastLights)) {
        screen.fillCircle(x + 22, 126, 13, i < game.lights ? ILI9341_RED : 0x4A49);
      }
    }
    lastLights = game.lights;
  }
  String message;
  String corrected;
  String hint;
  switch (game.state) {
    case State::Idle: message = "Press to start"; break;
    case State::Lighting:
    case State::Waiting: message = "Wait for lights out"; break;
    case State::GoPending:
    case State::Go: message = game.debug && game.state == State::Go ?
        "RAW " + duration(game.liveUs()) : String("GO!"); break;
    case State::Result:
      message = game.debug ? "RAW " + duration(game.resultUs) :
                             String(game.correctedMs(game.resultUs), 1) + " ms";
      if (game.debug) corrected = "CAL " + String(game.correctedMs(game.resultUs), 1) + " ms";
      hint = "Press PLAY to reset";
      break;
    case State::FalseStart: message = "FALSE START"; hint = "Press PLAY to reset"; break;
    case State::Timeout: message = "TIMEOUT"; hint = "Press PLAY to reset"; break;
  }
  field(0, 38, 150, 244, 31, message, 1,
        game.state == State::FalseStart ? ILI9341_RED : ILI9341_WHITE, true);
  gameMode(game.debug);
  field(2, 38, 183, 244, 22, corrected, 0, highlight, true);
  field(3, 38, 207, 244, 19, hint, 4, muted, true);
  field(4, 38, 226, 244, 14, game.saved() ? "" : "Flash save unavailable", 3, muted, true);
}

void Interface::startFireworks() {
  fireworks.reset(new (std::nothrow) GFXcanvas16(fireworkW, fireworkH));
  if (!fireworks || !fireworks->getBuffer()) {
    fireworks.reset();
    return;
  }
  fireworkStarted = millis();
  lastFireworkFrame = fireworkStarted - 40;
}

void Interface::stopFireworks() {
  fireworks.reset();
  screen.fillRect(fireworkX, fireworkY, fireworkW, fireworkH, background);
  lastLights = -1;
}

void Interface::updateFireworks(const Game& game) {
  if (!fireworks) return;
  const uint32_t now = millis();
  const uint32_t elapsed = now - fireworkStarted;
  if (elapsed >= 1350) {
    stopFireworks();
    this->game(game);
    return;
  }
  if (now - lastFireworkFrame < 40) return;
  lastFireworkFrame = now;

  // 只合成灯箱区域。三个烟花错峰展开，粒子减速并向下坠落。
  auto& frame = *fireworks;
  frame.fillScreen(background);
  constexpr int centers[] = {47, 120, 193};
  constexpr uint16_t colors[] = {ILI9341_YELLOW, 0xF94E, 0x65FF};
  for (int burst = 0; burst < 3; ++burst) {
    const int age = static_cast<int>(elapsed) - burst * 165;
    if (age < 0 || age > 850) continue;
    const float t = age / 850.0f;
    const float radius = 37.0f * t * (2.0f - t);
    for (int ray = 0; ray < 12; ++ray) {
      const float angle = ray * 0.523599f + burst * 0.24f;
      const float dx = cosf(angle), dy = sinf(angle);
      const int x = centers[burst] + lroundf(dx * radius);
      const int y = 33 + lroundf(dy * radius + 13 * t * t);
      frame.drawLine(x, y, x - lroundf(dx * 5), y - lroundf(dy * 5), colors[burst]);
      if (t < 0.62f) frame.fillCircle(x, y, 2, ILI9341_WHITE);
    }
  }
  if (elapsed >= 490) {
    frame.setFont(&NunitoSmall);
    frame.setTextColor(ILI9341_YELLOW);
    frame.setCursor(76, 76);
    frame.print("NEW BEST");
  }
  screen.drawRGBBitmap(fireworkX, fireworkY, frame.getBuffer(), fireworkW, fireworkH);
}

void Interface::gameMode(bool debug) {
  // 模式切换只改这块小字。入口单独处理右上标签的退场和入场。
  field(1, 223, 13, 61, 16, debug ? "DEBUG" : "NORMAL", 3,
        debug ? ILI9341_YELLOW : muted, true);
}

void Interface::stats(const Game& game, bool confirmClear) {
  using State = Game::State;
  const String last = game.lastOutcome == State::FalseStart ? String("FALSE START") :
                      game.lastOutcome == State::Timeout ? String("TIMEOUT") : duration(game.lastUs);
  const char* labels[] = {"LAST", "BEST", "ROUNDS"};
  const String values[] = {last, duration(game.best()), String(game.total())};
  for (int i = 0; i < 3; ++i) {
    const int y = 52 + i * 56;
    screen.drawRoundRect(42, y, 236, 46, 8, 0x39E7);
    field(i * 2, 51, y + 4, 86, 38, labels[i], 0, highlight);
    field(i * 2 + 1, 142, y + 4, 127, 38, values[i], 0, ILI9341_WHITE);
  }
  field(6, 38, 213, 244, 24, confirmClear ? "Press ZERO again to clear" :
        !game.saved() ? "Flash save unavailable" : "Times shown are raw", 3,
        confirmClear ? highlight : muted, true);
}

void Interface::calibration(const Game& game, bool editOffset, bool saveFailed) {
  const char* labels[] = {"Multiplier", "Offset (ms)"};
  const String values[] = {String(game.factor(), 2), String(game.offsetMs())};
  for (int i = 0; i < 2; ++i) {
    const int y = 58 + i * 53;
    const bool active = i == (editOffset ? 1 : 0);
    screen.drawRoundRect(42, y, 236, 44, 8, active ? highlight : 0x39E7);
    field(i * 2, 51, y + 4, 123, 36, labels[i], 0, ILI9341_WHITE);
    field(i * 2 + 1, 180, y + 4, 88, 36, values[i], 1, highlight, true);
  }
  field(4, 38, 171, 244, 20, "CAL = RAW x factor + offset", 4, muted, true);
  field(6, 38, 191, 244, 20, "FIELD selects  /  + or - adjusts", 4, muted, true);
  field(5, 38, 206, 244, 24, saveFailed ? "Save failed; press SAVE to retry" :
        "Hold FIELD to reset defaults", 4, saveFailed ? highlight : muted, true);
}

void Interface::card(int x, int y, int w, int h) {
  screen.fillRoundRect(x, y, w, h, 9, cardColor);
  screen.drawRoundRect(x, y, w, h, 9, cardOutline);
}

void Interface::metric(uint8_t slot, int x, int y, int w, int h,
                       char icon, const char* label, const String& value) {
  // 卡片边框、图标和标签只画一次，之后由 field 更新数值。
  if (!panelsReady) {
    card(x, y, w, h);
    smallIcon(screen, x + 15, y + 13, icon, highlight);
    screen.setFont(&NunitoSmall);
    screen.setTextColor(highlight);
    screen.setCursor(x + 30, y + 18);
    screen.print(label);
  }
  field(slot, x + 8, y + 22, w - 16, h - 26, value, 0, ILI9341_WHITE, false, true);
}

void Interface::weather(const WeatherData& data, bool forecast) {
  if (!data.available) {
    field(0, 42, 53, 236, 33, "Fetching weather...", 1, muted, true);
    return;
  }
  if (!panelsReady && cache[0].length()) screen.fillRect(42, 53, 236, 33, background);

  if (forecast) {
    for (uint8_t i = 0; i < 3; ++i) {
      const int y = 43 + i * 48;
      if (!panelsReady) card(40, y, 240, 44);
      if (forecastIcons[i] != data.dailyCode[i]) {
        screen.fillRect(44, y + 1, 43, 42, cardColor);
        weatherIcon(screen, 66, y + 22, data.dailyCode[i]);
        forecastIcons[i] = data.dailyCode[i];
      }
      field(i * 2, 91, y + 3, 181, 18, String(data.dates[i]) + "   " + condition(data.dailyCode[i]),
            0, highlight, false, true);
      field(i * 2 + 1, 91, y + 22, 181, 18,
            number(data.high[i]) + "/" + number(data.low[i]) + degree + "C   Rain " + number(data.chance[i]) + "%",
            0, ILI9341_WHITE, false, true);
    }
    field(8, 40, 217, 240, 18, String("Open-Meteo ") + data.updated + (data.stale ? "  (cached)" : ""),
          0, muted, true);
  } else {
    if (!panelsReady) card(40, 45, 240, 65);
    if (lastWeatherCode != data.code) {
      screen.fillRect(49, 54, 45, 45, cardColor);
      weatherIcon(screen, 72, 76, data.code);
      lastWeatherCode = data.code;
    }
    field(0, 99, 48, 171, 29, temperature(data.temp), 1, ILI9341_WHITE, false, true);
    field(1, 99, 79, 171, 23, String(condition(data.code)) + " / " + temperature(data.feels),
          0, highlight, false, true);
    metric(2, 40, 116, 116, 45, 'h', "Humidity", number(data.humidity) + "%");
    metric(3, 164, 116, 116, 45, 'v', "Wind", number(data.wind) + " km/h");
    metric(4, 40, 166, 116, 45, 'p', "Pressure", number(data.pressure) + " hPa");
    metric(5, 164, 166, 116, 45, 'h', "Rain", number(data.rain, 1) + " mm");
    // 日出、日落取当天预报的数据，但在当前天气页展示。
    field(6, 39, 214, 242, 22, String("Sunrise ") + data.sunrise + "  /  Sunset " + data.sunset,
          4, muted, true);
  }
  panelsReady = true;
}

void Interface::network() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (!panelsReady) card(40, 43, 240, 34);
  field(0, 51, 46, 218, 28, connected ? "Connected" : "Disconnected", 1, highlight, true, true);
  metric(1, 40, 82, 240, 44, 'w', "Network", connected ? WiFi.SSID() : String("--"));
  metric(2, 40, 132, 240, 44, 'm', "IP address", connected ? WiFi.localIP().toString() : String("--"));
  metric(3, 40, 182, 116, 45, 's', "Signal", connected ? String(WiFi.RSSI()) + " dBm" : String("--"));
  tm local;
  metric(4, 164, 182, 116, 45, 'p', "Clock", getLocalTime(&local, 10) ? "NTP ready" : "Waiting");
  panelsReady = true;
}

void Interface::bluetooth(const BluetoothSnapshot& scan, int first, bool namedOnly) {
  using State = BluetoothSnapshot::State;
  // 只在扫描状态或滚动位置改变时重画内容，保留标题和侧边键。
  screen.fillRect(32, 42, 256, 192, background);
  for (int slot = 0; slot <= 10; ++slot) cache[slot] = "";
  if (scan.state != State::Ready || scan.count == 0) {
    const char* message = scan.state == State::Scanning ? "Scanning nearby devices..." :
                          scan.state == State::Failed ? "Scan failed. Try SCAN again." :
                          scan.state == State::Ready ?
                              (namedOnly ? "No named BLE devices found" : "No BLE broadcasts found") :
                              "Press SCAN to start";
    field(0, 36, 91, 248, 34, message, 0, highlight, true);
    field(9, 38, 214, 244, 19, namedOnly ? "Named only  /  Hold SCAN for all" :
          "All devices  /  Hold SCAN to filter", 3, highlight, true);
    return;
  }
  for (int row = 0; row < bluetoothRows && first + row < scan.count; ++row) {
    const auto& device = scan.devices[first + row];
    const int y = 43 + row * 56;
    const int slot = row * 3;
    card(38, y, 244, 53);
    field(slot, 46, y + 3, 151, 20, device.name, 0, ILI9341_WHITE, false, true);
    field(slot + 1, 199, y + 3, 74, 20, String(device.rssi) + " dBm", 3, highlight, true, true);
    field(slot + 2, 46, y + 23, 224, 15, device.address, 3, muted, false, true);
    field(10, 46, y + 37, 224, 13, device.service, 3, muted, false, true);
    cache[10] = "";  // 每行共用 UUID 字段编号，因为整组结果一起绘制。
  }
  const int pages = (scan.count + bluetoothRows - 1) / bluetoothRows;
  field(9, 38, 214, 244, 19,
        String(first / bluetoothRows + 1) + "/" + pages + "  |  " +
        scan.count + (namedOnly ? " named  /  Hold SCAN" : " shown  /  Hold SCAN"), 3, highlight, true);
}

void Interface::memoryCard(uint8_t index, int y, const char* label, size_t total, size_t free) {
  constexpr int x = 40, width = 240, trackX = 52, trackWidth = 216;
  if (!panelsReady) card(x, y, width, 37);
  const size_t used = total - min(free, total);
  const int filled = total ? static_cast<int>((static_cast<uint64_t>(used) * trackWidth + total / 2) / total) : 0;
  const uint16_t color = index == 0 ? 0x7DEB : index == 1 ? 0x65FF : sunColor;
  constexpr float bytesPerMiB = 1024.0f * 1024.0f;
  String value = "Unavailable";
  if (total) {
    value = index == 2 ? String(used / bytesPerMiB, 2) + " / " + String(total / bytesPerMiB, 2) + " MiB" :
                         String(used / 1024) + " / " + String(total / 1024) + " KiB";
  }
  field(2 + index * 2, 51, y + 3, 58, 20, label, 3, color, false, true);
  field(3 + index * 2, 111, y + 3, 156, 20, value, 0, ILI9341_WHITE, false, true);
  if (deviceBars[index] != filled) {
    deviceBars[index] = filled;
    screen.fillRoundRect(trackX, y + 26, trackWidth, 6, 3, cardOutline);
    if (filled) screen.fillRoundRect(trackX, y + 26, filled, 6, min(filled / 2, 3), color);
  }
}

void Interface::device() {
  field(8, 198, 10, 88, 24, Config::firmwareVersion, 3, muted, true);
  constexpr uint16_t cool = 0x7DEB, warm = 0xFDC0, hot = 0xFA69;
  constexpr float warmAt = 40.0f, hotAt = 50.0f;
  const float chip = temperatureRead();
  const bool valid = isfinite(chip);
  const uint16_t tint = !valid ? muted : chip >= hotAt ? hot : chip >= warmAt ? warm : cool;
  const int level = valid ? constrain(static_cast<int>((chip - 20.0f) * 26 / 40.0f), 0, 26) : 0;
  const String reading = valid ? temperature(chip) : String("--");
  if (!panelsReady) card(40, 43, 240, 58);
  // 这些阈值只控制界面颜色，温度计刻度用于直观显示芯片读数。
  if (cache[0] != reading) {
    screen.fillRect(50, 48, 33, 47, cardColor);
    screen.drawRoundRect(61, 51, 11, 34, 5, muted);
    if (level > 0) screen.fillRect(64, 82 - level, 5, level, tint);
    screen.fillCircle(66, 86, 8, tint);
    field(0, 89, 65, 177, 29, reading, 1, tint, false, true);
  }
  field(1, 89, 49, 177, 15, "CHIP TEMPERATURE", 3, muted, false, true);

  memoryCard(0, 104, "HEAP", ESP.getHeapSize(), ESP.getFreeHeap());
  memoryCard(1, 144, "PSRAM", ESP.getPsramSize(), ESP.getFreePsram());
  // 固件镜像大小和正在运行的 APP 分区容量在本次开机期间固定。
  static const auto* app = esp_ota_get_running_partition();
  static const size_t appCapacity = app ? app->size : 0;
  static const size_t imageSize = appCapacity ? ESP.getSketchSize() : 0;
  memoryCard(2, 184, "Firmware", imageSize ? appCapacity : 0,
             appCapacity - min(appCapacity, imageSize));
  field(10, 40, 223, 240, 16, String("Up ") + millis() / 1000 + " s  /  Flash " +
        ESP.getFlashChipSize() / (1024 * 1024) + " MiB", 3, muted, true);
  panelsReady = true;
}

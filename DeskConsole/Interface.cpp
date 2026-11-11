#include "Interface.h"
#include "NunitoFonts.h"
#include <WiFi.h>
#include <time.h>

namespace {
constexpr uint16_t muted = 0xA69A;
constexpr uint16_t cardColor = 0x1928;
constexpr uint16_t cardOutline = 0x3A4E;
constexpr char degree = '\x01';
constexpr uint16_t sunColor = 0xFDC0;
constexpr uint16_t rainColor = 0x54FF;

void smallIcon(Adafruit_GFX& gfx, int x, int y, char kind, uint16_t color) {
  switch (kind) {
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

void Interface::beginPage(const char* title, uint16_t base, uint16_t color) {
  // 真正切页才清屏。换页后所有绘制缓存需要重新建立。
  background = base;
  highlight = color;
  for (auto& text : cache) text = "";
  lastLights = -1;
  lastSelection = -1;
  panelsReady = false;
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

void Interface::launcher(uint8_t selected) {
  constexpr int cardX = 46, firstY = 49, width = 228, height = 36, pitch = 43;
  static const char* labels[] = {"F1 Reaction", "Weather", "Wi-Fi", "Device"};
  if (lastSelection == selected) return;

  GFXcanvas16 frame(width, height);
  GFXcanvas1 shape(width, height);
  GFXcanvas1 ink(width, height);
  if (!frame.getBuffer() || !shape.getBuffer() || !ink.getBuffer()) return;
  shape.fillScreen(0);
  shape.fillRoundRect(0, 0, width, height, 8, 1);
  ink.setTextWrap(false);
  ink.setTextColor(1);

  auto paint = [&](int row, int highlightY) {
    const int y = firstY + row * pitch;
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
    for (int py = 0; py < height; ++py) {
      // 用循环列表中的相对坐标取样，让移动的高亮也保留圆角。
      constexpr int cycle = 4 * pitch;
      const int highlightRow = (y + py - highlightY + cycle) % cycle;
      for (int px = 0; px < width; ++px) {
        const bool lit = highlightRow < height && shape.getPixel(px, highlightRow);
        pixels[py * width + px] = !shape.getPixel(px, py) ? background :
            ink.getPixel(px, py) ? (lit ? ILI9341_BLACK : ILI9341_WHITE) :
            lit ? highlight : 0x2167;
      }
    }
    screen.drawRGBBitmap(cardX, y, pixels, width, height);
  };

  if (lastSelection < 0) {
    for (int row = 0; row < 4; ++row) paint(row, firstY + selected * pitch);
  } else {
    const int from = firstY + lastSelection * pitch;
    int distance = static_cast<int>(selected) - lastSelection;
    if (distance == 3) distance = -1;
    if (distance == -3) distance = 1;
    constexpr int steps = 9;
    for (int step = 1; step <= steps; ++step) {
      const float t = static_cast<float>(step) / steps;
      const float eased = t * t * (3.0f - 2.0f * t);
      const int pos = from + lroundf(distance * pitch * eased);
      paint(lastSelection, pos);
      paint(selected, pos);
    }
  }
  lastSelection = selected;
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
    field(6, 39, 188, 242, 20, String("Sunrise ") + data.sunrise + "  /  Sunset " + data.sunset, 0, ILI9341_WHITE, true);
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
  }
  field(8, 40, 217, 240, 18, String("Open-Meteo ") + data.updated + (data.stale ? "  (cached)" : ""), 0, muted, true);
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

void Interface::device() {
  field(0, 40, 39, 240, 22, ESP.getChipModel(), 0, highlight, true);
  metric(1, 40, 67, 116, 49, 't', "Chip temp", temperature(temperatureRead()));
  metric(2, 164, 67, 116, 49, 'p', "Uptime", String(millis() / 1000) + " s");
  metric(3, 40, 122, 116, 49, 'm', "Free heap", String(ESP.getFreeHeap() / 1024) + " KiB");
  metric(4, 164, 122, 116, 49, 'm', "PSRAM", String(ESP.getFreePsram() / 1024) + " KiB");
  metric(5, 40, 177, 240, 49, 'm', "Flash", String(ESP.getFlashChipSize() / (1024 * 1024)) + " MiB");
  panelsReady = true;
}

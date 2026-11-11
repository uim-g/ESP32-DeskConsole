#include "Interface.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <WiFi.h>
#include <time.h>

namespace {
constexpr uint16_t muted = 0xA69A;
constexpr char degree = '\x01';
constexpr uint16_t sunColor = 0xFDC0;
constexpr uint16_t rainColor = 0x54FF;

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
  background = base;
  highlight = color;
  for (auto& text : cache) text = "";
  lastLights = -1;
  lastSelection = -1;
  lastWeatherCode = -100;
  for (auto& code : forecastIcons) code = -100;
  for (auto& label : tabs) label = nullptr;
  screen.fillScreen(background);
  if (*title) {
    field(9, 32, 7, 256, 29, title, 1, highlight, true);
    screen.drawFastHLine(32, 40, 256, muted);
  } else {
    screen.drawFastHLine(84, 160, 152, muted);
  }
}

void Interface::tab(bool right, bool lower, const char* label) {
  tabs[(right ? 2 : 0) + (lower ? 1 : 0)] = label;
}

void Interface::animateTabs(bool entering) {
  bool any = false;
  for (const char* label : tabs) any |= label != nullptr;
  if (!any) return;

  constexpr int width = 24, body = 21, maxHeight = 48;
  GFXcanvas16 patch(width, maxHeight);
  GFXcanvas1 letters(36, 8);
  if (!patch.getBuffer() || !letters.getBuffer()) return;
  letters.setTextWrap(false);
  letters.setTextColor(1);

  // 退场先慢后快。入场略过目标位置，再回弹至最终位置。
  constexpr int slideOut[] = {0, 1, 3, 7, 12, 18, 21};
  constexpr int slideIn[] = {21, 15, 8, 2, -2, -1, 0};
  for (int frame = 0; frame < 7; ++frame) {
    const int offset = entering ? slideIn[frame] : slideOut[frame];
    for (uint8_t i = 0; i < 4; ++i) {
      const char* label = tabs[i];
      if (!label) continue;
      const bool right = i >= 2;
      const bool arrow = label[0] == '^' || label[0] == 'v';
      const bool apps = strcmp(label, "APPS") == 0;
      const bool back = strcmp(label, "BACK") == 0;
      const bool hasIcon = apps || back;
      const int length = strlen(label) * 6;
      const int h = arrow ? 22 : length + (hasIcon ? 20 : 8);
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
          letters.setCursor(0, 0);
          letters.print(label);
          const int middle = center + (hasIcon ? 6 : 0);
          for (int u = 0; u < length; ++u) {
            for (int v = 0; v < 8; ++v) {
              if (letters.getPixel(u, v)) {
                patch.drawPixel(cx + (right ? 3 - v : v - 3),
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
    for (auto& label : tabs) label = nullptr;
  }
}

void Interface::launcher(uint8_t selected) {
  static const char* labels[] = {"F1 Reaction", "Weather", "Wi-Fi", "Device"};
  for (uint8_t i = 0; i < 4; ++i) {
    if (lastSelection >= 0 && i != selected && i != lastSelection) continue;
    const int y = 49 + i * 43;
    const bool active = i == selected;
    const uint16_t fill = active ? highlight : 0x2167;
    const uint16_t ink = active ? ILI9341_BLACK : ILI9341_WHITE;
    screen.fillRoundRect(46, y, 228, 36, 8, fill);
    if (!active) screen.drawRoundRect(46, y, 228, 36, 8, 0x426C);
    const int x = 67, cy = y + 18;
    if (i == 0) {
      screen.drawFastVLine(x - 6, cy - 7, 15, ink);
      screen.drawRect(x - 5, cy - 7, 13, 9, ink);
      screen.fillRect(x - 4, cy - 6, 4, 3, ink);
      screen.fillRect(x, cy - 3, 4, 3, ink);
    } else if (i == 1) {
      screen.fillCircle(x - 5, cy, 5, ink);
      screen.fillCircle(x + 2, cy - 3, 6, ink);
      screen.fillRoundRect(x - 8, cy, 19, 6, 3, ink);
    } else if (i == 2) {
      for (int r : {5, 9, 13}) {
        screen.drawLine(x - r, cy + 3 - r / 2, x, cy - r, ink);
        screen.drawLine(x, cy - r, x + r, cy + 3 - r / 2, ink);
      }
      screen.fillCircle(x, cy + 5, 2, ink);
    } else {
      screen.drawRect(x - 6, cy - 6, 13, 13, ink);
      for (int n : {-4, 0, 4}) {
        screen.drawFastHLine(x - 9, cy + n, 3, ink);
        screen.drawFastHLine(x + 7, cy + n, 3, ink);
      }
    }
    screen.setFont(&FreeSans12pt7b);
    screen.setTextColor(ink);
    screen.setCursor(93, y + 25);
    screen.print(labels[i]);
  }
  lastSelection = selected;
}

void Interface::field(uint8_t slot, int x, int y, int w, int h, String value,
                      uint8_t size, uint16_t color, bool centered) {
  // 动态字段只能写在两侧按键标签之间。
  const int end = min(x + w, 288);
  x = max(x, 32);
  w = end - x;
  if (w <= 0) return;
  if (cache[slot] == value) return;
  cache[slot] = value;
  const GFXfont* font = size == 2 ? &FreeSansBold24pt7b :
                       size == 1 ? &FreeSans12pt7b : &FreeSans9pt7b;
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
  screen.fillRect(x, y, w, h, background);
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
  switch (game.state) {
    case State::Idle: message = "Press to start"; break;
    case State::Lighting:
    case State::Waiting: message = "Wait for lights out"; break;
    case State::GoPending:
    case State::Go: message = game.debug && game.state == State::Go ?
        "RAW " + duration(game.liveUs()) : String("GO!"); break;
    case State::Result: message = duration(game.resultUs); break;
    case State::FalseStart: message = "FALSE START"; break;
    case State::Timeout: message = "TIMEOUT"; break;
  }
  field(0, 38, 151, 244, 38, message, 1, ILI9341_WHITE, true);
  field(1, 88, 43, 144, 21, game.debug ? "DEBUG" : "NORMAL", 0, highlight, true);
  if (!game.saved()) field(3, 16, 210, 288, 21, "Flash save unavailable", 0, muted, true);
}

void Interface::stats(const Game& game) {
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
}

void Interface::weather(const WeatherData& data, bool forecast) {
  if (!data.available) {
    field(0, 82, 49, 200, 37, "Fetching weather...", 1, muted);
    return;
  }
  if (forecast) {
    for (uint8_t i = 0; i < 3; ++i) {
      const int y = 48 + i * 43;
      // 每天的图标只在其天气代码变化时更新。
      if (forecastIcons[i] != data.dailyCode[i]) {
        screen.fillRect(35, y, 43, 41, background);
        weatherIcon(screen, 57, y + 20, data.dailyCode[i]);
        forecastIcons[i] = data.dailyCode[i];
      }
      field(i * 2, 83, y, 199, 21, String(data.dates[i]) + "  " + condition(data.dailyCode[i]), 0, highlight);
      field(i * 2 + 1, 83, y + 21, 199, 21,
            number(data.high[i]) + "/" + temperature(data.low[i]) + "   Rain " + number(data.chance[i]) + "%");
    }
    field(6, 16, 179, 288, 22, String("Sunrise ") + data.sunrise + "   Sunset " + data.sunset);
  } else {
    if (lastWeatherCode != data.code) {
      screen.fillRect(34, 47, 44, 44, background);
      weatherIcon(screen, 56, 68, data.code);
      lastWeatherCode = data.code;
    }
    field(0, 82, 49, 200, 37, String(condition(data.code)) + "  " + temperature(data.temp), 1, highlight);
    field(1, 16, 90, 288, 23, "Feels like " + temperature(data.feels));
    field(2, 16, 114, 288, 23, "Humidity " + number(data.humidity) + "%");
    field(3, 16, 138, 288, 23, "Wind " + number(data.wind) + " km/h");
    field(4, 16, 162, 288, 23, "Pressure " + number(data.pressure) + " hPa");
    field(5, 16, 186, 288, 22, "Rain " + number(data.rain, 1) + " mm");
  }
  field(8, 16, 211, 288, 22, String("Open-Meteo ") + data.updated + (data.stale ? "  (cached)" : ""), 0, muted);
}

void Interface::network() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  field(0, 16, 52, 288, 27, connected ? "Connected" : "Disconnected", 1, accent);
  field(1, 16, 86, 288, 24, connected ? String("SSID: ") + WiFi.SSID() : "SSID: --");
  field(2, 16, 112, 288, 24, connected ? String("IP: ") + WiFi.localIP().toString() : "IP: --");
  field(3, 16, 138, 288, 24, connected ? String("RSSI: ") + WiFi.RSSI() + " dBm" : "RSSI: --");
  tm local;
  field(4, 16, 164, 288, 24, getLocalTime(&local, 10) ? "Clock: synchronized" : "Clock: waiting for NTP");
  field(5, 16, 202, 288, 24, "Action: reconnect", 0, muted);
}

void Interface::device() {
  field(0, 16, 52, 288, 29, ESP.getChipModel(), 1, highlight);
  field(1, 16, 86, 288, 24, "Chip temp: " + temperature(temperatureRead()));
  field(2, 16, 112, 288, 24, String("Uptime: ") + millis() / 1000 + " s");
  field(3, 16, 138, 288, 24, String("Free heap: ") + ESP.getFreeHeap() / 1024 + " KiB");
  field(4, 16, 164, 288, 24, String("Free PSRAM: ") + ESP.getFreePsram() / 1024 + " KiB");
  field(5, 16, 190, 288, 24, String("Flash: ") + ESP.getFlashChipSize() / (1024 * 1024) + " MiB");
}

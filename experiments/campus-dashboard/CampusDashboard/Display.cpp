#include "Display.h"
#include <WiFi.h>
#include <time.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

namespace {
constexpr uint16_t Background = 0x0842;
constexpr uint16_t Card = 0x1927;
constexpr uint16_t Accent = 0x5EFF;

// 用 GFX 图形绘制图标，无需额外图片、字体或 SD 卡。
void drawWeatherIcon(Adafruit_GFX& screen, int x, int y, int code) {
  screen.fillRect(x - 22, y - 17, 44, 34, Background);
  if (code < 0) return;

  if (code <= 2) screen.fillCircle(x - 5, y - 5, 10, ILI9341_YELLOW);
  if (code == 0) return;
  if (code == 45 || code == 48) {
    for (int offset = -8; offset <= 8; offset += 8) {
      screen.fillRoundRect(x - 17, y + offset, 34, 3, 1, ILI9341_LIGHTGREY);
    }
    return;
  }

  screen.fillCircle(x - 10, y, 7, ILI9341_LIGHTGREY);
  screen.fillCircle(x, y - 5, 9, ILI9341_LIGHTGREY);
  screen.fillCircle(x + 10, y, 7, ILI9341_LIGHTGREY);
  screen.fillRoundRect(x - 15, y, 30, 7, 3, ILI9341_LIGHTGREY);
  if (code >= 95) {
    screen.fillTriangle(x, y + 5, x - 5, y + 12, x + 1, y + 12, ILI9341_YELLOW);
    screen.fillTriangle(x - 1, y + 10, x + 5, y + 10, x - 1, y + 16, ILI9341_YELLOW);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    for (int dx = -8; dx <= 8; dx += 8) {
      screen.fillCircle(x + dx, y + 12, 2, ILI9341_WHITE);
    }
  } else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    for (int dx = -8; dx <= 8; dx += 8) {
      screen.drawLine(x + dx, y + 9, x + dx - 3, y + 15, Accent);
    }
  }
}
String number(float value, unsigned digits = 0) {
  return isnan(value) ? String("--") : String(value, digits);
}

constexpr uint16_t Muted = 0xA69A;
constexpr uint16_t Track = 0x2A69;
constexpr char DegreeMarker = '\x01';
constexpr int MarkerTop = 83;
constexpr int MarkerSpacing = 23;

// 两边共用平滑步进，起止速度为零。
float ease(float t) {
  return t * t * (3.0f - 2.0f * t);
}
}

void Display::begin() {
  SPI.begin(Config::sck, -1, Config::mosi, Config::cs);
  screen.begin(10000000);
  screen.setRotation(Config::rotation == 3 ? 3 : 1);
  screen.setTextWrap(false);
  canvas.setTextWrap(false);
  screen.fillScreen(Background);
}

void Display::rail(uint8_t page, float marker, bool redrawControls) {
  if (redrawControls) {
    screen.fillRect(Width, 0, 320 - Width, Height, Card);
    screen.drawFastVLine(Width, 0, Height, Track);
    screen.setFont(&FreeSans9pt7b);
    screen.setTextColor(ILI9341_WHITE);
    screen.setCursor(288, 27);
    screen.printf("%u/4", page + 1);
    const uint16_t up = Accent;
    const uint16_t down = Accent;
    screen.drawLine(294, 64, 301, 57, up);
    screen.drawLine(301, 57, 308, 64, up);
    screen.drawLine(294, 180, 301, 187, down);
    screen.drawLine(301, 187, 308, 180, down);
  }

  screen.fillRect(292, MarkerTop, 18, 82, Card);
  for (uint8_t i = 0; i < pageCount; ++i) {
    screen.fillRoundRect(292, MarkerTop + i * MarkerSpacing, 18, 13, 4, Track);
  }
  const int markerY = MarkerTop + lroundf(marker * MarkerSpacing);
  const int trackEnd = MarkerTop + (pageCount - 1) * MarkerSpacing + 13;
  // 跨过首尾时，在轨道另一端绘制亮块滑入的部分。
  for (int y : {markerY, markerY - pageCount * MarkerSpacing,
                markerY + pageCount * MarkerSpacing}) {
    const int top = y < MarkerTop ? MarkerTop : y;
    const int bottom = y + 13 > trackEnd ? trackEnd : y + 13;
    if (top < bottom) screen.fillRoundRect(292, top, 18, bottom - top, 4, Accent);
  }
}

void Display::transfer(int offset) {
  // 从画布截取连续的行，保证动画只写左侧，右侧导航保持固定。
  const int skipped = offset < 0 ? -offset : 0;
  const int y = offset > 0 ? offset : 0;
  screen.drawRGBBitmap(0, y, canvas.getBuffer() + skipped * Width,
                       Width, Height - abs(offset));
  if (offset < 0) screen.fillRect(0, Height + offset, Width, -offset, Background);
  if (offset > 0) screen.fillRect(0, 0, Width, offset, Background);
}

void Display::field(uint8_t slot, int x, int y, int width, int height,
                    const GFXfont* font, String text, uint16_t color, bool center) {
  if (previous[slot] == text) return;
  previous[slot] = text;
  target->setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  // 用 o 的字宽为度数符号预留位置，再在同一位置画空心圆。
  String measured = text;
  measured.replace(DegreeMarker, 'o');
  target->getTextBounds(measured.c_str(), 0, 0, &x1, &y1, &w, &h);
  while (w > width && text.length()) {
    text.remove(text.length() - 1);
    measured.remove(measured.length() - 1);
    target->getTextBounds(measured.c_str(), 0, 0, &x1, &y1, &w, &h);
  }
  target->fillRect(x, y, width, height, Background);
  if (!text.length()) return;
  target->setTextColor(color);
  target->setCursor(x - x1 + (center ? (width - w) / 2 : 0), y - y1 + (height - h) / 2);
  const int degree = text.indexOf(DegreeMarker);
  if (degree < 0) {
    target->print(text);
    return;
  }

  target->print(text.substring(0, degree));
  const int cursorX = target->getCursorX();
  const int baseline = target->getCursorY();
  int16_t glyphX, glyphY;
  uint16_t glyphWidth, glyphHeight;
  target->getTextBounds("o", cursorX, baseline, &glyphX, &glyphY, &glyphWidth, &glyphHeight);
  int16_t capX, capY;
  uint16_t capWidth, capHeight;
  target->getTextBounds("C", cursorX, baseline, &capX, &capY, &capWidth, &capHeight);
  const int radius = glyphHeight >= 20 ? 5 : (glyphHeight >= 12 ? 3 : 2);
  target->drawCircle(glyphX + glyphWidth / 2, capY + radius, radius, color);
  const uint8_t advance = font->glyph['o' - font->first].xAdvance;
  target->setCursor(cursorX + advance, baseline);
  target->print(text.substring(degree + 1));
}

void Display::icon(uint8_t slot, int x, int y, int code) {
  if (previousIcons[slot] == code) return;
  previousIcons[slot] = code;
  drawWeatherIcon(*target, x, y, code);
}

void Display::render(uint8_t page, const WeatherData& w, bool full) {
  if (full) {
    target->fillRect(0, 0, Width, Height, Background);
    for (auto& text : previous) text = "";
    for (auto& code : previousIcons) code = -2;
  }

  const GFXfont* small = &FreeSans9pt7b;
  const GFXfont* medium = &FreeSans12pt7b;
  switch (page) {
    case 0: {
      tm local;
      char clock[9] = "--:--:--";
      char date[24] = "Waiting for NTP";
      if (getLocalTime(&local, 10)) {
        strftime(clock, sizeof(clock), "%H:%M:%S", &local);
        strftime(date, sizeof(date), "%a %d %b %Y", &local);
      }
      field(0, 12, 23, 258, 30, medium, Config::location, Accent, true);
      if (full) {
        target->drawFastHLine(12, 63, 258, Accent);
        target->drawFastHLine(12, 162, 258, Accent);
      }
      // 时分秒分别缓存。秒变化时只擦写最右边的两位数字。
      const GFXfont* digits = &FreeSansBold24pt7b;
      field(1, 30, 73, 62, 78, digits, String(clock).substring(0, 2), ILI9341_WHITE, true);
      field(4, 110, 73, 62, 78, digits, String(clock).substring(3, 5), ILI9341_WHITE, true);
      field(5, 190, 73, 62, 78, digits, String(clock).substring(6, 8), ILI9341_WHITE, true);
      if (full) {
        field(6, 92, 73, 18, 78, digits, ":", ILI9341_WHITE, true);
        field(7, 172, 73, 18, 78, digits, ":", ILI9341_WHITE, true);
      }
      field(2, 12, 172, 258, 28, medium, date, ILI9341_WHITE, true);
      field(3, 12, 204, 258, 22, small, "MYT UTC+8", Muted, true);
      break;
    }
    case 1: {
      field(0, 12, 9, 258, 23, small, w.available ? Config::location : "Fetching weather...", Accent);
      if (!w.available) break;
      icon(0, 60, 65, w.code);
      field(1, 104, 35, 166, 53, &FreeSansBold24pt7b, number(w.temp, 1) + DegreeMarker + "C", ILI9341_WHITE);
      field(2, 105, 87, 165, 23, small, "Feels " + number(w.feels, 1) + DegreeMarker + "C", Muted);
      const char* labels[] = {"Humidity", "Wind", "Pressure", "Rain"};
      const String values[] = {number(w.humidity) + "%", number(w.wind) + " km/h",
                               number(w.pressure) + " hPa", number(w.rain, 1) + " mm"};
      for (uint8_t i = 0; i < 4; ++i) {
        const int x = 12 + (i % 2) * 132;
        const int y = 115 + (i / 2) * 48;
        if (previous[3 + i].length() == 0) {
          target->drawRoundRect(x, y, 126, 44, 6, Track);
          field(7 + i, x + 7, y + 2, 112, 18, small, labels[i], Accent);
        }
        field(3 + i, x + 7, y + 20, 112, 22, small, values[i], ILI9341_WHITE);
      }
      field(11, 12, 214, 258, 19, small, String("Open-Meteo ") + w.updated + (w.stale ? " cached" : ""), Muted);
      break;
    }
    case 2:
      field(0, 12, 8, 258, 24, small, w.available ? "3-DAY FORECAST" : "Fetching weather...", Accent);
      if (!w.available) break;
      for (uint8_t i = 0; i < 3; ++i) {
        const int y = 36 + i * 54;
        if (previousIcons[i] == -2) target->drawRoundRect(12, y, 258, 48, 7, Track);
        icon(i, 103, y + 24, w.dailyCode[i]);
        field(1 + 3 * i, 20, y + 5, 58, 37, small,
              i == 0 ? String("Today") : String(w.dates[i]).substring(5), ILI9341_WHITE);
        field(2 + 3 * i, 132, y + 3, 128, 22, medium,
              number(w.high[i]) + "/" + number(w.low[i]) + DegreeMarker + "C", ILI9341_WHITE);
        field(3 + 3 * i, 132, y + 25, 128, 20, small, "Rain " + number(w.chance[i]) + "%", Accent);
      }
      field(10, 12, 197, 258, 22, small, String("Sunrise ") + w.sunrise + "   Sunset " + w.sunset, ILI9341_WHITE, true);
      field(11, 12, 220, 258, 17, small, w.stale ? "Open-Meteo | cached" : "Open-Meteo", Muted, true);
      break;
    case 3: {
      const bool online = WiFi.status() == WL_CONNECTED;
      field(0, 12, 23, 258, 39, medium, online ? "WiFi Connected" : "WiFi Offline", Accent, true);
      if (full) target->drawFastHLine(12, 71, 258, Accent);
      field(1, 16, 84, 250, 22, small, online ? String("SSID: ") + WiFi.SSID() : "SSID: --", Muted);
      field(2, 16, 111, 250, 22, small, online ? String("IP: ") + WiFi.localIP().toString() : "IP: --", ILI9341_WHITE);
      field(3, 16, 138, 250, 22, small, online ? String("RSSI: ") + WiFi.RSSI() + " dBm" : "RSSI: --", ILI9341_WHITE);
      field(4, 16, 165, 250, 22, small, String("Uptime: ") + millis() / 1000 + " s", ILI9341_WHITE);
      field(5, 16, 192, 250, 22, small, String("Free heap: ") + ESP.getFreeHeap() / 1024 + " KiB", ILI9341_WHITE);
      break;
    }
  }
}

void Display::show(uint8_t page, const WeatherData& weather, bool newPage) {
  if (!newPage) {
    render(page, weather, false);
    return;
  }
  if (!canvas.getBuffer()) {
    render(page, weather, true);
    rail(page, page, true);
    activePage = page;
    return;
  }

  target = &canvas;
  if (activePage < pageCount) {
    const bool wrappedForward = activePage == pageCount - 1 && page == 0;
    const bool wrappedBackward = activePage == 0 && page == pageCount - 1;
    const int direction = !wrappedBackward && (wrappedForward || page > activePage) ? 1 : -1;
    // 共用同一张画布。先绘制离场页，再复用画布绘制入场页。
    render(activePage, weather, true);
    for (float t : {0.25f, 0.45f}) {
      const float p = ease(t);
      rail(activePage, activePage + direction * p, false);
      transfer(-direction * lroundf(36 * p));
    }
    render(page, weather, true);
    for (float t : {0.65f, 0.85f}) {
      const float p = ease(t);
      rail(activePage, activePage + direction * p, false);
      transfer(direction * lroundf(36 * (1.0f - p)));
    }
  } else {
    render(page, weather, true);
  }
  transfer(0);
  rail(page, page, true);
  activePage = page;
  target = &screen;
}

#include "Interface.h"
#include "NunitoFonts.h"

namespace {
constexpr uint16_t sunColor = 0xFDC0, rainColor = 0x54FF;
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

String Interface::temperature(float value) {
  return number(value, 1) + degree + "C";
}

// 桌面分别缓存时分秒和天气字段，普通秒跳只更新对应数字格。
void Interface::home(const WeatherData& data, const Platform::Clock& clock) {
  // 固定时分秒的文字区，秒变化时只刷新右边一格。
  field(0, 44, 45, 62, 71, String(clock.time).substring(0, 2), 2, ILI9341_WHITE, true);
  field(1, 110, 45, 18, 71, ":", 2, ILI9341_WHITE, true);
  field(2, 129, 45, 62, 71, String(clock.time).substring(3, 5), 2, ILI9341_WHITE, true);
  field(3, 192, 45, 18, 71, ":", 2, ILI9341_WHITE, true);
  field(4, 214, 45, 62, 71, String(clock.time).substring(6, 8), 2, ILI9341_WHITE, true);
  field(5, 18, 123, 284, 31, clock.date, 1, ILI9341_WHITE, true);
  if (lastWeatherCode != data.code) {
    screen.fillRect(46, 171, 41, 40, background);
    weatherIcon(screen, 68, 191, data.code);
    lastWeatherCode = data.code;
  }
  field(6, 89, 172, 176, 35, String(condition(data.code)) + "  |  " + temperature(data.temp),
        0, highlight, true);
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

// 按当前天气或预报组织卡片数据。未取得结果时显示等待，刷新失败时可以继续显示缓存。
void Interface::weather(const WeatherData& data, bool forecast) {
  const int view = forecast ? 1 : 0;
  if (weatherView != view) {
    if (weatherView >= 0) screen.fillRect(32, 42, 256, 198, background);
    weatherView = view;
    for (int i = 0; i <= 8; ++i) cache[i] = "";
    lastWeatherCode = -100;
    for (auto& code : forecastIcons) code = -100;
    panelsReady = false;
    field(10, weatherSubtitleX, 7, 280 - weatherSubtitleX, 29,
          forecast ? "Forecast" : "Now", 1, highlight);
  }
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

void Interface::bluetooth(const BluetoothSnapshot& scan, int first, bool namedOnly) {
  using State = BluetoothSnapshot::State;
  // 只在扫描状态或滚动位置改变时重画内容，保留标题和侧边键。
  screen.fillRect(32, 42, 256, 192, background);
  for (int slot = 0; slot <= 10; ++slot) cache[slot] = "";
  for (auto& name : scrollingNames) name.value = "";
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
    scrollingName(row, 46, y + 3, 151, 20, device.name, cardColor);
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

// 设备页展示快照中的温度、内存、Flash 和运行时长。时长等宽数字逐位刷新，位数变化才移动单位。
void Interface::device(const Platform::Device& data) {
  field(8, 198, 10, 88, 24, Config::firmwareVersion, 3, muted, true);
  constexpr uint16_t cool = 0x7DEB, warm = 0xFDC0, hot = 0xFA69;
  constexpr float warmAt = 40.0f, hotAt = 50.0f;
  const float chip = data.temperature;
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

  memoryCard(0, 104, "HEAP", data.heap.total, data.heap.free);
  memoryCard(1, 144, "PSRAM", data.psram.total, data.psram.free);
  memoryCard(2, 184, "Firmware", data.application.total, data.application.free);
  char digits[sizeof(uptimeDigits)] = {};
  snprintf(digits, sizeof(digits), "%lu", static_cast<unsigned long>(data.uptimeSeconds));
  constexpr int cellWidth = 9, baseline = 235;
  const auto glyphFor = [](char c) -> const GFXglyph& {
    return NunitoTab.glyph[c - NunitoTab.first];
  };
  const int prefixWidth = glyphFor('U').xAdvance + glyphFor('p').xAdvance + glyphFor(' ').xAdvance;
  const size_t previousLength = strlen(uptimeDigits);
  const size_t length = strlen(digits);
  const String suffix = String(" s  /  Flash ") + data.flash / (1024 * 1024) + " MiB";
  int suffixWidth = 0;
  for (const char* c = suffix.c_str(); *c; ++c) suffixWidth += glyphFor(*c).xAdvance;
  const int digitsWidth = static_cast<int>(length) * cellWidth;
  const int labelX = (320 - prefixWidth - digitsWidth - suffixWidth) / 2;
  const int firstDigit = labelX + prefixWidth;
  const bool layoutChanged = previousLength != length || cache[10] != suffix;
  screen.setFont(&NunitoTab);
  screen.setTextColor(muted);
  if (layoutChanged) {
    // 位数变化时重新居中；普通秒更新只重绘变化的数字格。
    screen.fillRect(40, 223, 240, 17, background);
    screen.setCursor(labelX, baseline);
    screen.print("Up");
    screen.setCursor(firstDigit + digitsWidth, baseline);
    screen.print(suffix);
    cache[10] = suffix;
  }
  for (size_t i = 0; i < length; ++i) {
    if (!layoutChanged && uptimeDigits[i] == digits[i]) continue;
    const int x = firstDigit + static_cast<int>(i) * cellWidth;
    const auto& glyph = glyphFor(digits[i]);
    screen.fillRect(x, 223, cellWidth, 16, background);
    screen.setCursor(x + (cellWidth - glyph.width) / 2 - glyph.xOffset, baseline);
    screen.write(static_cast<uint8_t>(digits[i]));
  }
  memcpy(uptimeDigits, digits, sizeof(digits));
  panelsReady = true;
}

void Interface::clearMotionBody() {
  motionNoticeWaiting = false;
  motionNoticeAnimation.stop();
  screen.fillRect(34, 43, 252, 173, background);
  for (uint8_t slot = 1; slot < 14; ++slot) cache[slot] = "";
}

void Interface::motionAttitude(int roll, int pitch) {
  constexpr int radius = 57, diameter = radius * 2 + 1;
  constexpr float radians = 0.0174532925f;
  GFXcanvas16 ball(diameter, diameter);
  if (!ball.getBuffer()) return;
  ball.fillScreen(background);

  const float sine = sinf(roll * radians);
  const float cosine = cosf(roll * radians);
  const float horizon = pitch * 1.2f;
  for (int y = -radius; y <= radius; ++y) {
    const int edge = static_cast<int>(sqrtf(radius * radius - y * y));
    for (int x = -edge; x <= edge; ++x) {
      const float normal = y * cosine - x * sine - horizon;
      const float across = x * cosine + y * sine;
      uint16_t color = normal < 0 ? 0x259B : 0xA3C8;
      if (fabsf(normal) < 0.8f ||
          (fabsf(across) < 13 && fabsf(fabsf(normal) - 15) < 0.65f) ||
          (fabsf(across) < 8 && fabsf(fabsf(normal) - 30) < 0.65f)) {
        color = ILI9341_WHITE;
      }
      ball.drawPixel(x + radius, y + radius, color);
    }
  }
  ball.drawCircle(radius, radius, radius, 0xD69A);
  constexpr uint16_t marker = 0xFFE0;
  ball.drawFastHLine(radius - 25, radius, 17, marker);
  ball.drawFastHLine(radius + 9, radius, 17, marker);
  ball.drawFastVLine(radius - 9, radius, 5, marker);
  ball.drawFastVLine(radius + 9, radius, 5, marker);
  ball.fillCircle(radius, radius, 2, marker);
  screen.drawRGBBitmap(160 - radius, 72, ball.getBuffer(), diameter, diameter);
  field(8, 49, 188, 100, 24, String("Roll ") + String(roll) + " deg", 4, ILI9341_WHITE, true);
  field(9, 171, 188, 100, 24, String("Pitch ") + String(pitch) + " deg", 4, ILI9341_WHITE, true);
}

void Interface::motionLevel(int roll, int pitch) {
  constexpr int radius = 55, size = radius * 2 + 1;
  constexpr float radians = 0.0174532925f;
  const bool flat = abs(roll) <= 1 && abs(pitch) <= 1;
  const uint16_t color = flat ? 0x57A8 : 0xE73C;
  GFXcanvas16 dial(size, size);
  if (!dial.getBuffer()) return;
  dial.fillScreen(background);
  const uint16_t fill = flat ? 0x2385 : 0x2126;
  dial.fillCircle(radius, radius, radius, fill);
  dial.drawCircle(radius, radius, radius, color);
  dial.drawCircle(radius, radius, 12, color);
  dial.drawFastHLine(radius - 17, radius, 35, color);
  dial.drawFastVLine(radius, radius - 17, 35, color);

  // 倾角越大，气泡越靠外；将气泡限制在圆盘内。
  const float x = sinf(roll * radians) * cosf(pitch * radians);
  const float y = -sinf(pitch * radians);
  constexpr float travel = radius - 12;
  const float scale = travel / max(0.5f, hypotf(x, y));
  const int bubbleX = radius + lroundf(x * scale);
  const int bubbleY = radius + lroundf(y * scale);
  dial.fillCircle(bubbleX, bubbleY, 9, color);
  dial.drawCircle(bubbleX, bubbleY, 9, ILI9341_WHITE);
  screen.drawRGBBitmap(160 - radius, 73, dial.getBuffer(), size, size);
  field(8, 49, 188, 100, 24, String("Roll ") + String(roll) + " deg", 4, ILI9341_WHITE, true);
  field(9, 171, 188, 100, 24, String("Pitch ") + String(pitch) + " deg", 4, ILI9341_WHITE, true);
}

void Interface::motionConnection(const char* state) {
  if (cache[0] == state) return;
  constexpr uint16_t frame = 0x630C, label = 0xAD55;
  screen.drawRoundRect(174, 12, 108, 20, 6, frame);
  field(0, 180, 14, 96, 16, state, 3, label, true);
}

void Interface::motionMode(const char* mode) {
  if (motionModeTarget && strcmp(motionModeTarget, mode) == 0) return;
  motionModeTarget = mode;
  if (!motionModeShown) {
    renderMotionMode(12);
  } else {
    motionModeAnimation.start(millis(), 240, 12);
  }
}

void Interface::renderMotionMode(int step) {
  constexpr int fullWidth = 124, height = 19, y = 218;
  if (step >= 6) motionModeShown = motionModeTarget;
  const float phase = abs(step - 6) / 6.0f;
  const int width = 14 + lroundf((fullWidth - 14) * smoothStep(phase));
  screen.fillRect(160 - fullWidth / 2, y, fullWidth, height, background);
  screen.fillRoundRect(160 - width / 2, y, width, height, min(8, width / 2), 0x4168);
  screen.drawRoundRect(160 - width / 2, y, width, height, min(8, width / 2), highlight);
  screen.setFont(&NunitoHint);
  int16_t x1, y1;
  uint16_t w, h;
  screen.getTextBounds(motionModeShown, 0, 0, &x1, &y1, &w, &h);
  if (w + 10 > width) return;
  screen.setTextColor(ILI9341_WHITE);
  screen.setCursor(160 - w / 2 - x1, y + (height - h) / 2 - y1);
  screen.print(motionModeShown);
}

void Interface::motionCalibrationGuide(bool negative, int reading, bool valid, bool aligned) {
  constexpr int x = 196, y = 81, width = 80, height = 32;
  const int position = valid ? 5 + (constrain(reading, -20000, 20000) + 20000) * 69 / 40000 : -1;
  const String key = String(negative) + "/" + String(position) + "/" + String(aligned);
  if (cache[13] == key) return;
  GFXcanvas16 guide(width, height);
  if (!guide.getBuffer()) return;
  cache[13] = key;
  guide.fillScreen(background);
  guide.fillRoundRect(3, 9, 74, 12, 6, 0x3246);
  // 目标区对应校准判定的 ±12000 门槛，圆点显示当前目标轴读数。
  guide.fillRoundRect(negative ? 3 : 60, 9, 17, 12, 6, 0x2385);
  guide.drawFastVLine(40, 8, 14, highlight);
  if (valid) guide.fillCircle(position, 15, 4, aligned ? 0x57A8 : ILI9341_WHITE);
  guide.drawFastHLine(4, 27, 5, ILI9341_WHITE);
  guide.drawFastHLine(70, 27, 5, ILI9341_WHITE);
  guide.drawFastVLine(72, 25, 5, ILI9341_WHITE);
  screen.drawRGBBitmap(x, y, guide.getBuffer(), width, height);
}

void Interface::motionTube(bool horizontal, int angle) {
  constexpr int length = 108, thickness = 33;
  const int width = horizontal ? length : thickness;
  const int height = horizontal ? thickness : length;
  const bool centered = abs(angle) <= 1;
  const uint16_t color = centered ? 0x57A8 : 0xE73C;
  GFXcanvas16 tube(width, height);
  if (!tube.getBuffer()) return;
  tube.fillScreen(background);
  tube.fillRoundRect(0, 0, width, height, 16, centered ? 0x2385 : 0x2126);
  tube.drawRoundRect(0, 0, width, height, 16, color);
  for (int offset : {-12, 12}) {
    if (horizontal) tube.drawFastVLine(width / 2 + offset, 2, height - 4, color);
    else tube.drawFastHLine(2, height / 2 + offset, width - 4, color);
  }
  const int travel = constrain(angle * 2, -42, 42);
  const int x = width / 2 + (horizontal ? travel : 0);
  const int y = height / 2 - (horizontal ? 0 : travel);
  tube.fillCircle(x, y, 9, color);
  tube.drawCircle(x, y, 9, ILI9341_WHITE);
  screen.drawRGBBitmap(horizontal ? 149 : 77, horizontal ? 110 : 73, tube.getBuffer(), width, height);
  field(horizontal ? 9 : 8, horizontal ? 149 : 43, 188, 108, 24,
        String(horizontal ? "Z  " : "X  ") + String(angle) + " deg", 4, ILI9341_WHITE, true);
}

void Interface::motionNotice(const char* message) {
  motionNoticeAnimation.stop();
  motionNoticeText = message;
  motionNoticeStarted = millis();
  motionNoticeWaiting = true;
  renderMotionNotice(0);
}

void Interface::renderMotionNotice(int offset) {
  constexpr int width = 220, height = 15;
  // 裁切到状态胶囊上沿，提示滑下时不会覆盖胶囊。
  GFXcanvas16 strip(width, height);
  if (!strip.getBuffer()) return;
  strip.fillScreen(background);
  strip.setTextWrap(false);
  strip.setFont(&NunitoHint);
  strip.setTextColor(ILI9341_WHITE);
  const char* message = motionNoticeText;
  int16_t x, y;
  uint16_t w, h;
  strip.getTextBounds(message, 0, 0, &x, &y, &w, &h);
  strip.setCursor((width - w) / 2 - x, (height - h) / 2 - y + offset);
  strip.print(message);
  screen.drawRGBBitmap(50, 201, strip.getBuffer(), width, height);
}

void Interface::motionCalibrationPrompt(const char* message, CalibrationTone tone, int offset) {
  const bool bold = tone != CalibrationTone::Normal;
  const String key = String(message) + '/' + String(static_cast<int>(tone)) + '/' + String(offset);
  if (cache[6] == key) return;
  constexpr int width = 248, height = 17;
  GFXcanvas16 line(width, height);
  if (!line.getBuffer()) return;
  cache[6] = key;
  line.fillScreen(background);
  line.setTextWrap(false);
  line.setFont(&NunitoHint);
  line.setTextColor(tone == CalibrationTone::Alert ? 0xF986 :
                    tone == CalibrationTone::Success ? 0x57A8 : ILI9341_WHITE);
  int16_t x, y;
  uint16_t w, h;
  line.getTextBounds(message, 0, 0, &x, &y, &w, &h);
  const int left = (width - w - (bold ? 1 : 0)) / 2 - x + offset;
  const int baseline = (height - h) / 2 - y;
  line.setCursor(left, baseline);
  line.print(message);
  if (bold) {
    line.setCursor(left + 1, baseline);
    line.print(message);
  }
  screen.drawRGBBitmap(36, 198, line.getBuffer(), width, height);
}

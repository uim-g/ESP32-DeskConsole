#include "Interface.h"
#include "NunitoFonts.h"

namespace {
void text(Adafruit_GFX& gfx, String value, const GFXfont* font,
          int x, int y, int width, int height, uint16_t color, bool centered = false) {
  gfx.setFont(font);
  gfx.setTextColor(color);
  int16_t dx, dy;
  uint16_t w, h;
  gfx.getTextBounds(value.c_str(), 0, 0, &dx, &dy, &w, &h);
  while (w > width && value.length()) {
    value.remove(value.length() - 1);
    gfx.getTextBounds(value.c_str(), 0, 0, &dx, &dy, &w, &h);
  }
  gfx.setCursor(x - dx + (centered ? (width - w) / 2 : 0), y - dy + (height - h) / 2);
  gfx.print(value);
}
constexpr int rowHeight = 44, rowPitch = 51, rowWidth = 236;
constexpr int wheelPitch = 76;
constexpr int listX = 42, listY = 46, switchX = 182, switchY = 11;
constexpr int knobOff = 12, knobOn = 32;

int measureName(Adafruit_GFX& gfx, const String& value) {
  gfx.setFont(&NunitoSmall);
  int16_t dx, dy;
  uint16_t w, h;
  gfx.getTextBounds(value.c_str(), 0, 0, &dx, &dy, &w, &h);
  return w;
}
}

// 名称先停留，再匀速滚动；尾部留白后接上下一轮。
int Interface::nameOffset(int width, int viewport, uint32_t elapsed) {
  if (width <= viewport) return 0;
  constexpr uint32_t pause = 900, msPerPixel = 45;
  const uint32_t travel = (width + 24) * msPerPixel;
  const uint32_t phase = elapsed % (pause + travel);
  return phase < pause ? 0 : (phase - pause) / msPerPixel;
}

// 通过画布裁切文字，避免覆盖相邻字段、开关或边缘按键。
void Interface::paintName(Adafruit_GFX& gfx, const String& value, int x, int y,
                          int w, int h, int offset, uint16_t color) {
  const int width = measureName(gfx, value);
  if (width <= w) {
    text(gfx, value, &NunitoSmall, x, y, w, h, color);
    return;
  }
  GFXcanvas1 strip(w, h);
  if (!strip.getBuffer()) {
    text(gfx, value, &NunitoSmall, x, y, w, h, color);
    return;
  }
  strip.setTextWrap(false);
  strip.fillScreen(0);
  text(strip, value, &NunitoSmall, -offset, 0, width, h, 1);
  text(strip, value, &NunitoSmall, width + 24 - offset, 0, width, h, 1);
  gfx.drawBitmap(x, y, strip.getBuffer(), w, h, color);
}

void Interface::scrollingName(int slot, int x, int y, int w, int h, const String& value,
                              uint16_t fill, bool centered) {
  auto& name = scrollingNames[slot];
  if (name.value == value) return;
  name = {value, x, y, w, h, measureName(screen, value), -1, fill, millis(), centered};
  if (!value.length()) screen.fillRect(x, y, w, h, fill);
  renderScrollingNames(name.started);
}

void Interface::renderScrollingNames(uint32_t now) {
  for (auto& name : scrollingNames) {
    if (!name.value.length()) continue;
    const int offset = nameOffset(name.width, name.w, now - name.started);
    if (name.offset == offset) continue;
    name.offset = offset;
    GFXcanvas16 strip(name.w, name.h);
    if (!strip.getBuffer()) {
      screen.fillRect(name.x, name.y, name.w, name.h, name.fill);
      text(screen, name.value, &NunitoSmall, name.x, name.y, name.w, name.h,
           ILI9341_WHITE, name.centered);
      continue;
    }
    strip.setTextWrap(false);
    strip.fillScreen(name.fill);
    if (name.centered && name.width <= name.w)
      text(strip, name.value, &NunitoSmall, 0, 0, name.w, name.h, ILI9341_WHITE, true);
    else paintName(strip, name.value, 0, 0, name.w, name.h, offset, ILI9341_WHITE);
    screen.drawRGBBitmap(name.x, name.y, strip.getBuffer(), name.w, name.h);
  }
}

void Interface::wifiStatus(bool enabled, bool connected) {
  const int state = !enabled ? 0 : connected ? 2 : 1;
  if (state == wifiConnectionBadge) return;
  wifiConnectionBadge = state;
  const uint16_t tint = state == 2 ? 0x7DEB : muted;
  screen.fillRoundRect(174, 9, 106, 24, 9, state == 2 ? 0x1248 : cardColor);
  text(screen, state == 2 ? "Connected" : state == 1 ? "Disconnected" : "Off",
       &NunitoHint, 178, 9, 98, 24, tint, true);
}

// 保存本次绘制快照，并在选择变动时启动高亮动画。时间轴由主循环统一推进。
void Interface::wifiList(const WifiRow* rows, int count, int selected, const String& footer) {
  if (wifiListAnimation.active() && selected != wifiSelected) {
    wifiListAnimation.stop();
    renderWifiList(1.0f);
  }
  const int radio = count ? rows[0].radio : -1;
  if (radio != wifiSwitchTarget) {
    wifiSwitchAnimation.stop();
    if (radio >= 0) {
      if (wifiSwitchTarget < 0) wifiSwitchKnob = radio ? knobOn : knobOff;
      else {
        wifiSwitchFrom = wifiSwitchKnob;
        wifiSwitchAnimation.start(millis(), 132, 8);
      }
    }
    wifiSwitchTarget = radio;
  }
  if (wifiRowCount != count) wifiNamesStarted = millis();
  for (int i = 0; i < count; ++i) {
    if (wifiRows[i].name != rows[i].name || wifiRowCount != count) {
      wifiNameWidths[i] = measureName(screen, rows[i].name);
      wifiNamesStarted = millis();
    }
    wifiRows[i] = rows[i];
  }
  wifiRowCount = count;
  if (wifiSelected != selected) {
    wifiPrevious = wifiSelected < 0 ? selected : wifiSelected;
    wifiSelected = selected;
    if (wifiPrevious != wifiSelected) {
      constexpr uint8_t steps = 9;
      constexpr uint16_t frameMs = 16;
      const uint32_t now = millis();
      wifiListAnimation.start(now - frameMs, steps * frameMs, steps);
      renderWifiList(wifiListAnimation.progress(wifiListAnimation.frame(now)));
    } else renderWifiList(1.0f);
  } else if (!wifiListAnimation.active()) renderWifiList(1.0f);
  field(6, 40, 205, 240, 23, footer, 4, muted, true);
}

// 把高亮遮罩与圆角卡片遮罩相交，再根据像素位置决定文字颜色。缓存命中的卡片跳过传输。
void Interface::renderWifiList(float progress, bool namesOnly) {
  constexpr int x = listX, firstY = listY;
  const int marker = lroundf((wifiPrevious + (wifiSelected - wifiPrevious) * smoothStep(progress)) * rowPitch);
  for (int row = 0; row < 3; ++row) {
    const int y = firstY + row * rowPitch;
    if (row >= wifiRowCount) {
      if (cache[row].length()) {
        screen.fillRect(x, y, rowWidth, rowHeight, background);
        cache[row] = "";
      }
      continue;
    }
    const auto& item = wifiRows[row];
    const int offset = marker - row * rowPitch;
    const int visibleOffset = abs(offset) < rowHeight ? offset : rowHeight;
    const int textWidth = item.radio >= 0 ? 171 : 218;
    const int scroll = nameOffset(wifiNameWidths[row], textWidth, millis() - wifiNamesStarted);
    if (namesOnly) {
      if (wifiNameOffsets[row] == scroll) continue;
      const bool active = row == wifiSelected;
      GFXcanvas16 strip(textWidth, 21);
      if (!strip.getBuffer()) continue;
      strip.setTextWrap(false);
      strip.fillScreen(active ? highlight : cardColor);
      paintName(strip, item.name, 0, 0, textWidth, 21, scroll, active ? ILI9341_BLACK : ILI9341_WHITE);
      screen.drawRGBBitmap(x + 9, y + 3, strip.getBuffer(), textWidth, 21);
      wifiNameOffsets[row] = scroll;
      continue;
    }
    const String key = String(visibleOffset) + "/" + item.radio + ":" + item.name + '\n' + item.detail;
    if (cache[row] == key && wifiNameOffsets[row] == scroll) continue;
    wifiNameOffsets[row] = scroll;

    GFXcanvas16 frame(rowWidth, rowHeight);
    GFXcanvas1 shape(rowWidth, rowHeight);
    GFXcanvas1 lit(rowWidth, rowHeight);
    GFXcanvas1 ink(rowWidth, rowHeight);
    if (!frame.getBuffer() || !shape.getBuffer() || !lit.getBuffer() || !ink.getBuffer()) {
      const bool active = row == wifiSelected;
      screen.fillRoundRect(x, y, rowWidth, rowHeight, 7, active ? highlight : cardColor);
      paintName(screen, item.name, x + 9, y + 3, textWidth, 21, scroll,
                active ? ILI9341_BLACK : ILI9341_WHITE);
      text(screen, item.detail, &NunitoHint, x + 9, y + 24, item.radio >= 0 ? 171 : 218, 15,
           active ? ILI9341_BLACK : muted);
      if (item.radio >= 0) paintSwitch(screen, x + switchX, y + switchY, 22, wifiSwitchKnob, 8);
      cache[row] = key;
      continue;
    }
    shape.fillScreen(0);
    lit.fillScreen(0);
    ink.fillScreen(0);
    ink.setTextWrap(false);
    shape.fillRoundRect(0, 0, rowWidth, rowHeight, 7, 1);
    if (visibleOffset != rowHeight) lit.fillRoundRect(0, visibleOffset, rowWidth, rowHeight, 7, 1);
    paintName(ink, item.name, 9, 3, textWidth, 21, scroll, 1);
    text(ink, item.detail, &NunitoHint, 9, 24, textWidth, 15, 1);
    frame.fillScreen(background);
    frame.drawRoundRect(0, 0, rowWidth, rowHeight, 7, cardOutline);
    for (int py = 0; py < rowHeight; ++py) {
      for (int px = 0; px < rowWidth; ++px) {
        if (!shape.getPixel(px, py)) continue;
        const bool selected = lit.getPixel(px, py);
        if (ink.getPixel(px, py)) {
          frame.drawPixel(px, py, selected ? ILI9341_BLACK : py < 24 ? ILI9341_WHITE : muted);
        } else if (selected) frame.drawPixel(px, py, highlight);
        else if (frame.getPixel(px, py) == background) frame.drawPixel(px, py, cardColor);
      }
    }
    if (item.radio >= 0) paintSwitch(frame, switchX, switchY, 22, wifiSwitchKnob, 8);
    screen.drawRGBBitmap(x, y, frame.getBuffer(), rowWidth, rowHeight);
    cache[row] = key;
  }
}

// 拨钮按缓动曲线移动。只更新轨道的固定小区域，选中卡片和其他内容由列表绘制负责。
void Interface::renderWifiSwitch(float progress) {
  if (wifiSwitchTarget < 0) return;
  const int target = wifiSwitchTarget ? knobOn : knobOff;
  const int position = wifiSwitchFrom + lroundf((target - wifiSwitchFrom) * smoothStep(progress));
  if (position == wifiSwitchKnob) return;
  wifiSwitchKnob = position;
  // 拨钮和轨道共用固定矩形，动画帧无需重绘卡片。
  paintSwitch(screen, listX + switchX, listY + switchY, 22, wifiSwitchKnob, 8);
}

void Interface::wifiMessage(const String& title, const String& detail, const String& footer) {
  field(0, 35, 77, 250, 37, title, 1, highlight, true);
  scrollingName(0, 36, 121, 248, 31, detail, background, true);
  field(6, 38, 199, 244, 28, footer, 4, muted, true);
}

void Interface::wifiPassword(const String& ssid, const String& value, size_t length, int group,
                             const String& previous, const String& selected, const String& next,
                             int direction, const char* footer) {
  if (!panelsReady) {
    card(40, 69, 240, 43);
    panelsReady = true;
  }
  scrollingName(0, 39, 40, 242, 25, ssid, background, true);
  field(1, 49, 73, 222, 23, value.length() ? value : String("Enter password"), 0,
        value.length() ? ILI9341_WHITE : muted, false, true);
  field(2, 49, 96, 222, 12, String(length) + " chars", 4, muted, false, true);

  if (wifiGroup != group) {
    wifiPreviousGroup = wifiGroup < 0 ? group : wifiGroup;
    wifiGroup = group;
    if (wifiGroup == wifiPreviousGroup) renderWifiGroups(1.0f);
    else wifiGroupAnimation.start(millis(), 144, 9);
  }

  if (wifiWheel[0] != previous || wifiWheel[1] != selected || wifiWheel[2] != next) {
    for (int i = 0; i < 3; ++i) wifiPreviousWheel[i] = wifiWheel[i];
    wifiWheel[0] = previous;
    wifiWheel[1] = selected;
    wifiWheel[2] = next;
    wifiWheelDirection = direction;
    wifiWheelAnimation.stop();
    if (direction && wifiPreviousWheel[1].length()) wifiWheelAnimation.start(millis(), 128, 8);
    else renderWifiWheel(1.0f);
  }
  field(8, 38, 185, 244, 17, "Space / Delete / Show / Connect", 4, muted, true);
  field(6, 38, 203, 244, 26, footer, 4, muted, true);
}

// 在四个字符组之间移动圆角框，保留每一组的固定文字位置。
void Interface::renderWifiGroups(float progress) {
  constexpr int width = 232, height = 25, pitch = 58;
  constexpr const char* labels[] = {"abc", "ABC", "123", "+=#"};
  const int x = lroundf((wifiPreviousGroup + (wifiGroup - wifiPreviousGroup) * smoothStep(progress)) * pitch);
  GFXcanvas16 strip(width, height);
  if (!strip.getBuffer()) return;
  strip.setTextWrap(false);
  strip.fillScreen(background);
  strip.drawRoundRect(x + 5, 2, pitch - 10, height - 4, 6, highlight);
  for (int i = 0; i < 4; ++i) {
    text(strip, labels[i], &NunitoHint, i * pitch, 0, pitch, height,
         i == wifiGroup ? highlight : muted, true);
  }
  screen.drawRGBBitmap(44, 115, strip.getBuffer(), width, height);
}

// 离屏条带裁切滑出边缘的候选字符。中心卡位固定，旧字符和新字符按方向连续滑动。
void Interface::renderWifiWheel(float progress) {
  constexpr int width = 240, height = 41;
  GFXcanvas16 strip(width, height);
  if (!strip.getBuffer()) return;
  strip.setTextWrap(false);
  strip.fillScreen(background);
  strip.fillRoundRect(81, 2, 78, height - 4, 7, 0x2450);
  const int direction = wifiWheelDirection;
  const int shift = direction * lroundf(wheelPitch * smoothStep(progress));
  auto paint = [&](const String& token, int center) {
    if (center + wheelPitch / 2 < 0 || center - wheelPitch / 2 >= width) return;
    const bool focused = abs(center - width / 2) < wheelPitch / 2;
    text(strip, token, focused ? &NunitoSmall : &NunitoHint, center - wheelPitch / 2, 0,
         wheelPitch, height, focused ? ILI9341_WHITE : muted, true);
  };
  if (!direction || progress >= 1.0f) {
    for (int i = 0; i < 3; ++i) paint(wifiWheel[i], width / 2 + (i - 1) * wheelPitch);
  } else {
    for (int i = 0; i < 3; ++i) paint(wifiPreviousWheel[i], width / 2 + (i - 1) * wheelPitch - shift);
    paint(wifiWheel[direction > 0 ? 2 : 0], width / 2 + direction * 2 * wheelPitch - shift);
  }
  screen.drawRGBBitmap(40, 143, strip.getBuffer(), width, height);
}

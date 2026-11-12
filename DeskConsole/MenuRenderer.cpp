#include "Interface.h"
#include "NunitoFonts.h"

void Interface::launcher(uint8_t selected, const char* const labels[], int count) {
  if (launcherAnimation.active()) {
    if (launcherTarget == selected) return;
    launcherAnimation.stop();
    renderLauncher(1.0f);
  }
  if (lastSelection == selected) return;
  launcherTarget = selected;
  launcherLabels = labels;
  launcherCount = count;
  if (lastSelection < 0) {
    renderLauncher(1.0f);
  } else {
    const bool wrap = (lastSelection == count - 1 && selected == 0) ||
                      (lastSelection == 0 && selected == count - 1);
    const bool scroll = selected < launcherTop || selected >= launcherTop + 4;
    const uint8_t steps = wrap ? 8 : scroll ? 5 : 9;
    // v1.5.0 先画第一步，再按帧间隔推进，滚动帧较少时也更快收尾。
    constexpr uint16_t frameMs = 16;
    const uint32_t now = millis();
    launcherAnimation.start(now - frameMs, steps * frameMs, steps);
    renderLauncher(launcherAnimation.progress(launcherAnimation.frame(now)));
  }
}

void Interface::renderLauncher(float progress) {
  const int selected = launcherTarget;
  const auto labels = launcherLabels;
  const int count = launcherCount;
  constexpr int cardX = 46, firstY = 49, width = 228, height = 36, pitch = 43;
  constexpr int viewportEnd = firstY + 4 * pitch;
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
    } else if (row == 5) {
      smallIcon(ink, x, cy, 'g', 1);
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

  const float eased = smoothStep(progress);
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
  constexpr int visibleRows = 4;
  if (count > visibleRows) {
    constexpr int barX = 282, barWidth = 4, radius = 2;
    constexpr int trackHeight = (visibleRows - 1) * pitch + height;
    const int thumbHeight = trackHeight * visibleRows / count;
    const float visibleTop = lastSelection < 0 ? nextTop : launcherTop + (nextTop - launcherTop) * eased;
    const int thumbY = firstY + lroundf(visibleTop * (trackHeight - thumbHeight) / (count - visibleRows));
    if (thumbY != launcherThumbY) {
      screen.fillRoundRect(barX, firstY, barWidth, trackHeight, radius, 0x3A4E);
      screen.fillRoundRect(barX, thumbY, barWidth, thumbHeight, radius, highlight);
      launcherThumbY = thumbY;
    }
  }
  if (progress >= 1.0f) {
    lastSelection = selected;
    launcherTop = nextTop;
  }
}

void Interface::settings(const Settings& values, uint8_t selected, bool editing, bool saveFailed) {
  if (settingsAnimation.active()) {
    settingsAnimation.stop();
    renderSettings(1.0f);
  }
  settingsValues = values;
  settingsTarget = selected;
  settingsEditing = editing;
  settingsFailed = saveFailed;

  const bool moved = lastSettingRow >= 0 && lastSettingRow != selected;
  const bool toggled = lastSettingRow >= 0 &&
      (settingSwitches[0] != (values.autoSleep ? 32 : 12) ||
       settingSwitches[1] != (values.rgbEnabled ? 32 : 12));
  const bool rolled = editing &&
      ((selected == 1 && lastSleepMinutes >= 1 && lastSleepMinutes != values.sleepMinutes()) ||
       (selected == 3 && lastBrightness >= 1 && lastBrightness != values.brightness));
  if (moved || toggled || rolled) {
    settingsAnimation.start(millis(), moved ? 120 : 96, rolled ? 8 : 6);
  } else renderSettings(1.0f);
}

void Interface::renderSettings(float progress) {
  const auto& values = settingsValues;
  const int selected = settingsTarget;
  const bool editing = settingsEditing, saveFailed = settingsFailed;
  const float eased = smoothStep(progress);
  constexpr int x = 37, firstY = 44, pitch = 39, width = 246, height = 37;
  constexpr uint16_t selection = 0x2A69;
  const char* labels[] = {"Auto sleep", "Sleep after", "RGB status LED", "RGB brightness"};
  const bool available[] = {true, values.autoSleep != 0, true, values.rgbEnabled != 0};
  GFXcanvas16 strip(width, pitch);
  strip.setTextWrap(false);

  auto paint = [&](int row, int highlightY, int knob) {
    const int y = firstY + row * pitch;
    if (strip.getBuffer()) {
      strip.fillScreen(background);
      // 分隔线先画，滑动的高亮经过时会覆盖它。
      strip.drawFastHLine(5, 37, 235, 0x2946);
      strip.fillRoundRect(0, highlightY - y, width, height, 7, selection);
    }
    Adafruit_GFX& gfx = strip.getBuffer() ? static_cast<Adafruit_GFX&>(strip) : screen;
    const int ox = strip.getBuffer() ? 0 : x;
    const int oy = strip.getBuffer() ? 0 : y;
    if (!strip.getBuffer()) {
      screen.fillRect(x, y, width, height, background);
      screen.drawFastHLine(42, y + 37, 235, 0x2946);
      if (row == selected) screen.fillRoundRect(x, y, width, height, 7, selection);
    }
    gfx.setFont(&NunitoSmall);
    gfx.setTextColor(available[row] ? ILI9341_WHITE : muted);
    gfx.setCursor(ox + 10, oy + 24);
    gfx.print(labels[row]);
    if (row % 2 == 0) {
      gfx.fillRoundRect(ox + 192, oy + 6, 44, 24, 12, knob >= 22 ? 0x3D6C : 0x528A);
      gfx.fillCircle(ox + 192 + knob, oy + 18, 9, ILI9341_WHITE);
    } else {
      gfx.setTextColor(!available[row] ? muted : editing && row == selected ? highlight : ILI9341_WHITE);
      gfx.setCursor(ox + 170, oy + 24);
      if (row == 1) gfx.printf("%u min", static_cast<unsigned>(values.sleepMinutes()));
      else gfx.print(values.brightness);
      if (available[row]) {
        gfx.drawLine(ox + 228, oy + 13, ox + 233, oy + 18, highlight);
        gfx.drawLine(ox + 233, oy + 18, ox + 228, oy + 23, highlight);
      }
    }
    if (strip.getBuffer()) screen.drawRGBBitmap(x, y, strip.getBuffer(), width, pitch);
  };

  const int knobs[] = {values.autoSleep ? 32 : 12, values.rgbEnabled ? 32 : 12};
  const int targetY = firstY + selected * pitch;
  if (strip.getBuffer() && lastSettingRow >= 0 && lastSettingRow != selected) {
    const int direction = selected == 0 && lastSettingRow == 3 ? 1 :
                          selected == 3 && lastSettingRow == 0 ? -1 : selected - lastSettingRow;
    constexpr int cycle = 4 * pitch;
    const int pos = firstY + lastSettingRow * pitch + lroundf(direction * pitch * eased);
    for (int row : {lastSettingRow, selected}) {
      const int y = firstY + row * pitch;
      const int relative = (pos - y + cycle + cycle / 2) % cycle - cycle / 2;
      paint(row, y + relative, knobs[row / 2]);
    }
    if (progress < 1.0f) return;
  }

  const bool rollingSleep = selected == 1;
  const int previousValue = rollingSleep ? lastSleepMinutes : lastBrightness;
  const int currentValue = rollingSleep ? values.sleepMinutes() : values.brightness;
  const bool roll = editing && (rollingSleep || selected == 3) && previousValue >= 1 &&
                    previousValue != currentValue && strip.getBuffer();
  for (int row = 0; row < 4; ++row) {
    if (roll) break;  // 连调时只写数值窗口，保留整行其余内容。
    const int knob = knobs[row / 2];
    const int previous = settingSwitches[row / 2];
    if (progress < 1.0f) {
      // 开关滑动期间只传输拨钮区域。
      if (row % 2 != 0 || previous < 0 || previous == knob) continue;
      GFXcanvas16 toggle(44, 24);
      if (toggle.getBuffer()) {
        const int current = previous + lroundf((knob - previous) * eased);
        toggle.fillScreen(row == selected ? selection : background);
        toggle.fillRoundRect(0, 0, 44, 24, 12, current >= 22 ? 0x3D6C : 0x528A);
        toggle.fillCircle(current, 12, 9, ILI9341_WHITE);
        screen.drawRGBBitmap(229, firstY + row * pitch + 6, toggle.getBuffer(), 44, 24);
      }
      continue;
    }
    paint(row, targetY, knob);
    if (row % 2 == 0) settingSwitches[row / 2] = knob;
  }

  if (roll) {
    // 数字画布独立裁剪，旧数字退出时变淡，新数字从另一端进入。
    GFXcanvas16 digits(56, 31);
    if (digits.getBuffer()) {
      digits.setFont(&NunitoSmall);
      digits.setTextWrap(false);
      const int direction = currentValue < previousValue ? 1 : -1;
      auto tint = [&](int alpha) {
        const int inverse = 8 - alpha;
        const int r = (((highlight >> 11) & 31) * alpha + ((selection >> 11) & 31) * inverse) / 8;
        const int g = (((highlight >> 5) & 63) * alpha + ((selection >> 5) & 63) * inverse) / 8;
        const int b = ((highlight & 31) * alpha + (selection & 31) * inverse) / 8;
        return static_cast<uint16_t>((r << 11) | (g << 5) | b);
      };
      const int offset = lroundf(27 * eased);
      const int alpha = lroundf(8 * eased);
      digits.fillScreen(selection);
      digits.setTextColor(tint(8 - alpha));
      digits.setCursor(0, 21 - direction * offset);
      digits.print(previousValue);
      if (rollingSleep) digits.print(" min");
      digits.setTextColor(tint(alpha));
      digits.setCursor(0, 21 + direction * (27 - offset));
      digits.print(currentValue);
      if (rollingSleep) digits.print(" min");
      screen.drawRGBBitmap(207, firstY + selected * pitch + 3, digits.getBuffer(), 56, 31);
    } else {
      paint(selected, targetY, knobs[selected / 2]);
    }
  }
  if (progress < 1.0f) return;
  lastSettingRow = selected;
  lastBrightness = values.brightness;
  lastSleepMinutes = values.sleepMinutes();
  field(0, 39, 207, 242, 24, saveFailed ? "Save failed; try again" :
        editing ? "EDIT confirms / BACK cancels" : "Changes saved automatically",
        4, saveFailed ? ILI9341_RED : muted, true);
}

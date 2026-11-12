#include "Interface.h"
#include "NunitoFonts.h"

void Interface::smallIcon(Adafruit_GFX& gfx, int x, int y, char kind, uint16_t color) {
  switch (kind) {
    case 'g':  // 齿轮
      gfx.drawCircle(x, y, 6, color);
      gfx.drawCircle(x, y, 2, color);
      for (int d : {-1, 1}) {
        gfx.drawFastHLine(x + d * 6 - 1, y, 3, color);
        gfx.drawFastVLine(x, y + d * 6 - 1, 3, color);
        for (int dy : {-1, 1}) gfx.drawPixel(x + d * 5, y + dy * 5, color);
      }
      break;
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
  tabAnimation.stop();
  launcherAnimation.stop();
  settingsAnimation.stop();
  replacementLabel = nullptr;
  // 真正切页才清屏。换页后所有绘制缓存需要重新建立。
  background = base;
  highlight = color;
  for (auto& text : cache) text = "";
  lastLights = -1;
  lastSelection = -1;
  launcherTop = 0;
  for (auto& value : settingSwitches) value = -1;
  lastSettingRow = lastBrightness = lastSleepMinutes = -1;
  panelsReady = false;
  uptimeDigits[0] = '\0';
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
                        strcmp(title, "Settings") == 0 ? 'g' :
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

void Interface::startTabs(bool entering, uint8_t mask) {
  if (!entering && mask == 0x0F) replacementLabel = nullptr;
  tabsEntering = entering;
  tabMask = mask;
  tabAnimation.stop();
  for (uint8_t i = 0; i < 4; ++i) {
    if ((mask & (1U << i)) && tabs[i]) {
      tabAnimation.start(millis(), 72, 6);
      renderTabs(entering ? 21 : 0, mask);
      return;
    }
  }
}

void Interface::replaceTab(uint8_t index, const char* label) {
  // 连续切换模式时，以当前最终目标作为下一次退场的标签。
  if (replacementLabel) tabs[replacementIndex] = replacementLabel;
  replacementIndex = index;
  replacementLabel = label;
  startTabs(false, 1U << index);
  if (!tabsAnimating()) {
    tabs[index] = label;
    replacementLabel = nullptr;
    startTabs(true, 1U << index);
  }
}

void Interface::advanceAnimations(uint32_t now) {
  int step;
  if ((step = tabAnimation.frame(now)) >= 0) {
    constexpr int slideOut[] = {0, 1, 3, 7, 12, 18, 21};
    constexpr int slideIn[] = {21, 15, 8, 2, -2, -1, 0};
    renderTabs(tabsEntering ? slideIn[step] : slideOut[step], tabMask);
    if (!tabAnimation.active() && !tabsEntering) {
      for (uint8_t i = 0; i < 4; ++i) if (tabMask & (1U << i)) tabs[i] = nullptr;
      if (replacementLabel) {
        tabs[replacementIndex] = replacementLabel;
        replacementLabel = nullptr;
        startTabs(true, tabMask);
      }
    }
  }
  if ((step = launcherAnimation.frame(now)) >= 0) renderLauncher(launcherAnimation.progress(step));
  if ((step = settingsAnimation.frame(now)) >= 0) renderSettings(settingsAnimation.progress(step));
}

void Interface::renderTabs(int offset, uint8_t mask) {
  bool any = false;
  for (uint8_t i = 0; i < 4; ++i) any |= (mask & (1U << i)) && tabs[i];
  if (!any) return;

  constexpr int width = 24, body = 21, maxHeight = 64;
  // 将约 16% 的主题色混入页面背景，让填充只隐约可见。
  const uint16_t red = (5 * ((background >> 11) & 31) + ((highlight >> 11) & 31) + 3) / 6;
  const uint16_t green = (5 * ((background >> 5) & 63) + ((highlight >> 5) & 63) + 3) / 6;
  const uint16_t blue = (5 * (background & 31) + (highlight & 31) + 3) / 6;
  const uint16_t fill = (red << 11) | (green << 5) | blue;
  constexpr uint16_t ink = ILI9341_WHITE;
  GFXcanvas16 patch(width, maxHeight);
  GFXcanvas1 letters(48, 14);
  if (!patch.getBuffer() || !letters.getBuffer()) return;
  letters.setTextWrap(false);
  letters.setTextColor(1);
  letters.setFont(&NunitoTab);

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
      patch.fillRoundRect(right ? x : x - 7, 0, body + 7, h, 7, fill);
      if (arrow) {
        const int direction = label[0] == '^' ? -1 : 1;
        patch.drawLine(cx - 5, center - direction * 3, cx, center + direction * 3, ink);
        patch.drawLine(cx, center + direction * 3, cx + 5, center - direction * 3, ink);
      } else if (minus) {
        patch.drawFastHLine(cx - 4, center, 9, ink);
      } else {
        if (apps) {
          for (int dy : {-4, 1}) {
            for (int dx : {-4, 1}) patch.drawRect(cx + dx, 9 + dy, 3, 3, ink);
          }
        } else if (back) {
          patch.drawFastHLine(cx - 4, 9, 9, ink);
          patch.drawLine(cx - 4, 9, cx, 6, ink);
          patch.drawLine(cx - 4, 9, cx, 12, ink);
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
                              ink);
            }
          }
        }
      }
    }
    const int y = (i % 2 ? 179 : 58) - h / 2;
    // 只传输本标签的矩形像素，中央内容保持原样。
    screen.drawRGBBitmap(right ? 320 - width : 0, y, patch.getBuffer(), width, h);
  }
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

void Interface::card(int x, int y, int w, int h) {
  screen.fillRoundRect(x, y, w, h, 9, cardColor);
  screen.drawRoundRect(x, y, w, h, 9, cardOutline);
}

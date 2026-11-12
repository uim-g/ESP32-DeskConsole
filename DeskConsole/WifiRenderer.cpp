#include "Interface.h"
#include "NunitoFonts.h"

namespace {
void text(Adafruit_GFX& gfx, String value, const GFXfont* font,
          int x, int y, int width, int height, uint16_t color) {
  gfx.setFont(font);
  gfx.setTextColor(color);
  int16_t dx, dy;
  uint16_t w, h;
  gfx.getTextBounds(value.c_str(), 0, 0, &dx, &dy, &w, &h);
  while (w > width && value.length()) {
    value.remove(value.length() - 1);
    gfx.getTextBounds(value.c_str(), 0, 0, &dx, &dy, &w, &h);
  }
  gfx.setCursor(x - dx, y - dy + (height - h) / 2);
  gfx.print(value);
}
}

void Interface::wifiList(const WifiRow* rows, int count, int selected, const String& footer) {
  constexpr int x = 42, width = 236, height = 44;
  for (int i = 0; i < 3; ++i) {
    const int y = 46 + i * 51;
    if (i >= count) {
      if (cache[i].length()) {
        screen.fillRect(x, y, width, height, background);
        cache[i] = "";
      }
      continue;
    }
    const bool active = i == selected;
    const String marker = String(active ? "+" : "-") + rows[i].name + '\n' + rows[i].detail;
    if (cache[i] == marker) continue;
    cache[i] = marker;
    const uint16_t fill = active ? highlight : cardColor;
    screen.fillRoundRect(x, y, width, height, 7, fill);
    if (!active) screen.drawRoundRect(x, y, width, height, 7, cardOutline);
    text(screen, rows[i].name, &NunitoSmall, x + 9, y + 3, width - 18, 21,
         active ? ILI9341_BLACK : ILI9341_WHITE);
    text(screen, rows[i].detail, &NunitoHint, x + 9, y + 24, width - 18, 15,
         active ? ILI9341_BLACK : muted);
  }
  field(6, 40, 205, 240, 23, footer, 4, muted, true);
}

void Interface::wifiMessage(const String& title, const String& detail, const String& footer) {
  field(0, 35, 77, 250, 37, title, 1, highlight, true);
  field(1, 36, 121, 248, 31, detail, 0, ILI9341_WHITE, true);
  field(6, 38, 199, 244, 28, footer, 4, muted, true);
}

void Interface::wifiPassword(const String& ssid, const String& value, size_t length,
                             const char* group, const String& previous, const String& selected,
                             const String& next, const char* footer) {
  if (!panelsReady) {
    card(40, 69, 240, 43);
    screen.fillRoundRect(116, 144, 88, 35, 7, highlight);
    panelsReady = true;
  }
  field(0, 39, 40, 242, 25, ssid, 0, ILI9341_WHITE, true);
  field(1, 49, 73, 222, 23, value.length() ? value : String("Enter password"), 0,
        value.length() ? ILI9341_WHITE : muted, false, true);
  field(2, 49, 96, 222, 12, String(length) + " chars", 4, muted, false, true);
  field(3, 44, 116, 232, 20, String("Group: ") + group + "  /  abc  ABC  123  #+=", 4, muted, true);
  field(4, 40, 144, 74, 35, previous, 4, muted, true);
  if (cache[5] != selected) {
    cache[5] = selected;
    screen.fillRect(123, 148, 74, 27, highlight);
    text(screen, selected, &NunitoSmall, 127, 148, 66, 27, ILI9341_BLACK);
  }
  field(7, 206, 144, 74, 35, next, 4, muted, true);
  field(8, 38, 181, 244, 17, "Group / Space / Delete / Show / Connect", 4, muted, true);
  field(6, 38, 198, 244, 29, footer, 4, muted, true);
}

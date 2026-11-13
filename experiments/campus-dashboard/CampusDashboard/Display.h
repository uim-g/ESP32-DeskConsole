#pragma once
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include "Config.h"
#include "Weather.h"

class Display {
 public:
  static constexpr uint8_t pageCount = 4;
  void begin();
  void show(uint8_t page, const WeatherData& weather, bool newPage);

 private:
  static constexpr int Width = 282;
  static constexpr int Height = 240;
  void rail(uint8_t page, float marker, bool redrawControls);
  void transfer(int offset);
  void render(uint8_t page, const WeatherData& weather, bool full);
  void field(uint8_t slot, int x, int y, int width, int height,
             const GFXfont* font, String text, uint16_t color, bool center = false);
  void icon(uint8_t slot, int x, int y, int code);
  Adafruit_ILI9341 screen{&SPI, Config::dc, Config::cs, Config::reset};
  GFXcanvas16 canvas{Width, Height};
  Adafruit_GFX* target = &screen;
  uint8_t activePage = pageCount;
  String previous[14];
  int previousIcons[3] = {-2, -2, -2};
};

#pragma once
#include "Config.h"
#include "Game.h"
#include "Weather.h"
#include <Adafruit_ILI9341.h>
#include <SPI.h>

class Interface {
 public:
  static constexpr uint16_t accent = 0x5EFF;
  void begin();
  void beginPage(const char* title, uint16_t base, uint16_t highlight);
  void tab(bool right, bool lower, const char* label);
  void animateTabs(bool entering);
  void launcher(uint8_t selected);
  void field(uint8_t slot, int x, int y, int w, int h, String value,
             uint8_t size = 0, uint16_t color = ILI9341_WHITE, bool centered = false);
  void home(const WeatherData& data);
  void game(const Game& game);
  void stats(const Game& game);
  void weather(const WeatherData& data, bool forecast);
  void network();
  void device();

 private:
  Adafruit_ILI9341 screen{&SPI, Config::dc, Config::cs, Config::reset};
  String cache[10];
  int lastLights = -1;
  int lastSelection = -1;
  int lastWeatherCode = -100;
  int forecastIcons[3] = {-100, -100, -100};
  const char* tabs[4] = {};
  uint16_t background = 0x0842;
  uint16_t highlight = accent;
};

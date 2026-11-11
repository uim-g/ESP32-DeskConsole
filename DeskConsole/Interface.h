#pragma once
#include "Config.h"
#include "Game.h"
#include "Weather.h"
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <memory>

// 负责 UI 绘制。游戏状态、网络请求和输入逻辑由各模块管理。
class Interface {
 public:
  static constexpr uint16_t accent = 0x5EFF;
  void begin();
  void setDisplayOn(bool on);
  void beginPage(const char* title, uint16_t base, uint16_t highlight);
  void tab(bool right, bool lower, const char* label);
  void animateTabs(bool entering, uint8_t mask = 0x0F);
  void gameMode(bool debug);
  void launcher(uint8_t selected);
  void field(uint8_t slot, int x, int y, int w, int h, String value,
             uint8_t size = 0, uint16_t color = ILI9341_WHITE, bool centered = false,
             bool onCard = false);
  void home(const WeatherData& data);
  void game(const Game& game);
  void startFireworks();
  void updateFireworks(const Game& game);
  void stats(const Game& game, bool confirmClear);
  void calibration(const Game& game, bool editOffset, bool saveFailed);
  void weather(const WeatherData& data, bool forecast);
  void network();
  void device();

 private:
  void card(int x, int y, int w, int h);
  void stopFireworks();
  void metric(uint8_t slot, int x, int y, int w, int h,
              char icon, const char* label, const String& value);
  Adafruit_ILI9341 screen{&SPI, Config::dc, Config::cs, Config::reset};
  // 每个字段缓存上次的文字，定时刷新只重绘发生变化的区域。
  String cache[10];
  int lastLights = -1;
  std::unique_ptr<GFXcanvas16> fireworks;
  uint32_t fireworkStarted = 0;
  uint32_t lastFireworkFrame = 0;
  int lastSelection = -1;
  bool panelsReady = false;
  int lastWeatherCode = -100;
  int forecastIcons[3] = {-100, -100, -100};
  const char* tabs[4] = {};
  uint16_t background = 0x0842;
  uint16_t highlight = accent;
};

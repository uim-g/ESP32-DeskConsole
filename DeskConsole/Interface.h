#pragma once
#include "Config.h"
#include "Game.h"
#include "Weather.h"
#include "Bluetooth.h"
#include "Settings.h"
#include "FrameTimeline.h"
#include "Platform.h"
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <memory>

// 负责 UI 绘制。游戏状态、网络请求和输入逻辑由各模块管理。
class Interface {
 public:
  static constexpr uint16_t accent = 0x5EFF;
  static constexpr int bluetoothRows = 3;
  void begin();
  void setDisplayOn(bool on);
  void beginPage(const char* title, uint16_t base, uint16_t highlight);
  void tab(bool right, bool lower, const char* label);
  void startTabs(bool entering, uint8_t mask = 0x0F);
  void replaceTab(uint8_t index, const char* label);
  bool tabsAnimating() const { return tabAnimation.active(); }
  void advanceAnimations(uint32_t now);
  void gameMode(bool debug);
  void launcher(uint8_t selected, const char* const labels[], int count);
  void settings(const Settings& values, uint8_t selected, bool editing, bool saveFailed);
  void bluetooth(const BluetoothSnapshot& scan, int first, bool namedOnly);
  void field(uint8_t slot, int x, int y, int w, int h, String value,
             uint8_t size = 0, uint16_t color = ILI9341_WHITE, bool centered = false,
             bool onCard = false);
  void home(const WeatherData& data, const Platform::Clock& clock);
  void game(const Game& game);
  void startFireworks();
  void updateFireworks(const Game& game);
  void stats(const Game& game, bool confirmClear);
  void calibration(const Game& game, bool editOffset, bool saveFailed);
  void weather(const WeatherData& data, bool forecast);
  struct WifiRow { String name, detail; int radio = -1; };
  void wifiList(const WifiRow* rows, int count, int selected, const String& footer);
  void wifiMessage(const String& title, const String& detail, const String& footer);
  void wifiPassword(const String& ssid, const String& value, size_t length, int group,
                    const String& previous, const String& selected, const String& next, int direction, const char* footer);
  void device(const Platform::Device& data);

 private:
  static constexpr uint16_t muted = 0xA69A, cardColor = 0x1928, cardOutline = 0x3A4E;
  static constexpr char degree = '\x01';
  static void smallIcon(Adafruit_GFX& gfx, int x, int y, char kind, uint16_t color);
  static String temperature(float value);
  void renderTabs(int offset, uint8_t mask);
  void renderLauncher(float progress);
  void renderSettings(float progress);
  void renderWifiList(float progress);
  void renderWifiGroups(float progress);
  void renderWifiWheel(float progress);
  void renderWifiSwitch(float progress);
  void paintWifiSwitch(Adafruit_GFX& gfx, int x, int y) const;
  FrameTimeline wifiListAnimation, wifiGroupAnimation, wifiWheelAnimation, wifiSwitchAnimation;
  int wifiSwitchTarget = -1, wifiSwitchFrom = 12, wifiSwitchKnob = 12;
  WifiRow wifiRows[3];
  int wifiRowCount = 0, wifiPrevious = -1, wifiSelected = -1;
  int wifiPreviousGroup = -1, wifiGroup = -1, wifiWheelDirection = 0;
  String wifiPreviousWheel[3], wifiWheel[3];
  void card(int x, int y, int w, int h);
  FrameTimeline tabAnimation, launcherAnimation, settingsAnimation;
  bool tabsEntering = false;
  uint8_t tabMask = 0, replacementIndex = 0;
  const char* replacementLabel = nullptr;
  uint8_t launcherTarget = 0;
  const char* const* launcherLabels = nullptr;
  int launcherCount = 0;
  Settings settingsValues;
  uint8_t settingsTarget = 0;
  bool settingsEditing = false, settingsFailed = false;
  void memoryCard(uint8_t index, int y, const char* label, size_t total, size_t free);
  void stopFireworks();
  void metric(uint8_t slot, int x, int y, int w, int h,
              char icon, const char* label, const String& value);
  Adafruit_ILI9341 screen{&SPI, Config::dc, Config::cs, Config::reset};
  // 每个字段缓存上次的文字，定时刷新只重绘发生变化的区域。
  String cache[11];
  int lastLights = -1;
  std::unique_ptr<GFXcanvas16> fireworks;
  uint32_t fireworkStarted = 0;
  uint32_t lastFireworkFrame = 0;
  int lastSelection = -1;
  int launcherTop = 0;
  int launcherThumbY = -1;
  int settingSwitches[2] = {-1, -1};
  int lastSettingRow = -1;
  int lastBrightness = -1;
  int lastSleepMinutes = -1;
  bool panelsReady = false;
  char uptimeDigits[8] = {};
  int deviceBars[3] = {-1, -1, -1};
  int lastWeatherCode = -100;
  int forecastIcons[3] = {-100, -100, -100};
  const char* tabs[4] = {};
  uint16_t background = 0x0842;
  uint16_t highlight = accent;
};

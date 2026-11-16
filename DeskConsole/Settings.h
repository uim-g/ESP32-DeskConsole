// 设备设置与闪存读写。完整的一组设置通过单个键保存，读取时校验范围以防旧记录或损坏记录被误用。

#pragma once
#include <Preferences.h>
#include "Config.h"

// 用单条 NVS 记录保存设备设置。开机读不到有效记录时使用默认值。
struct Settings {
  enum class ScreenRgb : uint8_t { Off, Intermittent, Continuous };
  static constexpr int rowCount = 5;
  uint8_t autoSleep = 1;
  uint8_t sleepIndex = 1;
  uint8_t rgbEnabled = 1;
  uint8_t brightness = Config::rgbBrightness;

  ScreenRgb screenRgb = ScreenRgb::Off;

  uint8_t sleepMinutes() const {
    constexpr uint8_t minutes[] = {1, 3, 5, 10};
    return minutes[sleepIndex];
  }

  bool valid() const {
    return autoSleep <= 1 && sleepIndex < 4 && rgbEnabled <= 1 &&
           brightness >= 1 && brightness <= 8 && screenRgb <= ScreenRgb::Continuous;
  }
};

class SettingsStore {
 public:
  void begin() {
    ready = storage.begin("desk-settings", false);
    Settings loaded;
    if (!ready) return;
    // 旧固件的四字节记录保留原设置，新灯效使用默认的关闭。
    constexpr size_t legacySize = 4;
    const size_t size = storage.getBytesLength("options");
    if ((size == sizeof(loaded) || size == legacySize) &&
        storage.getBytes("options", &loaded, size) == size &&
        loaded.valid()) current = loaded;
  }

  bool save(const Settings& next) {
    if (!ready || !next.valid() ||
        storage.putBytes("options", &next, sizeof(next)) != sizeof(next)) return false;
    current = next;
    return true;
  }

  Settings current;

 private:
  Preferences storage;
  bool ready = false;
};

// 设备设置与闪存读写。完整的一组设置通过单个键保存，读取时校验范围以防旧记录或损坏记录被误用。

#pragma once
#include <Preferences.h>
#include "Config.h"

// 用单条 NVS 记录保存四项设置。开机读不到有效记录时使用默认值。
struct Settings {
  uint8_t autoSleep = 1;
  uint8_t sleepIndex = 1;
  uint8_t rgbEnabled = 1;
  uint8_t brightness = Config::rgbBrightness;

  uint8_t sleepMinutes() const {
    constexpr uint8_t minutes[] = {1, 3, 5, 10};
    return minutes[sleepIndex];
  }

  bool valid() const {
    return autoSleep <= 1 && sleepIndex < 4 && rgbEnabled <= 1 &&
           brightness >= 1 && brightness <= 8;
  }
};

class SettingsStore {
 public:
  void begin() {
    ready = storage.begin("desk-settings", false);
    Settings loaded;
    if (ready && storage.getBytesLength("options") == sizeof(loaded) &&
        storage.getBytes("options", &loaded, sizeof(loaded)) == sizeof(loaded) &&
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

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
  uint8_t brightness = 0;  // 0–10 档，每档相差 10%。

  ScreenRgb screenRgb = ScreenRgb::Off;

  uint8_t sleepMinutes() const {
    constexpr uint8_t minutes[] = {1, 3, 5, 10};
    return minutes[sleepIndex];
  }

  uint8_t brightnessPercent() const { return brightness * 10; }

  // 将 0–10 档线性映射到 1–255，四舍五入到最近的通道值。
  uint8_t brightnessLevel() const { return 1 + (254 * brightness + 5) / 10; }

  bool valid() const {
    return autoSleep <= 1 && sleepIndex < 4 && rgbEnabled <= 1 &&
           brightness <= 10 && screenRgb <= ScreenRgb::Continuous;
  }
};

class SettingsStore {
 public:
  void begin() {
    ready = storage.begin("desk-settings", false);
    Settings loaded;
    if (!ready) return;
    if (storage.getBytesLength("options-v2") == sizeof(loaded) &&
        storage.getBytes("options-v2", &loaded, sizeof(loaded)) == sizeof(loaded) &&
        loaded.valid()) {
      current = loaded;
      return;
    }

    // 旧记录里的 1–8 都最接近新刻度的 0% 档。保留其他设置和旧记录。
    loaded = Settings{};
    constexpr size_t legacySize = 4;
    const size_t size = storage.getBytesLength("options");
    if ((size == sizeof(loaded) || size == legacySize) &&
        storage.getBytes("options", &loaded, size) == size &&
        loaded.brightness >= 1 && loaded.brightness <= 8) {
      loaded.brightness = 0;
      if (loaded.valid()) current = loaded;
    }
  }

  bool save(const Settings& next) {
    if (!ready || !next.valid() ||
        storage.putBytes("options-v2", &next, sizeof(next)) != sizeof(next)) return false;
    current = next;
    return true;
  }

  Settings current;

 private:
  Preferences storage;
  bool ready = false;
};

// 设备设置以单条 NVS 记录保存，旧版记录保留供旧固件读取。
#pragma once
#include <Preferences.h>
#include "Config.h"

struct Settings {
  enum class ScreenRgb : uint8_t { Off, Intermittent, Continuous };
  static constexpr int rowCount = 3;
  static constexpr int sleepChoices = 6;
  static constexpr int rgbChoices = 11;

  uint8_t sleepIndex = 3;
  uint8_t brightness = 1;
  ScreenRgb screenRgb = ScreenRgb::Off;

  bool autoSleep() const { return sleepIndex != 0; }
  bool rgbEnabled() const { return brightness != 0; }

  uint8_t sleepMinutes() const {
    constexpr uint8_t minutes[] = {0, 1, 2, 3, 5, 10};
    return minutes[sleepIndex];
  }

  uint8_t brightnessPercent() const { return brightness * 10; }
  uint8_t brightnessLevel() const { return 1 + (254 * brightness + 5) / 10; }

  bool valid() const {
    return sleepIndex < sleepChoices && brightness < rgbChoices && screenRgb <= ScreenRgb::Continuous;
  }
};

class SettingsStore {
 public:
  void begin() {
    ready = storage.begin("desk-settings", false);
    if (!ready) return;
    Settings loaded;
    if (storage.getBytesLength("options-v3") == sizeof(loaded) &&
        storage.getBytes("options-v3", &loaded, sizeof(loaded)) == sizeof(loaded) &&
        loaded.valid()) {
      current = loaded;
      return;
    }

    if (loadLegacy("options-v2", false)) return;
    loadLegacy("options", true);
  }

  bool save(const Settings& next) {
    if (!ready || !next.valid() ||
        storage.putBytes("options-v3", &next, sizeof(next)) != sizeof(next)) return false;
    current = next;
    return true;
  }

  Settings current;

 private:
  bool loadLegacy(const char* key, bool original) {
    // 旧布局依次保存开关、时长索引、RGB 开关、亮度和可选的熄屏灯效。
    uint8_t record[5]{};
    const size_t size = storage.getBytesLength(key);
    if ((size != sizeof(record) && !(original && size == 4)) ||
        storage.getBytes(key, record, size) != size ||
        record[0] > 1 || record[1] >= 4 || record[2] > 1 ||
        (original ? record[3] < 1 || record[3] > 8 : record[3] > 10) ||
        record[4] > static_cast<uint8_t>(Settings::ScreenRgb::Continuous)) return false;
    constexpr uint8_t migratedSleep[] = {1, 3, 4, 5};
    current.sleepIndex = record[0] ? migratedSleep[record[1]] : 0;
    current.brightness = record[2] ? (original || record[3] == 0 ? 1 : record[3]) : 0;
    current.screenRgb = static_cast<Settings::ScreenRgb>(record[4]);
    return true;
  }

  Preferences storage;
  bool ready = false;
};

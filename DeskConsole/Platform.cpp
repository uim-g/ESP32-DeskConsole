// 平台实现集中调用 Arduino、Wi-Fi 和 ESP32 系统 API。网页构建时将输入就绪和画面提交转交浏览器适配器。

#include "Platform.h"
#include "Config.h"
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>
#include <esp_system.h>

#ifdef __EMSCRIPTEN__
void presentBrowserDisplay();
void setBrowserInputReady(bool ready);
#endif

namespace Platform {
namespace {
constexpr time_t earliestClock = 1577836800;
}
bool clockReady() { return time(nullptr) >= earliestClock; }

Clock clock() {
  Clock result;
  const auto now = time(nullptr);
  tm local;
  if (now >= earliestClock && localtime_r(&now, &local)) {
    strftime(result.time, sizeof(result.time), "%H:%M:%S", &local);
    strftime(result.date, sizeof(result.date), "%a, %d %b %Y", &local);
  }
  return result;
}

Network network() {
  const bool connected = isConnected(networkStatus());
  return {connected, connected ? WiFi.SSID() : String("--"),
          connected ? WiFi.localIP().toString() : String("--"),
          connected ? signal() : 0, clockReady()};
}

Device device() {
  static const auto* app = esp_ota_get_running_partition();
  static const size_t capacity = app ? app->size : 0;
  static const size_t image = capacity ? ESP.getSketchSize() : 0;
  return {temperatureRead(), {ESP.getHeapSize(), ESP.getFreeHeap()},
          {ESP.getPsramSize(), ESP.getFreePsram()},
          {image ? capacity : 0, capacity - min(capacity, image)}, ESP.getFlashChipSize(), millis() / 1000};
}

bool stopNetwork() { return WiFi.mode(WIFI_OFF); }
int networkStatus() { return WiFi.status(); }
bool isConnected(int status) { return status == WL_CONNECTED; }
int32_t signal() { return WiFi.RSSI(); }
void configureClock() { configTzTime("MYT-8", "pool.ntp.org", "time.google.com"); }
int resetReason() { return static_cast<int>(esp_reset_reason()); }
int lightSleep() { return esp_light_sleep_start(); }
void setInputReady(bool ready) {
#ifdef __EMSCRIPTEN__
  setBrowserInputReady(ready);
#else
  (void)ready;
#endif
}
void presentDisplay() {
#ifdef __EMSCRIPTEN__
  presentBrowserDisplay();
#endif
}
}

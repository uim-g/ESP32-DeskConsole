#pragma once
#include <Arduino.h>

namespace Config {
constexpr char ssid[] = "YOUR_WIFI_NAME";
constexpr char password[] = "YOUR_WIFI_PASSWORD";
constexpr char latitude[] = "2.945";
constexpr char longitude[] = "101.874";
constexpr char location[] = "UNM / Semenyih";
constexpr uint8_t rotation = 1;  // 3 为反向横屏。
constexpr int sck = 12, mosi = 11, cs = 10, dc = 9, reset = 8;
constexpr int touchPin = 4;
constexpr uint8_t touchOnPercent = 18;  // 读数高于基线 18% 视为触摸。
constexpr uint8_t touchOffPercent = 8;  // 回落后才允许下一次翻页。
}

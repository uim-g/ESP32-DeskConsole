// 集中定义固件版本、屏幕连线和默认配置。Wi-Fi 常量仅用于还没有保存记录时的启动连接；后续连接历史由 WifiNetworks 管理。

#pragma once
#include <Arduino.h>

namespace Config {
constexpr uint8_t rgbPin = 48;
constexpr uint8_t rgbBrightness = 8;  // 每个颜色通道的最大亮度。
constexpr char firmwareVersion[] = "v2.2.0";  // 正式发布时改成对应的 Git 标签。
// 本地填写 2.4 GHz Wi-Fi 信息。仓库中保留占位符。
constexpr char ssid[] = "YOUR_WIFI_NAME";
constexpr char password[] = "YOUR_WIFI_PASSWORD";
// Open-Meteo 查询使用十进制度。默认值对应诺丁汉大学士毛月校区。
constexpr char latitude[] = "2.945";
constexpr char longitude[] = "101.874";
constexpr char location[] = "UNM / Semenyih";
constexpr uint8_t rotation = 1;  // 3 为反向横屏。
// 屏幕 SPI 为只写模式。SCK 是时钟，MOSI 是数据，CS 选屏，DC 区分命令和数据。
constexpr int sck = 12, mosi = 11, cs = 10, dc = 9, reset = 8;
}

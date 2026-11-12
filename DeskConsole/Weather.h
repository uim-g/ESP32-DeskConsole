// Open-Meteo 天气缓存的数据结构。缺失数值用 NaN 表示，available 表示曾经取到结果，stale 表示当前显示缓存。

#pragma once
#include <Arduino.h>

// 天气快照只保存 UI 所需的数据。缺失数值为 NaN，未知天气代码为 -1。
struct WeatherData {
  // available 表示至少成功取过一次。stale 表示目前显示的可能是缓存。
  bool available = false;
  bool stale = false;
  float temp = NAN, feels = NAN, humidity = NAN, pressure = NAN;
  float wind = NAN, gust = NAN, direction = NAN, rain = NAN, clouds = NAN;
  int code = -1;
  int dailyCode[3] = {-1, -1, -1};
  float high[3] = {NAN, NAN, NAN};
  float low[3] = {NAN, NAN, NAN};
  float chance[3] = {NAN, NAN, NAN};
  // 日期和时间使用固定长度字符数组，便于任务间复制整份快照。
  char dates[3][11] = {};
  char sunrise[6] = {}, sunset[6] = {}, updated[6] = {};
};

// 后台任务负责网络请求。主循环调用 snapshot() 读取缓存。
class Weather {
 public:
  void begin();
  WeatherData snapshot();

 private:
  static void worker(void* context);
  static bool fetch(WeatherData& result);
  portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
  WeatherData cached;
};

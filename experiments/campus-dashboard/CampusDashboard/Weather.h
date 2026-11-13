#pragma once
#include <Arduino.h>

struct WeatherData {
  bool available = false;
  bool stale = false;
  float temp = NAN, feels = NAN, humidity = NAN, pressure = NAN;
  float wind = NAN, gust = NAN, direction = NAN, rain = NAN, clouds = NAN;
  int code = -1;
  int dailyCode[3] = {-1, -1, -1};
  float high[3] = {NAN, NAN, NAN};
  float low[3] = {NAN, NAN, NAN};
  float chance[3] = {NAN, NAN, NAN};
  char dates[3][11] = {};
  char sunrise[6] = {}, sunset[6] = {}, updated[6] = {};
};

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

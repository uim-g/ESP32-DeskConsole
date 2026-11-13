#include "Weather.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {
void copyTime(char (&target)[6], const char* timestamp) {
  snprintf(target, sizeof(target), "%.5s",
           strlen(timestamp) >= 16 ? timestamp + 11 : "--:--");
}
}

void Weather::begin() {
  if (xTaskCreate(worker, "weather", 8192, this, 1, nullptr) != pdPASS) {
    Serial.println("Could not start weather task");
  }
}

WeatherData Weather::snapshot() {
  // 只在复制缓存时持锁。网络请求和绘图都在锁外执行。
  portENTER_CRITICAL(&lock);
  WeatherData result = cached;
  portEXIT_CRITICAL(&lock);
  return result;
}

void Weather::worker(void* context) {
  auto& self = *static_cast<Weather*>(context);
  for (;;) {
    bool ok = false;
    if (WiFi.status() == WL_CONNECTED) {
      WeatherData result;
      ok = fetch(result);
      portENTER_CRITICAL(&self.lock);
      if (ok) self.cached = result;
      self.cached.stale = !ok;
      portEXIT_CRITICAL(&self.lock);
    }
    vTaskDelay(pdMS_TO_TICKS(ok ? 900000 : 30000));
  }
}

bool Weather::fetch(WeatherData& result) {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += Config::latitude;
  url += "&longitude=";
  url += Config::longitude;
  url += "&timezone=Asia%2FKuala_Lumpur&forecast_days=3"
         "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
         "surface_pressure,wind_speed_10m,wind_gusts_10m,wind_direction_10m,"
         "precipitation,cloud_cover,weather_code"
         "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
         "precipitation_probability_max,sunrise,sunset";

  WiFiClientSecure client;
  client.setInsecure();  // 原型沿用此设置。部署时改用可信 CA 校验服务器。
  client.setHandshakeTimeout(5);
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(4000);
  http.useHTTP10(true);
  if (!http.begin(client, url)) return false;

  const int status = http.GET();
  JsonDocument doc;
  const bool ok = status == HTTP_CODE_OK &&
                  !deserializeJson(doc, http.getStream()) &&
                  doc["current"].is<JsonObject>() && doc["daily"].is<JsonObject>();
  http.end();
  if (!ok) {
    Serial.printf("Weather request or parsing failed, HTTP %d\n", status);
    return false;
  }

  // 缺失数值使用 NAN，界面会显示 --，避免误显示为零。
  JsonObjectConst c = doc["current"].as<JsonObjectConst>();
  JsonObjectConst d = doc["daily"].as<JsonObjectConst>();
  result.temp = c["temperature_2m"] | NAN;
  result.feels = c["apparent_temperature"] | NAN;
  result.humidity = c["relative_humidity_2m"] | NAN;
  result.pressure = c["surface_pressure"] | NAN;
  result.wind = c["wind_speed_10m"] | NAN;
  result.gust = c["wind_gusts_10m"] | NAN;
  result.direction = c["wind_direction_10m"] | NAN;
  result.rain = c["precipitation"] | NAN;
  result.clouds = c["cloud_cover"] | NAN;
  result.code = c["weather_code"] | -1;
  copyTime(result.updated, c["time"] | "");
  copyTime(result.sunrise, d["sunrise"][0] | "");
  copyTime(result.sunset, d["sunset"][0] | "");
  for (size_t i = 0; i < 3; ++i) {
    result.dailyCode[i] = d["weather_code"][i] | -1;
    result.high[i] = d["temperature_2m_max"][i] | NAN;
    result.low[i] = d["temperature_2m_min"][i] | NAN;
    result.chance[i] = d["precipitation_probability_max"][i] | NAN;
    snprintf(result.dates[i], sizeof(result.dates[i]), "%.10s", d["time"][i] | "");
  }
  result.available = true;
  return true;
}

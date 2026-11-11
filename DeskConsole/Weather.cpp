#include "Weather.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {
// API 的时间是 ISO 格式，例如 2026-11-11T06:56。这里只取 HH:MM。
void copyTime(char (&target)[6], const char* timestamp) {
  snprintf(target, sizeof(target), "%.5s",
           strlen(timestamp) >= 16 ? timestamp + 11 : "--:--");
}
}

// 启动 FreeRTOS 任务，让 HTTPS 请求和 JSON 解析与按键扫描分开运行。
void Weather::begin() {
  if (xTaskCreate(worker, "weather", 8192, this, 1, nullptr) != pdPASS) {
    Serial.println("Could not start weather task");
  }
}

// 只在复制数据时加锁。这样 UI 不会读到更新了一半的天气数据。
WeatherData Weather::snapshot() {
  // 只在复制缓存时持锁。网络请求和绘图都在锁外执行。
  portENTER_CRITICAL(&lock);
  WeatherData result = cached;
  portEXIT_CRITICAL(&lock);
  return result;
}

// 联网后抓取。成功每 15 分钟再抓，失败约 30 秒后重试，并保留上次数据。
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

// 一次请求取回实时数据和三天预报，使用校区坐标与 Kuala Lumpur 时区。
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
  // 为网络连接和读取设置超时，避免单次请求无限等待。
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(4000);
  http.useHTTP10(true);
  if (!http.begin(client, url)) return false;

  const int status = http.GET();
  // 只接受 HTTP 成功且包含 current 和 daily 的响应。
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
  // 日预报字段是平行数组，同一个下标对应同一天。
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

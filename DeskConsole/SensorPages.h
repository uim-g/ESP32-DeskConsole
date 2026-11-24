#pragma once
#include "Page.h"

#ifndef __EMSCRIPTEN__
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <ESP_I2S.h>
#endif

class EnvironmentPage : public RefreshPage {
 public:
  using RefreshPage::RefreshPage;
  PageStyle style() const override { return {"Temp & Humidity", 0x20C5, 0xFD68, {"BACK", "", "", ""}}; }
  void draw() override;

 private:
#ifndef __EMSCRIPTEN__
  enum class Sensor { None, Bmp280, Bme280 };
  Sensor sensor = Sensor::None;
  Adafruit_BMP280 bmp;
  Adafruit_BME280 bme;
  void detect();
#endif
};

class MicrophonePage : public Page {
 public:
  using Page::Page;
  PageStyle style() const override { return {"Microphone", 0x18CA, 0xCF5F, {"BACK", "", "", ""}}; }
  void enter() override;
  void leave() override;
  void tick(uint32_t now) override;
  void draw() override;

 private:
#ifndef __EMSCRIPTEN__
  I2SClass i2s;
  bool started = false;
#endif
  uint32_t sampledAt = 0;
  int level = -1;
};

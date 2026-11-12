// 桌面、App 列表、天气、蓝牙和设备页的页面控制器。只在各自页面活跃时更新画面。

#pragma once
#include "Page.h"

class HomePage : public RefreshPage {
 public:
  HomePage(PageContext& context, Weather& service) : RefreshPage(context), weather(service) {}
  PageStyle style() const override { return {"", 0x1845, 0xFE23, {"APPS"}}; }
  void draw() override { ctx.ui.home(weather.snapshot(), Platform::clock()); }
  void input(Key key, uint32_t) override { if (key == Key::Back) ctx.nav.open(View::Launcher); }
 private:
  Weather& weather;
};

class LauncherPage : public Page {
 public:
  using Page::Page;
  PageStyle style() const override { return {"Apps", 0x10A4, 0xFE28, {"BACK","OPEN","^","v"}}; }
  void draw() override;
  void input(Key key, uint32_t) override;
  bool repeats() const override { return true; }
 private:
  uint8_t selected = 0;
};

class WeatherPage : public RefreshPage {
 public:
  WeatherPage(PageContext& context, Weather& service) : RefreshPage(context), weather(service) {}
  PageStyle style() const override {
    return {"Weather /", 0x08A7, 0x65FF, {"BACK",nullptr,"^","v"}};
  }
  void enter() override;
  void draw() override { ctx.ui.weather(weather.snapshot(), forecast); }
  void input(Key key, uint32_t us) override;
  bool repeats() const override { return true; }
 private:
  Weather& weather;
  bool forecast = false;
};

class DevicePage : public RefreshPage {
 public:
  using RefreshPage::RefreshPage;
  PageStyle style() const override { return {"Device",0x18E3,0xB6B1,{"BACK"}}; }
  void draw() override { ctx.ui.device(Platform::device()); }
};

class BluetoothPage : public Page {
 public:
  BluetoothPage(PageContext& context, BluetoothScanner& service) : Page(context), scanner(service) {}
  PageStyle style() const override { return {"Bluetooth",0x0868,0x75BF,{"BACK","SCAN","^","v"}}; }
  void draw() override;
  void input(Key key, uint32_t us) override;
  void tick(uint32_t now) override;
  void leave() override { scanPending = false; }
  void wake() override { scanPending = false; }
  bool repeats() const override { return true; }
 private:
  BluetoothScanner& scanner;
  BluetoothSnapshot::State state = BluetoothSnapshot::State::Idle;
  int first = 0;
  bool namedOnly = false, scanPending = false;
  uint32_t pressedAt = 0;
};

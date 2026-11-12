#pragma once
#include "DashboardPages.h"
#include "F1Pages.h"
#include "SettingsPage.h"

class DeskConsoleApp : public Navigation {
 public:
  void begin();
  void tick();
  void open(View next) override;

 private:
  Page& page(View view);
  void show(View next);
  void reportConnectivity();
  void sleepIfIdle(uint32_t now);
  void dispatch(Key key, uint32_t pressedUs, uint32_t now);
  uint32_t idleSleepMs() const { return settings.current.sleepMinutes() * 60000UL; }

  Buttons buttons;
  Interface ui;
  StatusLed led;
  Game game;
  Weather weather;
  BluetoothScanner bluetooth;
  SettingsStore settings;
  PageContext context{*this, ui, buttons, led};
  HomePage home{context, weather};
  LauncherPage launcher{context};
  F1Page f1{context, game};
  WeatherPage forecast{context, weather};
  NetworkPage network{context};
  BluetoothPage ble{context, bluetooth};
  DevicePage device{context};
  SettingsPage preferences{context, settings};
  StatsPage stats{context, game};
  CalibrationPage calibration{context, game};

  View active = View::Home, pending = View::Home;
  enum class Transition { None, Exiting, Entering };
  Transition transition = Transition::None;
  Key repeatKey = Key::None;
  uint32_t repeatAt = 0, lastInput = 0, lastWakeLow = 0;
  int lastWifi = -1;
  bool timeReported = false, wakeConfigured = false, discardWakePress = false;
};

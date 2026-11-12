#include "DeskConsoleApp.h"

Page& DeskConsoleApp::page(View view) {
  switch (view) {
    case View::Home: return home;
    case View::Launcher: return launcher;
    case View::F1: return f1;
    case View::Weather: return forecast;
    case View::Network: return network;
    case View::Bluetooth: return ble;
    case View::Device: return device;
    case View::Settings: return preferences;
    case View::Stats: return stats;
    case View::Calibration: return calibration;
  }
  return home;
}

void DeskConsoleApp::open(View next) {
  pending = next;
  page(active).leave();
  if (next != active) repeatKey = Key::None;
  transition = Transition::Exiting;
  ui.startTabs(false);
}

void DeskConsoleApp::show(View next) {
  active = next;
  auto& current = page(active);
  current.enter();
  const auto style = current.style();
  Serial.printf("[UI] Page: %s\n", *style.title ? style.title : "Home");
  ui.beginPage(style.title, style.background, style.accent);
  current.draw();
  for (uint8_t i = 0; i < 4; ++i) ui.tab(i >= 2, i % 2, style.tabs[i]);
  ui.startTabs(true);
  transition = Transition::Entering;
}

void DeskConsoleApp::begin() {
  Serial.begin(115200);
  Serial.printf("[BOOT] DeskConsole starting; reset reason=%d\n", Platform::resetReason());
  buttons.begin();
  settings.begin();
  led.configure(settings.current.rgbEnabled, settings.current.brightness);
  wakeConfigured = buttons.enableWakeup();
  if (wakeConfigured && settings.current.autoSleep) {
    Serial.printf("[POWER] Light sleep after %lu s idle\n", static_cast<unsigned long>(idleSleepMs() / 1000));
  } else if (wakeConfigured) Serial.println("[POWER] Auto sleep disabled in Settings");
  else Serial.println("[POWER] GPIO wake setup failed; sleep disabled");

  game.begin();
  ui.begin();
  show(View::Home);
  Platform::presentDisplay();
  led.begin();
  const uint32_t networkStarted = millis();
  Serial.println("[WIFI] Connecting in station mode");
  Platform::connectNetwork();
  Platform::configureClock();
  Serial.println("[TIME] NTP configured for MYT (UTC+8)");
  weather.begin();
  Serial.printf("[BOOT] Network setup returned in %lu ms; loop ready at %lu ms\n",
                static_cast<unsigned long>(millis() - networkStarted), static_cast<unsigned long>(millis()));
  lastInput = millis();
}

void DeskConsoleApp::reportConnectivity() {
  const int status = Platform::networkStatus();
  if (status != lastWifi) {
    lastWifi = status;
    if (Platform::isConnected(status)) {
      Serial.printf("[WIFI] Connected; signal=%ld dBm\n", static_cast<long>(Platform::signal()));
      led.play(StatusLed::Cue::Connected);
    } else Serial.printf("[WIFI] Connection status=%d\n", status);
  }
  if (!timeReported && Platform::clockReady()) {
    timeReported = true;
    Serial.println("[TIME] Clock synchronized");
  }
}

void DeskConsoleApp::sleepIfIdle(uint32_t now) {
  if (!wakeConfigured || !settings.current.autoSleep || now - lastInput < idleSleepMs() ||
      buttons.anyPressed() || !page(active).canSleep() || bluetooth.busy()) return;

  Serial.println("[POWER] Idle timeout; entering light sleep");
  if (!Platform::stopNetwork()) {
    Serial.println("[POWER] Could not stop Wi-Fi; sleep skipped");
    lastInput = millis();
    return;
  }
  ui.setDisplayOn(false);
  led.stop();
  const int result = buttons.anyPressed() ? ESP_OK : Platform::lightSleep();
  ui.setDisplayOn(true);
  Platform::connectNetwork();
  lastInput = lastWakeLow = millis();
  discardWakePress = true;
  repeatKey = Key::None;
  page(active).wake();
  Serial.printf("[POWER] Active again; sleep status=%d, Wi-Fi reconnecting\n", result);
  page(active).draw();
}

void DeskConsoleApp::dispatch(Key key, uint32_t pressedUs, uint32_t now) {
  if (key != Key::None) {
    lastInput = now;
    constexpr const char* names[] = {"LEFT_TOP", "LEFT_BOTTOM", "RIGHT_TOP", "RIGHT_BOTTOM"};
    Serial.printf("[%lu ms] [KEY] %s\n", static_cast<unsigned long>(now), names[static_cast<uint8_t>(key)]);
    page(active).input(key, pressedUs);
    if ((transition == Transition::None || pending == active) && page(active).repeats() && vertical(key)) {
      repeatKey = key;
      repeatAt = now + 420;
    }
  }
  if (transition == Transition::None && repeatKey != Key::None) {
    if (!page(active).repeats() || !buttons.held(repeatKey)) repeatKey = Key::None;
    else if (due(now, repeatAt)) {
      lastInput = now;
      page(active).input(repeatKey, micros());
      repeatAt = millis() + 140;
    }
  }
}

void DeskConsoleApp::tick() {
  const uint32_t now = millis();
  if (discardWakePress) {
    if (buttons.anyPressed()) lastWakeLow = now;
    if (now - lastWakeLow >= 35) {
      buttons.begin();
      lastInput = now;
      discardWakePress = false;
    }
    return;
  }

  uint32_t pressedUs = 0;
  const auto key = buttons.poll(pressedUs);
  // 倒计时和抢跑先于普通动画及联网日志处理。
  if (transition == Transition::None) {
    dispatch(key, pressedUs, now);
    if (transition == Transition::None) page(active).tick(millis());
  } else if (key != Key::None) lastInput = now;
  ui.advanceAnimations(millis());
  if (!ui.tabsAnimating()) {
    if (transition == Transition::Exiting) show(pending);
    else if (transition == Transition::Entering) {
      transition = Transition::None;
      if (repeatKey != Key::None && due(now, repeatAt)) repeatAt = now + 140;
    }
  }
  if (active != View::F1) reportConnectivity();
  led.update();
  if (transition == Transition::None && !ui.tabsAnimating()) sleepIfIdle(millis());
}

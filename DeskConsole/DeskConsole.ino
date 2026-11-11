#include "Config.h"
#include "Buttons.h"
#include "Bluetooth.h"
#include "Game.h"
#include "Interface.h"
#include "Weather.h"
#include <WiFi.h>
#include <time.h>

// 页面路由独立于绘图。相同的实体键根据当前页面执行不同操作。
enum class View { Home, Launcher, F1, Weather, Network, Bluetooth, Device, Stats, Calibration };
constexpr View apps[] = {View::F1, View::Weather, View::Network, View::Bluetooth, View::Device};
constexpr const char* appNames[] = {"F1 Reaction", "Weather", "Wi-Fi", "Bluetooth", "Device"};
constexpr int appCount = sizeof(apps) / sizeof(apps[0]);
View view = View::Home;
uint8_t selection = 0;
bool forecast = false;
bool confirmClear = false, editOffset = false, calibrationDirty = false, saveFailed = false;
Buttons::Key repeatKey = Buttons::Key::None;
uint32_t repeatAt = 0, fieldPressedAt = 0;
bool fieldPending = false, fieldReset = false;
uint32_t lastRefresh = 0;
constexpr uint32_t idleSleepMs = 3 * 60 * 1000UL;
constexpr uint32_t wakeReleaseMs = 35;
uint32_t lastInput = 0, lastWakeLow = 0;
bool wakeConfigured = false, discardWakePress = false;
int firstBluetooth = 0;
bool namedBluetoothOnly = false, scanPending = false;
uint32_t scanPressedAt = 0;
Buttons::Key navRepeatKey = Buttons::Key::None;
uint32_t navRepeatAt = 0;
BluetoothSnapshot::State bluetoothState = BluetoothSnapshot::State::Idle;

Buttons buttons;
Game game;
Interface ui;
Weather weather;
BluetoothScanner bluetooth;

void adjustCalibration(Buttons::Key key) {
  game.adjustCalibration(editOffset, key == Buttons::Key::Up ? 1 : -1);
  calibrationDirty = true;
  saveFailed = false;
  Serial.printf("[CAL] %s: factor=%.2f offset=%+d ms\n",
                editOffset ? "Offset" : "Multiplier", game.factor(), game.offsetMs());
}

// 所有页面共用按键、屏幕和天气缓存。只有当前页面会绘制。
void draw() {
  switch (view) {
    case View::Home: ui.home(weather.snapshot()); break;
    case View::Launcher: ui.launcher(selection, appNames); break;
    case View::F1: ui.game(game); break;
    case View::Weather: ui.weather(weather.snapshot(), forecast); break;
    case View::Network: ui.network(); break;
    case View::Bluetooth: {
      const auto scan = bluetooth.snapshot();
      bluetoothState = scan.state;
      ui.bluetooth(scan, firstBluetooth, namedBluetoothOnly);
      break;
    }
    case View::Device: ui.device(); break;
    case View::Stats: ui.stats(game, confirmClear); break;
    case View::Calibration: ui.calibration(game, editOffset, saveFailed); break;
  }
}

void open(View next) {
  if (next != view) {
    navRepeatKey = Buttons::Key::None;
    scanPending = false;
  }
  // 先收起旧标签，再替换正文，最后弹入新页面的标签。
  ui.animateTabs(false);
  view = next;
  lastRefresh = millis();
  const char* title = "";
  uint16_t base = 0x0842, color = Interface::accent;
  switch (view) {
    case View::Home: base = 0x1845; color = 0xFE23; break;
    case View::Launcher: title = "Apps"; base = 0x10A4; color = 0xFE28; break;
    case View::F1: title = "F1 REACTION"; base = ILI9341_BLACK; color = 0xFA69; break;
    case View::Weather:
      title = forecast ? "Weather / Forecast" : "Weather / Now";
      base = 0x08A7; color = 0x65FF; break;
    case View::Network: title = "Wi-Fi"; break;
    case View::Bluetooth: title = "Bluetooth"; base = 0x0868; color = 0x75BF; break;
    case View::Device: title = "Device"; base = 0x18E3; color = 0xB6B1; break;
    case View::Stats: title = "F1 Statistics"; base = ILI9341_BLACK; color = 0xFB0C; break;
    case View::Calibration: title = "F1 Calibration"; base = ILI9341_BLACK; color = 0xFB0C; break;
  }
  Serial.printf("[UI] Page: %s\n", *title ? title : "Home");
  ui.beginPage(title, base, color);
  draw();
  ui.tab(false, false, view == View::Home ? "APPS" : view == View::Calibration ? "SAVE" : "BACK");
  if (view == View::Launcher || view == View::Weather || view == View::Bluetooth) {
    ui.tab(true, false, "^");
    ui.tab(true, true, "v");
  }
  if (view == View::Launcher) ui.tab(false, true, "OPEN");
  if (view == View::Network) ui.tab(false, true, "RETRY");
  if (view == View::Bluetooth) ui.tab(false, true, "SCAN");
  if (view == View::F1) {
    ui.tab(false, true, "MODE");
    ui.tab(true, false, game.debug ? "SET" : "STATS");
    ui.tab(true, true, "PLAY");
  }
  if (view == View::Stats) ui.tab(false, true, "ZERO");
  if (view == View::Calibration) {
    ui.tab(false, true, "FIELD");
    ui.tab(true, false, "+");
    ui.tab(true, true, "-");
  }
  ui.animateTabs(true);
}

void handle(Buttons::Key key, uint32_t pressedUs) {
  using Key = Buttons::Key;
  if (key == Key::Back) {
    if (view == View::Calibration && calibrationDirty) {
      if (!game.saveCalibration()) {
        Serial.println("[CAL] Flash save failed");
        saveFailed = true;
        draw();
        return;
      }
      Serial.println("[CAL] Saved to flash");
    }
    if (view == View::F1) game.reset();
    confirmClear = false;
    open(view == View::Stats || view == View::Calibration ? View::F1 : view == View::Home ? View::Launcher :
         view == View::Launcher ? View::Home : View::Launcher);
    return;
  }
  if (view == View::Launcher) {
    if (key == Key::Action) {
      Serial.printf("[APPS] Open: %s\n", appNames[selection]);
      forecast = false;
      open(apps[selection]);
    } else if (key == Key::Up || key == Key::Down) {
      selection = (selection + (key == Key::Up ? appCount - 1 : 1)) % appCount;
      Serial.printf("[APPS] Selected: %s\n", appNames[selection]);
      draw();
    }
  } else if (view == View::F1) {
    if (key == Key::Down) {
      game.press(pressedUs);
      draw();
      if (game.newRecord) ui.startFireworks();
    } else if (key == Key::Action && game.canChangeMode()) {
      constexpr uint8_t upperRightTab = 1U << 2;
      game.debug = !game.debug;
      Serial.printf("[F1] Mode: %s\n", game.debug ? "DEBUG" : "NORMAL");
      ui.gameMode(game.debug);
      ui.animateTabs(false, upperRightTab);
      ui.tab(true, false, game.debug ? "SET" : "STATS");
      ui.animateTabs(true, upperRightTab);
    } else if (key == Key::Up && game.canChangeMode()) {
      editOffset = calibrationDirty = saveFailed = confirmClear = false;
      repeatKey = Key::None;
      fieldPending = fieldReset = false;
      open(game.debug ? View::Calibration : View::Stats);
    }
  } else if (view == View::Stats && key == Key::Action) {
    if (confirmClear) game.clearStats();
    else Serial.println("[F1] Press ZERO again to confirm");
    confirmClear = !confirmClear;
    draw();
  } else if (view == View::Calibration) {
    if (key == Key::Action) {
      repeatKey = Key::None;
      fieldPressedAt = millis();
      fieldPending = true;
      fieldReset = false;
    } else if ((key == Key::Up || key == Key::Down) && !fieldPending) {
      adjustCalibration(key);
      repeatKey = key;
      repeatAt = millis() + 420;
      draw();
    }
  } else if (view == View::Weather && (key == Key::Up || key == Key::Down)) {
    forecast = !forecast;
    open(View::Weather);
  } else if (view == View::Network && key == Key::Action) {
    Serial.printf("[WIFI] Manual reconnect: %s\n", WiFi.reconnect() ? "requested" : "failed");
    draw();
  } else if (view == View::Bluetooth) {
    if (key == Key::Action && !bluetooth.busy()) {
      scanPending = true;
      scanPressedAt = millis();
    } else if (key == Key::Up || key == Key::Down) {
      const auto scan = bluetooth.snapshot();
      if (scan.state != BluetoothSnapshot::State::Ready) return;
      constexpr int rows = Interface::bluetoothRows;
      const int last = max(0, scan.count - 1) / rows * rows;
      const int next = constrain(firstBluetooth + (key == Key::Up ? -rows : rows), 0, last);
      if (next != firstBluetooth) {
        firstBluetooth = next;
        draw();
      }
    }
  }
}

void updateBluetoothScanKey() {
  if (!scanPending) return;
  const bool held = buttons.held(Buttons::Key::Action);
  if (held && millis() - scanPressedAt < 800) return;
  scanPending = false;
  // 长按切换过滤并重扫，先过滤再按 RSSI 选取前 24 条。
  if (held) {
    namedBluetoothOnly = !namedBluetoothOnly;
    Serial.printf("[BLE] Named-only filter: %s\n", namedBluetoothOnly ? "on" : "off");
  }
  firstBluetooth = 0;
  bluetooth.start(namedBluetoothOnly);
  draw();
}

bool repeatsNavigation() {
  return view == View::Launcher || view == View::Weather || view == View::Bluetooth;
}

void updateNavigationKeys() {
  using Key = Buttons::Key;
  if (navRepeatKey == Key::None) return;
  if (!repeatsNavigation() || !buttons.held(navRepeatKey)) {
    navRepeatKey = Key::None;
    return;
  }
  if (static_cast<int32_t>(millis() - navRepeatAt) >= 0) {
    // 每次动画完成后再等待一小段时间，避免堆积翻页动作。
    lastInput = millis();
    handle(navRepeatKey, micros());
    navRepeatAt = millis() + 140;
  }
}

void updateCalibrationKeys() {
  // FIELD 短按在松开时触发，长按则只恢复一次默认值。
  using Key = Buttons::Key;
  const uint32_t now = millis();
  if (fieldPending) {
    if (!buttons.held(Key::Action)) {
      if (!fieldReset) {
        editOffset = !editOffset;
        Serial.printf("[CAL] Editing: %s\n", editOffset ? "Offset" : "Multiplier");
      }
      fieldPending = false;
      draw();
    } else if (!fieldReset && now - fieldPressedAt >= 800) {
      game.resetCalibration();
      Serial.println("[CAL] Defaults restored: factor=1.00 offset=0 ms; press SAVE to persist");
      calibrationDirty = fieldReset = true;
      saveFailed = false;
      draw();
    }
  }
  if (repeatKey != Key::None) {
    if (!buttons.held(repeatKey)) {
      repeatKey = Key::None;
    } else if (static_cast<int32_t>(now - repeatAt) >= 0) {
      adjustCalibration(repeatKey);
      repeatAt = now + 85;
      draw();
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("[BOOT] DeskConsole starting");
  buttons.begin();
  wakeConfigured = buttons.enableWakeup();
  if (wakeConfigured) {
    Serial.printf("[POWER] Light sleep after %lu s idle\n",
                  static_cast<unsigned long>(idleSleepMs / 1000));
  } else {
    Serial.println("[POWER] GPIO wake setup failed; sleep disabled");
  }
  game.begin();
  ui.begin();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  Serial.println("[WIFI] Connecting in station mode");
  WiFi.begin(Config::ssid, Config::password);
  configTzTime("MYT-8", "pool.ntp.org", "time.google.com");
  Serial.println("[TIME] NTP configured for MYT (UTC+8)");
  weather.begin();
  open(View::Home);
  lastInput = millis();
}

void reportConnectivity() {
  // 仅在状态改变时打印。避免周期刷新把有用的串口信息淹没。
  static int lastStatus = -1;
  static bool timeReported = false;
  const int status = WiFi.status();
  if (status != lastStatus) {
    lastStatus = status;
    if (status == WL_CONNECTED) {
      Serial.printf("[WIFI] Connected; signal=%ld dBm\n", static_cast<long>(WiFi.RSSI()));
    } else {
      Serial.printf("[WIFI] Connection status=%d\n", status);
    }
  }
  if (!timeReported && time(nullptr) >= 1577836800) {
    timeReported = true;
    Serial.println("[TIME] Clock synchronized");
  }
}

void sleepIfIdle() {
  if (!wakeConfigured || millis() - lastInput < idleSleepMs || buttons.anyPressed()) return;
  if (view == View::F1 && !game.canChangeMode()) return;
  if (bluetooth.busy()) return;

  Serial.println("[POWER] Idle timeout; entering light sleep");
  // 主动轻睡前停止 Wi-Fi，醒来后以原配置重连。RAM 中的天气和页面仍然可用。
  if (!WiFi.mode(WIFI_OFF)) {
    Serial.println("[POWER] Could not stop Wi-Fi; sleep skipped");
    lastInput = millis();
    return;
  }
  ui.setDisplayOn(false);
  // 如果按键恰好在准备阶段按下，直接走恢复流程，避免吞掉一次唤醒。
  const esp_err_t result = buttons.anyPressed() ? ESP_OK : esp_light_sleep_start();
  ui.setDisplayOn(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(Config::ssid, Config::password);
  lastInput = lastRefresh = lastWakeLow = millis();
  discardWakePress = true;
  repeatKey = Buttons::Key::None;
  navRepeatKey = Buttons::Key::None;
  scanPending = false;
  fieldPending = fieldReset = false;
  Serial.printf("[POWER] Active again; sleep status=%d, Wi-Fi reconnecting\n", result);
  draw();
}

void loop() {
  if (discardWakePress) {
    // 要求所有键稳定松开，唤醒的那一次按压不传给当前页面。
    if (buttons.anyPressed()) lastWakeLow = millis();
    if (millis() - lastWakeLow >= wakeReleaseMs) {
      buttons.begin();
      lastInput = millis();
      discardWakePress = false;
    }
    delay(1);
    return;
  }
  uint32_t pressedUs = 0;
  const auto key = buttons.poll(pressedUs);
  if (key != Buttons::Key::None) {
    lastInput = millis();
    constexpr const char* names[] = {"LEFT_TOP", "LEFT_BOTTOM", "RIGHT_TOP", "RIGHT_BOTTOM"};
    Serial.printf("[%lu ms] [KEY] %s\n", static_cast<unsigned long>(millis()),
                  names[static_cast<uint8_t>(key)]);
    const View before = view;
    handle(key, pressedUs);
    if (view == before && repeatsNavigation() &&
        (key == Buttons::Key::Up || key == Buttons::Key::Down)) {
      navRepeatKey = key;
      navRepeatAt = millis() + 420;
    }
  }
  updateNavigationKeys();
  if (view == View::Bluetooth) {
    updateBluetoothScanKey();
    if (bluetoothState == BluetoothSnapshot::State::Scanning && !bluetooth.busy()) draw();
  } else if (view == View::Calibration) {
    updateCalibrationKeys();
  } else if (view == View::F1) {
    const uint32_t now = micros();
    if (game.tick(now)) {
      if (game.state == Game::State::GoPending && buttons.playHeld()) {
        buttons.consumeHeldPlay();
        game.press(now);  // 按住起跑键等熄灯也算抢跑。
      }
      draw();
      // 五盏屏幕灯画暗后才记录起跑时刻。屏幕自身的延迟仍会影响成绩。
      game.presented(micros());
    } else if (game.debug && game.state == Game::State::Go && millis() - lastRefresh >= 100) {
      lastRefresh = millis();
      draw();
    }
    ui.updateFireworks(game);
  } else if (view != View::Launcher && millis() - lastRefresh >= 1000) {
    lastRefresh = millis();
    draw();
  }
  // F1 计时优先，网络诊断放在其他页面进行。
  if (view != View::F1) reportConnectivity();
  sleepIfIdle();
  delay(1);
}

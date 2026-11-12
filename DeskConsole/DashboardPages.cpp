#include "DashboardPages.h"

namespace {
constexpr View apps[] = {View::F1, View::Weather, View::Network, View::Bluetooth, View::Device, View::Settings};
constexpr const char* names[] = {"F1 Reaction", "Weather", "Wi-Fi", "Bluetooth", "Device", "Settings"};
constexpr int count = sizeof(apps) / sizeof(apps[0]);
static_assert(count == sizeof(names) / sizeof(names[0]));
}

void LauncherPage::draw() { ctx.ui.launcher(selected, names, count); }

// 上下键循环选择，左下打开，左上返回桌面。
void LauncherPage::input(Key key, uint32_t) {
  if (key == Key::Back) ctx.nav.open(View::Home);
  else if (key == Key::Action) {
    Serial.printf("[APPS] Open: %s\n", names[selected]);
    ctx.nav.open(apps[selected]);
  } else if (vertical(key)) {
    selected = (selected + (key == Key::Up ? count - 1 : 1)) % count;
    Serial.printf("[APPS] Selected: %s\n", names[selected]);
    draw();
  }
}

void WeatherPage::enter() {
  RefreshPage::enter();
  forecast = false;
}
// 同一页面内切换天气视图，只更新副标题和内容区域。
void WeatherPage::input(Key key, uint32_t us) {
  if (vertical(key)) {
    forecast = !forecast;
    draw();
  } else Page::input(key, us);
}

void BluetoothPage::draw() {
  const auto scan = scanner.snapshot();
  state = scan.state;
  ctx.ui.bluetooth(scan, first, namedOnly);
}

void BluetoothPage::input(Key key, uint32_t us) {
  if (key == Key::Action && !scanner.busy()) {
    scanPending = true;
    pressedAt = millis();
  } else if (vertical(key)) {
    const auto scan = scanner.snapshot();
    if (scan.state != BluetoothSnapshot::State::Ready) return;
    constexpr int rows = Interface::bluetoothRows;
    const int last = max(0, scan.count - 1) / rows * rows;
    const int next = constrain(first + (key == Key::Up ? -rows : rows), 0, last);
    if (next != first) { first = next; draw(); }
  } else Page::input(key, us);
}

// 松手触发普通扫描，长按切换过滤并扫描；后台完成后更新页面及 RGB 提示。
void BluetoothPage::tick(uint32_t now) {
  if (scanPending) {
    const bool held = ctx.buttons.held(Key::Action);
    if (!held || now - pressedAt >= 800) {
      scanPending = false;
      if (held) {
        namedOnly = !namedOnly;
        Serial.printf("[BLE] Named-only filter: %s\n", namedOnly ? "on" : "off");
      }
      first = 0;
      if (scanner.start(namedOnly)) ctx.led.play(StatusLed::Cue::Scan);
      draw();
    }
  }
  if (state == BluetoothSnapshot::State::Scanning && !scanner.busy()) {
    draw();
    ctx.led.play(state == BluetoothSnapshot::State::Ready ? StatusLed::Cue::Connected : StatusLed::Cue::Timeout);
  }
}

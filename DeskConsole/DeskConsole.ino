#include "Config.h"
#include "Buttons.h"
#include "Game.h"
#include "Interface.h"
#include "Weather.h"
#include <WiFi.h>
#include <time.h>

enum class View { Home, Launcher, F1, Weather, Network, Device, Stats };
constexpr View apps[] = {View::F1, View::Weather, View::Network, View::Device};
View view = View::Home;
uint8_t selection = 0;
bool forecast = false;
uint32_t lastRefresh = 0;

Buttons buttons;
Game game;
Interface ui;
Weather weather;

// 所有页面共用按键、屏幕和天气缓存。只有当前页面会绘制。
void draw() {
  switch (view) {
    case View::Home: ui.home(weather.snapshot()); break;
    case View::Launcher: ui.launcher(selection); break;
    case View::F1: ui.game(game); break;
    case View::Weather: ui.weather(weather.snapshot(), forecast); break;
    case View::Network: ui.network(); break;
    case View::Device: ui.device(); break;
    case View::Stats: ui.stats(game); break;
  }
}

void open(View next) {
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
    case View::Device: title = "Device"; base = 0x18E3; color = 0xB6B1; break;
    case View::Stats: title = "F1 Statistics"; base = ILI9341_BLACK; color = 0xFB0C; break;
  }
  ui.beginPage(title, base, color);
  draw();
  ui.tab(false, false, view == View::Home ? "APPS" : "BACK");
  if (view == View::Launcher || view == View::Weather) {
    ui.tab(true, false, "^");
    ui.tab(true, true, "v");
  }
  if (view == View::Launcher) ui.tab(false, true, "OPEN");
  if (view == View::Network) ui.tab(false, true, "RETRY");
  if (view == View::F1) {
    ui.tab(false, true, "MODE");
    ui.tab(true, false, "STATS");
    ui.tab(true, true, "PLAY");
  }
  ui.animateTabs(true);
}

void handle(Buttons::Key key, uint32_t pressedUs) {
  using Key = Buttons::Key;
  if (key == Key::Back) {
    if (view == View::F1) game.reset();
    open(view == View::Stats ? View::F1 : view == View::Home ? View::Launcher :
         view == View::Launcher ? View::Home : View::Launcher);
    return;
  }
  if (view == View::Launcher) {
    if (key == Key::Action) {
      forecast = false;
      open(apps[selection]);
    } else if (key == Key::Up || key == Key::Down) {
      selection = (selection + (key == Key::Up ? 3 : 1)) % 4;
      draw();
    }
  } else if (view == View::F1) {
    if (key == Key::Down) {
      game.press(pressedUs);
      draw();
    } else if (key == Key::Action && game.canChangeMode()) {
      game.debug = !game.debug;
      draw();
    } else if (key == Key::Up && game.canChangeMode()) {
      open(View::Stats);
    }
  } else if (view == View::Weather && (key == Key::Up || key == Key::Down)) {
    forecast = !forecast;
    open(View::Weather);
  } else if (view == View::Network && key == Key::Action) {
    WiFi.reconnect();
    draw();
  }
}

void setup() {
  Serial.begin(115200);
  buttons.begin();
  game.begin();
  ui.begin();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(Config::ssid, Config::password);
  configTzTime("MYT-8", "pool.ntp.org", "time.google.com");
  weather.begin();
  open(View::Home);
}

void loop() {
  uint32_t pressedUs = 0;
  const auto key = buttons.poll(pressedUs);
  if (key != Buttons::Key::None) handle(key, pressedUs);
  if (view == View::F1) {
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
  } else if (view != View::Launcher && millis() - lastRefresh >= 1000) {
    lastRefresh = millis();
    draw();
  }
  delay(1);
}

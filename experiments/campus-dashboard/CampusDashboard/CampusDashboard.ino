#include "Config.h"
#include "PageButtons.h"
#include "Weather.h"
#include "Display.h"
#include <WiFi.h>
#include <time.h>

PageButtons buttons;
Weather weather;
Display display;
uint8_t page = 0;
uint32_t lastDraw = 0;

void setup() {
  Serial.begin(115200);
  display.begin();
  buttons.begin();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(Config::ssid, Config::password);
  configTzTime("MYT-8", "pool.ntp.org", "time.google.com");
  weather.begin();  // HTTPS 在后台运行，避免阻塞按键扫描。
  display.show(page, weather.snapshot(), true);
}

void loop() {
  const uint32_t now = millis();
  int selected = page + buttons.direction(now);
  if (selected == Display::pageCount) selected = 0;
  if (selected < 0) selected = Display::pageCount - 1;
  const bool pageChanged = selected >= 0 && selected < Display::pageCount && selected != page;
  if (pageChanged) page = selected;
  if (pageChanged || now - lastDraw >= 1000) {
    lastDraw = now;
    display.show(page, weather.snapshot(), pageChanged);
  }
  delay(10);
}

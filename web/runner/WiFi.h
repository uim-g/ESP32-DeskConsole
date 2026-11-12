#pragma once
#include "Arduino.h"
constexpr int WIFI_STA=1, WIFI_OFF=0, WL_CONNECTED=3;
extern "C" bool browserOnline();
struct BrowserIP { String toString() const { return "192.0.2.2"; } };
struct {
  bool enabled=false;
  uint32_t reconnectAt=0;
  bool mode(int mode) { enabled=mode==WIFI_STA; return true; }
  void setAutoReconnect(bool) {}
  void begin(const char*,const char*) { reconnect(); }
  bool reconnect() { reconnectAt=millis()+450; return true; }
  int status() const { return enabled && browserOnline() && millis()>=reconnectAt ? WL_CONNECTED : 0; }
  String SSID() const { return "YOUR_WIFI_NAME"; }
  BrowserIP localIP() const { return {}; }
  int32_t RSSI() const { return -55; }
} inline WiFi;

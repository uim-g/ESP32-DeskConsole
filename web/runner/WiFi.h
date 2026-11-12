#pragma once
#include "Arduino.h"
constexpr int WIFI_STA=1, WIFI_OFF=0, WL_CONNECTED=3;
constexpr int WIFI_SCAN_RUNNING=-1, WIFI_SCAN_FAILED=-2;
enum wifi_auth_mode_t {
  WIFI_AUTH_OPEN, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK,
  WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_WPA2_ENTERPRISE
};
extern "C" bool browserOnline();
struct BrowserIP { String toString() const { return "192.0.2.2"; } };
struct BrowserAccessPoint {
  const char* name;
  const char* password;
  int32_t rssi;
  int channel;
  wifi_auth_mode_t auth;
};
constexpr BrowserAccessPoint demoNetworks[] = {
  {"HomeNet","password",-46,6,WIFI_AUTH_WPA2_PSK},
  {"StudioNet","password",-62,11,WIFI_AUTH_WPA2_PSK},
  {"Guest Wi-Fi","",-73,1,WIFI_AUTH_OPEN},
  {"Campus Wi-Fi","",-76,6,WIFI_AUTH_WPA2_ENTERPRISE},
  {"Mobile Hotspot","password",-81,3,WIFI_AUTH_WPA2_WPA3_PSK}
};
struct BrowserWiFi {
  bool enabled=false, credentialsOK=true, scanActive=false, demoMode=false;
  uint32_t reconnectAt=0, scanAt=0;
  String ssid="YOUR_WIFI_NAME";
  void persistent(bool) { demoMode=true; credentialsOK=false; }
  bool mode(int mode) { enabled=mode==WIFI_STA; return true; }
  void setAutoReconnect(bool) {}
  void setMinSecurity(wifi_auth_mode_t) {}
  void begin(const char* name,const char* password) {
    ssid=name;
    credentialsOK=!demoMode;
    for(const auto& ap : demoNetworks) {
      if(strcmp(name,ap.name)==0) credentialsOK=strcmp(password?password:"",ap.password)==0;
    }
    reconnect();
  }
  bool reconnect() { reconnectAt=millis()+450; return true; }
  bool disconnect(bool,bool,unsigned long) { credentialsOK=false; ssid=""; return true; }
  int status() const { return enabled && browserOnline() && credentialsOK && millis()>=reconnectAt ? WL_CONNECTED : 0; }
  String SSID() const { return ssid; }
  String SSID(uint8_t index) const { return demoNetworks[index].name; }
  BrowserIP localIP() const { return {}; }
  int32_t RSSI() const {
    for(const auto& ap : demoNetworks) if(ssid==ap.name) return ap.rssi;
    return -55;
  }
  int32_t RSSI(uint8_t index) const { return demoNetworks[index].rssi; }
  int channel(uint8_t index) const { return demoNetworks[index].channel; }
  wifi_auth_mode_t encryptionType(uint8_t index) const { return demoNetworks[index].auth; }
  int scanNetworks(bool) { scanAt=millis()+650; scanActive=true; return WIFI_SCAN_RUNNING; }
  int scanComplete() const {
    if(!scanActive) return WIFI_SCAN_FAILED;
    return millis()<scanAt?WIFI_SCAN_RUNNING:sizeof(demoNetworks)/sizeof(demoNetworks[0]);
  }
  void scanDelete() { scanActive=false; }
};
inline BrowserWiFi WiFi;

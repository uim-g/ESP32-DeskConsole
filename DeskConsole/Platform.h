#pragma once
#include <Arduino.h>

// 读数由平台采集，绘制层只接收快照。
namespace Platform {
struct Clock {
  char time[9] = "--:--:--";
  char date[28] = "Waiting for time";
};
struct Network {
  bool connected;
  String ssid;
  String address;
  int32_t signal;
  bool clockReady;
};
struct Memory { size_t total, free; };
struct Device {
  float temperature;
  Memory heap, psram, application;
  size_t flash;
  uint32_t uptimeSeconds;
};
Clock clock();
Network network();
Device device();
bool clockReady();
void connectNetwork();
bool retryNetwork();
bool stopNetwork();
int networkStatus();
bool isConnected(int status);
int32_t signal();
void configureClock();
int resetReason();
int lightSleep();
void presentDisplay();
}

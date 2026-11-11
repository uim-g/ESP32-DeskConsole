#pragma once
#include <Arduino.h>

struct BluetoothResult {
  char name[36] = {};
  char address[18] = {};
  char service[37] = {};
  int rssi = 0;
};

struct BluetoothSnapshot {
  enum class State { Idle, Scanning, Ready, Failed };
  static constexpr int limit = 24;
  State state = State::Idle;
  int count = 0;
  int detected = 0;
  BluetoothResult devices[limit];
};

class BluetoothScanner {
 public:
  bool start(bool namedOnly);
  bool busy();
  BluetoothSnapshot snapshot();

 private:
  static void worker(void* context);
  BluetoothSnapshot results;
  bool namedOnly = false;
  portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
};

// MPU6050 and the environment module share the same physical I2C bus.
#pragma once
#include "Config.h"
#include <Wire.h>

inline bool beginSensorBus() {
  static bool ready = false;
  if (!ready && Wire.begin(Config::motionSda, Config::motionScl, 100000)) {
    Wire.setTimeOut(10);
    ready = true;
  }
  return ready;
}

#include "MotionPage.h"

#ifndef __EMSCRIPTEN__
#include <Wire.h>
#endif

void MotionPage::wake() {
  RefreshPage::wake();
  address = 0;  // 唤醒后重新确认身份并退出传感器睡眠。
}

#ifndef __EMSCRIPTEN__
bool MotionPage::readRegisters(uint8_t target, uint8_t reg, uint8_t* data, size_t length) {
  Wire.beginTransmission(target);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(target, length) != length) return false;
  for (size_t i = 0; i < length; ++i) data[i] = Wire.read();
  return true;
}

bool MotionPage::initializeSensor() {
  if (!busReady) {
    busReady = Wire.begin(Config::motionSda, Config::motionScl, 100000);
    if (!busReady) return false;
    Wire.setTimeOut(10);
  }

  for (uint8_t candidate : {0x68, 0x69}) {
    uint8_t identity = 0;
    if (!readRegisters(candidate, 0x75, &identity, 1) || identity != 0x68) continue;
    Wire.beginTransmission(candidate);
    Wire.write(0x6B);  // PWR_MGMT_1，清除上电默认的 SLEEP 位。
    Wire.write(0x01);
    if (Wire.endTransmission() != 0) continue;
    address = candidate;
    return true;
  }
  return false;
}
#endif

bool MotionPage::readSample(int16_t (&axes)[6]) {
#ifdef __EMSCRIPTEN__
  return false;  // 模拟器没有实体 I²C 模块。
#else
  if (!address) {
    initializeSensor();
    // 等下次页面刷新再读，给刚唤醒的传感器留出稳定时间。
    return false;
  }

  uint8_t data[14];
  if (!readRegisters(address, 0x3B, data, sizeof(data))) {
    address = 0;
    return false;
  }
  for (size_t i = 0; i < 6; ++i) {
    const size_t offset = i * 2 + (i >= 3 ? 2 : 0);  // 跳过中间的温度寄存器。
    axes[i] = static_cast<int16_t>((static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1]);
  }
  return true;
#endif
}

void MotionPage::draw() {
  int16_t axes[6] = {};
  const bool valid = readSample(axes);
  const char* state = valid ? (address == 0x68 ? "Connected  /  0x68" : "Connected  /  0x69") :
                      address ? "Initializing..." : "Sensor not detected";
  ctx.ui.field(0, 40, 43, 240, 24, String(state), 0, Interface::accent, true);
  constexpr const char* labels[] = {"Accel X", "Accel Y", "Accel Z", "Gyro X", "Gyro Y", "Gyro Z"};
  for (uint8_t i = 0; i < 6; ++i) {
    const int y = 72 + i * 22 + (i >= 3 ? 8 : 0);
    ctx.ui.field(i + 1, 48, y, 224, 21,
                 String(labels[i]) + "     " + (valid ? String(axes[i]) : String("--")));
  }
  ctx.ui.field(7, 40, 218, 240, 17, String("RAW  /  signed 16-bit counts"), 4, Interface::accent, true);
}

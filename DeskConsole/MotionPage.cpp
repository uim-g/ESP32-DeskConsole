#include "MotionPage.h"

#ifndef __EMSCRIPTEN__
#include <Wire.h>
#endif

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
    // 固定量程并开启低通滤波，让校准和显示使用同一套换算系数。
    Wire.beginTransmission(candidate);
    Wire.write(0x1A);
    Wire.write(0x03); // CONFIG
    Wire.write(0x00); // GYRO_CONFIG: ±250 deg/s
    Wire.write(0x00); // ACCEL_CONFIG: ±2 g
    if (Wire.endTransmission() != 0) continue;
    address = candidate;
    return true;
  }
  return false;
}
#endif

bool MotionPage::readSample(int16_t (&reading)[6]) {
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
    reading[i] = static_cast<int16_t>((static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1]);
  }
  return true;
#endif
}


void MotionPage::enter() {
  if (!loaded) {
    if (storage.begin("motion", true)) {
      calibrated = storage.getBytes("cal-v1", &calibration, sizeof(calibration)) == sizeof(calibration)
                   && usable(calibration);
      storage.end();
    }
    loaded = true;
    showCorrected = calibrated;
  }
  notice = "";
  lastRoll = lastPitch = 999;
  wake();
}

void MotionPage::leave() {
  calibrating = sampling = false;
}

void MotionPage::wake() {
  sampling = valid = false;
  address = 0;
  nextRead = millis();
}

bool MotionPage::usable(const Calibration& data) {
  for (int i = 0; i < 3; ++i) {
    if (!(data.scale[i] >= 12000 && data.scale[i] <= 20000 &&
          fabsf(data.offset[i]) <= 6000 && fabsf(data.gyro[i]) <= 4000)) return false;
  }
  return true;
}

void MotionPage::input(Key key, uint32_t pressedUs) {
  if (key == Key::Back) {
    if (!calibrating) {
      Page::input(key, pressedUs);
      return;
    }
    leave();
    notice = "Calibration cancelled";
  } else if (key == Key::Action && valid && !calibrating) {
    if (attitude) ctx.ui.clearMotionBody();
    attitude = false;
    calibrating = true;
    face = 0;
    pending = {};
    notice = "";
  } else if (key == Key::Up && !calibrating) {
    if (attitude) {
      attitude = false;
      showCorrected = false;
      ctx.ui.clearMotionBody();
    } else if (calibrated && !showCorrected) {
      showCorrected = true;
    } else {
      attitude = true;
      lastRoll = lastPitch = 999;
      ctx.ui.clearMotionBody();
    }
    notice = "";
  } else if (key == Key::Down && calibrating && !sampling && valid) {
    samples = 0;
    nextRead = millis() + 500;  // 等按键带来的振动消退。
    sampling = true;
    notice = "Hold still...";
  } else {
    return;
  }
  draw();
}

void MotionPage::tick(uint32_t now) {
  if (!due(now, nextRead)) return;
  valid = readSample(axes);
  nextRead = now + (attitude ? 100 : 200);
  if (sampling) {
    if (valid) collect();
    else {
      sampling = false;
      notice = "Sensor lost. Retry face";
    }
    if (sampling) {
      nextRead = now + 20;
      return;
    }
  }
  draw();
}

void MotionPage::collect() {
  // 每面约两秒。检查峰峰值，避免把移动中的样本用于校准。
  constexpr int sampleCount = 100;
  for (int i = 0; i < 6; ++i) {
    if (!samples) {
      low[i] = high[i] = axes[i];
      sums[i] = 0;
    }
    low[i] = min(low[i], axes[i]);
    high[i] = max(high[i], axes[i]);
    sums[i] += axes[i];
  }
  if (++samples < sampleCount) return;
  sampling = false;
  float mean[6];
  for (int i = 0; i < 6; ++i) {
    mean[i] = sums[i] / static_cast<float>(sampleCount);
    if (static_cast<int32_t>(high[i]) - low[i] > (i < 3 ? 800 : 350)) {
      notice = "Movement detected. Retry";
      return;
    }
  }
  const int axis = face / 2;
  const bool negative = face % 2;
  if ((negative ? -mean[axis] : mean[axis]) < 12000 ||
      fabsf(mean[(axis + 1) % 3]) > 4500 || fabsf(mean[(axis + 2) % 3]) > 4500) {
    notice = "Wrong face. Retry";
    return;
  }

  // 正反面求偏移和 1 g 幅值，陀螺仪取六次静置平均。
  if (negative) {
    pending.offset[axis] = (positive[axis] + mean[axis]) * 0.5f;
    pending.scale[axis] = (positive[axis] - mean[axis]) * 0.5f;
  } else {
    positive[axis] = mean[axis];
  }
  for (int i = 0; i < 3; ++i) pending.gyro[i] += mean[i + 3] / 6.0f;
  notice = "";
  if (++face < 6) return;

  calibrating = false;
  notice = "Calibration out of range";
  if (!usable(pending)) return;
  bool saved = false;
  if (storage.begin("motion", false)) {
    saved = storage.putBytes("cal-v1", &pending, sizeof(pending)) == sizeof(pending);
    storage.end();
  }
  if (!saved) {
    notice = "Could not save calibration";
    return;
  }
  calibration = pending;
  showCorrected = calibrated = true;
  notice = "Calibration saved";
}

void MotionPage::draw() {
  const char* state = valid ? (address == 0x68 ? "Connected  /  0x68" : "Connected  /  0x69") :
                      address ? "Initializing..." : "Sensor not detected";
  ctx.ui.field(0, 40, 43, 240, 24, String(state), 0, Interface::accent, true);
  if (attitude) {
    if (!valid) {
      if (lastRoll != 999) ctx.ui.clearMotionBody();
      lastRoll = lastPitch = 999;
      ctx.ui.motionHint("ATTITUDE  |  Waiting for sensor");
      return;
    }
    // 先在模块坐标中校准，再按实物安装方向映射到屏幕坐标。
    float gravity[3];
    for (int i = 0; i < 3; ++i) {
      const float value = calibrated ? (axes[i] - calibration.offset[i]) / calibration.scale[i]
                                     : axes[i] / 16384.0f;
      gravity[i] = i < 2 ? -value : value;
    }
    // 模块 X 对应屏幕横滚，模块 Y 对应屏幕俯仰。交换轴时反转一轴以保持右手系。
    const float screenX = gravity[1];
    const float screenY = -gravity[0];
    const float screenZ = gravity[2];
    constexpr float degrees = 57.2957795f;
    const int roll = lroundf(atan2f(screenY, screenZ) * degrees);
    const int pitch = lroundf(atan2f(-screenX, hypotf(screenY, screenZ)) * degrees);
    if (roll != lastRoll || pitch != lastPitch) {
      ctx.ui.motionAttitude(roll, pitch);
      lastRoll = roll;
      lastPitch = pitch;
    }
    ctx.ui.motionHint(calibrated ? "ATTITUDE  |  Roll and pitch" : "ATTITUDE  |  Uncalibrated");
    return;
  }

  constexpr const char* labels[] = {"Accel X", "Accel Y", "Accel Z", "Gyro X", "Gyro Y", "Gyro Z"};
  for (uint8_t i = 0; i < 6; ++i) {
    const int y = 72 + i * 22 + (i >= 3 ? 8 : 0);
    // 轴名和数值分别缓存。变化时只擦除对应数值的区域。
    ctx.ui.field(8 + i, 48, y, 100, 21, String(labels[i]));
    String reading("--");
    if (valid) {
      if (!showCorrected) {
        reading = String(axes[i]);
      } else {
        char value[32];
        if (i < 3) snprintf(value, sizeof(value), "%.3f g", (axes[i] - calibration.offset[i]) / calibration.scale[i]);
        else snprintf(value, sizeof(value), "%.2f deg/s", (axes[i] - calibration.gyro[i - 3]) / 131.0f);
        reading = value;
      }
    }
    ctx.ui.field(i + 1, 154, y, 120, 21, reading);
  }
  constexpr const char* faces[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
  char hint[80];
  if (calibrating) {
    snprintf(hint, sizeof(hint), "%u/6  %s up  |  %s", unsigned(face + 1), faces[face],
             *notice ? notice : "Press SAMPLE");
  } else {
    snprintf(hint, sizeof(hint), "%s  |  %s", showCorrected ? "CALIBRATED" : "RAW",
             *notice ? notice : showCorrected ? "g / deg/s" : "signed 16-bit counts");
  }
  ctx.ui.motionHint(hint);
}

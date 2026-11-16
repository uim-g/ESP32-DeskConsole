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
  graphic = lastInstrument = Graphic::Level;
  uprightLevel = false;
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
  warningActive = false;
  calibrationTabs = CalibrationTabs::Idle;
}

void MotionPage::finishCalibration() {
  leave();
  ctx.ui.clearMotionBody();
  ctx.ui.replaceTab(3, "CAL");
  calibrationTabs = CalibrationTabs::RestoreViews;
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
  if (calibrationTabs != CalibrationTabs::Idle || ctx.ui.tabsAnimating()) return;
  if (key == Key::Back) {
    if (!calibrating) {
      Page::input(key, pressedUs);
      return;
    }
    finishCalibration();
    notice = "";
    ctx.ui.motionNotice("Calibration cancelled");
  } else if (key == Key::Action && !calibrating) {
    if (graphic != Graphic::None) {
      lastInstrument = graphic;
      graphic = Graphic::None;
      showCorrected = true;
      ctx.ui.clearMotionBody();
      ctx.ui.replaceTab(3, "CAL");
    } else {
      showCorrected = !showCorrected;
    }
    notice = "";
  } else if (key == Key::Up && !calibrating) {
    const bool wasData = graphic == Graphic::None;
    graphic = wasData ? lastInstrument :
        graphic == Graphic::Attitude ? Graphic::Level : Graphic::Attitude;
    if (wasData) ctx.ui.replaceTab(3, nullptr);
    uprightLevel = false;
    lastRoll = lastPitch = 999;
    ctx.ui.clearMotionBody();
    notice = "";
  } else if (key == Key::Down && graphic == Graphic::None && !calibrating) {
    if (!valid) return;
    calibrating = true;
    pending = {};
    face = 0;
    notice = "";
    ctx.ui.clearMotionBody();
    ctx.ui.startTabs(false, (1U << 1) | (1U << 2));
    calibrationTabs = CalibrationTabs::ShowSample;
  } else if (key == Key::Down && calibrating && !sampling && valid) {
    const float current[3] = {float(axes[0]), float(axes[1]), float(axes[2])};
    if (!faceAligned(current)) {
      showCalibrationFeedback("Align the axis before sampling");
      draw();
      return;
    }
    samples = 0;
    warningActive = false;
    nextRead = millis() + 500;  // 等按键带来的振动消退。
    sampling = true;
    notice = "Hold still...";
  } else {
    return;
  }
  draw();
}

void MotionPage::tick(uint32_t now) {
  if (calibrationTabs != CalibrationTabs::Idle && !ctx.ui.tabsAnimating()) {
    if (calibrationTabs == CalibrationTabs::ShowSample) {
      ctx.ui.replaceTab(3, "SAMPLE");
    } else {
      ctx.ui.tab(false, true, "DATA");
      ctx.ui.tab(true, false, "VIEW");
      ctx.ui.startTabs(true, (1U << 1) | (1U << 2));
    }
    calibrationTabs = CalibrationTabs::Idle;
  }
  if (warningActive && due(now, warningFrameAt)) {
    if (now - warningStarted >= 1400) {
      warningActive = false;
      notice = "";
    } else {
      const uint32_t elapsed = now - warningStarted;
      warningFrameAt = !faceAccepted && elapsed < 280 ? now + 40 : warningStarted + 1400;
    }
    drawCalibration();
  }
  if (!due(now, nextRead)) return;
  valid = readSample(axes);
  nextRead = now + (graphic == Graphic::None ? 200 : 100);
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
      showCalibrationFeedback("Movement detected. Retry");
      return;
    }
  }
  const int axis = face / 2;
  const float accel[3] = {mean[0], mean[1], mean[2]};
  if (!faceAligned(accel)) {
    showCalibrationFeedback("Align the axis before sampling");
    return;
  }

  // 正反面求偏移和 1 g 幅值，陀螺仪取六次静置平均。
  if (face % 2) {
    pending.offset[axis] = (firstFaceReading + mean[axis]) * 0.5f;
    // X/Y 的负原始读数先采样，Z 的正原始读数先采样。
    pending.scale[axis] = (targetRawNegative() ? firstFaceReading - mean[axis] :
                                                mean[axis] - firstFaceReading) * 0.5f;
  } else {
    firstFaceReading = mean[axis];
  }
  for (int i = 0; i < 3; ++i) pending.gyro[i] += mean[i + 3] / 6.0f;
  notice = "";
  if (++face < 6) {
    showCalibrationFeedback("Face accepted", true);
    return;
  }

  finishCalibration();
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
  notice = "";
  ctx.ui.motionNotice("Calibration saved");
}

void MotionPage::draw() {
  const char* state = valid ? (address == 0x68 ? "Connected 0x68" : "Connected 0x69") :
                      address ? "Initializing..." : "No sensor";
  ctx.ui.field(0, 164, 10, 114, 23, String(state), 4, Interface::accent, true);
  if (calibrating) {
    drawCalibration();
    return;
  }
  if (graphic != Graphic::None) {
    const bool level = graphic == Graphic::Level;
    if (!valid) {
      if (lastRoll != 999) ctx.ui.clearMotionBody();
      lastRoll = lastPitch = 999;
      ctx.ui.motionMode(level ? "LEVEL" : "ATTITUDE");
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
    // 实机正常直立时，旧坐标的 +X 朝上。修正两种直立视图的共同零位。
    const float uprightX = -screenY;
    const float uprightY = screenX;
    constexpr float degrees = 57.2957795f;
    // 水平仪以屏幕法线为零位，姿态仪以屏幕上边缘朝上为零位。
    const int roll = lroundf((level ? atan2f(screenY, screenZ) :
                                      atan2f(uprightX, uprightY)) * degrees);
    const int pitch = lroundf((level ? atan2f(-screenX, hypotf(screenY, screenZ)) :
                                       atan2f(screenZ, hypotf(uprightX, uprightY))) * degrees);
    if (level) {
        // 测量重力与屏幕平面的夹角，板子立在哪一边都能触发。
        const float inPlane = hypotf(screenX, screenY);
        const float inclination = atan2f(inPlane, fabsf(screenZ)) * degrees;
        const bool upright = inclination >= (uprightLevel ? 55.0f : 65.0f);
        const bool layoutChanged = upright != uprightLevel;
        if (layoutChanged) ctx.ui.clearMotionBody();
        if (upright) {
          // 分别测量两侧平面内的偏斜，避免把 X 和 Z 合成一个角度。
          // 绕 X 转动改变法向重力分量，绕 Z 转动改变屏幕平面内的分量。
          const int tiltX = lroundf(atan2f(screenZ, fabsf(uprightY)) * degrees);
          const int tiltZ = lroundf(atan2f(uprightX, fabsf(uprightY)) * degrees);
          if (layoutChanged || lastRoll == 999 || tiltX != lastTiltX) ctx.ui.motionTube(false, tiltX);
          if (layoutChanged || lastRoll == 999 || tiltZ != lastTiltZ) ctx.ui.motionTube(true, tiltZ);
          lastTiltX = tiltX;
          lastTiltZ = tiltZ;
        } else if (layoutChanged || roll != lastRoll || pitch != lastPitch) {
          ctx.ui.motionLevel(roll, pitch);
        }
        uprightLevel = upright;
    } else if (roll != lastRoll || pitch != lastPitch) {
      ctx.ui.motionAttitude(roll, -pitch);
    }
    lastRoll = roll;
    lastPitch = pitch;
    ctx.ui.motionMode(level ? "LEVEL" : "ATTITUDE");
    return;
  }

  ctx.ui.field(11, 38, 59, 100, 25, showCorrected ? "Accel (g)" : "Accel",
               0, Interface::accent, true);
  ctx.ui.field(12, 182, 59, 100, 25, showCorrected ? "Gyro (deg/s)" : "Gyro",
               0, Interface::accent, true);
  constexpr const char* labels[] = {"X", "Y", "Z"};
  for (uint8_t axis = 0; axis < 3; ++axis) {
    ctx.ui.field(8 + axis, 144, 93 + axis * 28, 32, 24, labels[axis],
                 0, Interface::accent, true);
  }
  for (uint8_t i = 0; i < 6; ++i) {
    const int x = i < 3 ? 38 : 182;
    const int y = 93 + (i % 3) * 28;
    String reading("--");
    if (valid && (!showCorrected || calibrated)) {
      if (!showCorrected) {
        reading = String(axes[i]);
      } else {
        char value[32];
        if (i < 3) snprintf(value, sizeof(value), "%.3f", (axes[i] - calibration.offset[i]) / calibration.scale[i]);
        else snprintf(value, sizeof(value), "%.2f", (axes[i] - calibration.gyro[i - 3]) / 131.0f);
        reading = value;
      }
    }
    // 每个读数独立缓存，标题和轴名保持固定。
    ctx.ui.field(i + 1, x, y, 100, 24, reading, 0, ILI9341_WHITE, true);
  }
  ctx.ui.field(7, 39, 181, 242, 20,
               *notice ? notice : showCorrected && !calibrated ? "Press CAL to set up" : " ",
               4, Interface::accent, true);
  ctx.ui.motionMode(showCorrected ? "CALIBRATED" : "RAW");
}

bool MotionPage::faceAligned(const float (&accel)[3]) const {
  const int axis = face / 2;
  return (targetRawNegative() ? -accel[axis] : accel[axis]) >= 12000 &&
         fabsf(accel[(axis + 1) % 3]) <= 4500 && fabsf(accel[(axis + 2) % 3]) <= 4500;
}

void MotionPage::showCalibrationFeedback(const char* message, bool accepted) {
  notice = message;
  faceAccepted = accepted;
  warningStarted = millis();
  warningFrameAt = warningStarted + (accepted ? 1400 : 40);
  warningActive = true;
}

void MotionPage::drawCalibration() {
  constexpr const char* faces[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
  const int axis = face / 2;
  const float current[3] = {float(axes[0]), float(axes[1]), float(axes[2])};
  const bool aligned = valid && faceAligned(current);
  ctx.ui.field(1, 36, 47, 248, 23, String("CALIBRATING  ") + String(face + 1) + " / 6",
               0, Interface::accent, true);
  ctx.ui.field(2, 58, 73, 133, 48, String(faces[face]) + "  UP", 1, ILI9341_WHITE, true);
  ctx.ui.motionCalibrationGuide(targetRawNegative(), axes[axis], valid, aligned);
  ctx.ui.field(3, 36, 127, 248, 22,
               axis == 2 ? (face % 2 ? "Lay module face down" : "Lay module face up") :
                           "Stand module on its edge",
               0, ILI9341_WHITE, true);
  ctx.ui.field(4, 36, 151, 248, 22,
               String("Raw ") + faces[face][1] + (targetRawNegative() ? " near -16000" : " near +16000"),
               4, ILI9341_WHITE, true);
  const String readings = valid ? String("X ") + String(axes[0]) + "   Y " + String(axes[1]) +
                                  "   Z " + String(axes[2]) : String("Waiting for sensor");
  ctx.ui.field(5, 36, 175, 248, 20, readings, 4, Interface::accent, true);
  constexpr int shake[] = {0, -5, 5, -4, 4, -2, 2};
  const uint32_t frame = (millis() - warningStarted) / 40;
  const int offset = warningActive && !faceAccepted && frame < sizeof(shake) / sizeof(shake[0]) ? shake[frame] : 0;
  const char* prompt = sampling ? "Hold still..." : *notice ? notice :
                       aligned ? "Aligned. Press SAMPLE" : "Rotate to match the target";
  using Tone = Interface::CalibrationTone;
  const Tone tone = sampling ? Tone::Normal :
      (warningActive ? faceAccepted : aligned) ? Tone::Success : Tone::Alert;
  ctx.ui.motionCalibrationPrompt(prompt, tone, offset);
  ctx.ui.motionMode("CALIBRATING");
}

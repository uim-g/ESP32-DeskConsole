#pragma once
#include <Preferences.h>
#include "Page.h"

class MotionPage : public Page {
 public:
  using Page::Page;
  PageStyle style() const override {
    return {"Motion", 0x18E3, 0xB6B1, {"BACK", "CAL", "VIEW", "SAMPLE"}};
  }
  void enter() override;
  void leave() override;
  void wake() override;
  void draw() override;
  void tick(uint32_t now) override;
  void input(Key key, uint32_t pressedUs) override;
  bool canSleep() const override { return !calibrating; }

 private:
  struct Calibration {
    float offset[3], scale[3], gyro[3];
  };
  static bool usable(const Calibration& data);
  void collect();
  bool readSample(int16_t (&axes)[6]);
  bool readRegisters(uint8_t address, uint8_t reg, uint8_t* data, size_t length);
  bool initializeSensor();

  Preferences storage;
  Calibration calibration{};
  bool loaded = false, calibrated = false, showCorrected = false;
  bool attitude = false;
  int lastRoll = 999, lastPitch = 999;
  bool calibrating = false, sampling = false, valid = false, busReady = false;
  uint8_t address = 0, face = 0, samples = 0;
  uint32_t nextRead = 0;
  int16_t axes[6]{}, low[6]{}, high[6]{};
  int32_t sums[6]{};
  float positive[3]{};
  Calibration pending{};
  const char* notice = "";
};

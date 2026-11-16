#pragma once
#include "Page.h"

// 仅在 Motion 页面读取 MPU6050，读数保留为有符号原始计数。
class MotionPage : public RefreshPage {
 public:
  using RefreshPage::RefreshPage;
  PageStyle style() const override { return {"Motion", 0x18E3, 0xB6B1, {"BACK"}}; }
  void wake() override;
  void draw() override;

 private:
  bool readSample(int16_t (&axes)[6]);
  bool readRegisters(uint8_t address, uint8_t reg, uint8_t* data, size_t length);
  bool initializeSensor();
  bool busReady = false;
  uint8_t address = 0;
};

#pragma once
#include "Adafruit_GFX.h"
#include "SPI.h"
constexpr uint16_t ILI9341_BLACK=0, ILI9341_WHITE=0xffff, ILI9341_RED=0xf800, ILI9341_YELLOW=0xffe0;
constexpr uint8_t ILI9341_DISPON=0x29, ILI9341_DISPOFF=0x28;
void markDisplayDirty();
void displayPower(bool on);
inline uint16_t pixels[320*240]={};
class Adafruit_ILI9341 : public Adafruit_GFX {
 public:
  Adafruit_ILI9341(SPIClass*,int,int,int) : Adafruit_GFX(320,240) {}
  void begin(unsigned) {}
  // The browser framebuffer is already in landscape orientation.
  void setRotation(uint8_t) override {}
  void sendCommand(uint8_t command) { displayPower(command==ILI9341_DISPON); }
  void drawPixel(int16_t x,int16_t y,uint16_t color) override {
    if(unsigned(x)<320 && unsigned(y)<240) pixels[y*320+x]=color;
  }
  void startWrite() override { markDisplayDirty(); }
  void endWrite() override {}
};

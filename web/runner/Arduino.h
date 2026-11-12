#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <type_traits>
#include <emscripten.h>

#define PROGMEM
class __FlashStringHelper;
using std::max;
using std::min;
using std::isnan;
using std::isfinite;
using byte = uint8_t;
using boolean = bool;
using portMUX_TYPE = int;
constexpr int portMUX_INITIALIZER_UNLOCKED = 0;
constexpr int LOW = 0, HIGH = 1, INPUT_PULLUP = 2;
template<class T> T constrain(T n, T low, T high) { return std::clamp(n, low, high); }

class String {
  std::string value;
 public:
  String() = default;
  String(const char* s) : value(s ? s : "") {}
  String(char c) : value(1,c) {}
  template<class T, std::enable_if_t<std::is_integral_v<T>,int> = 0>
  String(T n) : value(std::to_string(n)) {}
  String(double n, int digits = 2) {
    char buffer[96];
    std::snprintf(buffer,sizeof(buffer),"%.*f",digits,n);
    value = buffer;
  }
  const char* c_str() const { return value.c_str(); }
  unsigned length() const { return value.size(); }
  void remove(unsigned start) { if(start<length()) value.resize(start); }
  void replace(char old, char next) { std::replace(value.begin(),value.end(),old,next); }
  int indexOf(char c) const {
    auto index=value.find(c);
    return index==std::string::npos ? -1 : int(index);
  }
  String substring(unsigned start,unsigned end=~0u) const {
    String result;
    if(start<length()) result.value=value.substr(start,end-start);
    return result;
  }
  bool operator==(const String& s) const { return value==s.value; }
  bool operator!=(const String& s) const { return value!=s.value; }
  friend String operator+(String a,const String& b) { a.value+=b.value; return a; }
};

inline uint32_t micros() { return uint32_t(uint64_t(emscripten_get_now()*1000)); }
inline uint32_t millis() { return uint32_t(emscripten_get_now()); }
inline void pinMode(int,int) {}
int digitalRead(int pin);
void delay(unsigned ms);
extern "C" void rgbLedWrite(int pin,int r,int g,int b);
inline long random(long low,long high) { return low+std::rand()%(high-low); }
inline void configTzTime(const char*,const char*,const char*) {
  setenv("TZ","MYT-8",1);
  tzset();
}
inline bool getLocalTime(tm* local,unsigned = 0) {
  auto now=time(nullptr);
  return localtime_r(&now,local)!=nullptr;
}
struct {
  void begin(int) {}
  void println(const char* message) { std::puts(message); }
  void flush() {}
  template<class... Args> void printf(const char* format,Args... args) {
    std::printf(format,args...);
  }
} inline Serial;
struct {
  const char* getChipModel() const { return "ESP32-S3"; }
  size_t getHeapSize() const { return 320*1024; }
  size_t getFreeHeap() const { return 220*1024; }
  size_t getPsramSize() const { return 8*1024*1024; }
  size_t getFreePsram() const { return getPsramSize()-2048; }
  size_t getFlashChipSize() const { return 16*1024*1024; }
  size_t getSketchSize() const { return 1370247; }
} inline ESP;
inline float temperatureRead() { return 40.0f; }
inline float radians(float degrees) { return degrees * 0.017453292519943295f; }

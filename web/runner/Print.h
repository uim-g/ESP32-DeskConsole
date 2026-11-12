#pragma once
#include "Arduino.h"
class Print {
 public:
  virtual size_t write(uint8_t) = 0;
  size_t print(const String& value) {
    const char* s=value.c_str();
    size_t count=0;
    while(*s) count+=write(uint8_t(*s++));
    return count;
  }
  template<class... Args> size_t printf(const char* format,Args... args) {
    char text[128];
    std::snprintf(text,sizeof(text),format,args...);
    return print(text);
  }
};

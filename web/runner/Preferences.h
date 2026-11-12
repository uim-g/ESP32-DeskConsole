#pragma once
#include "Arduino.h"
extern "C" size_t readPreference(const char* group,const char* key,void* dest,size_t size);
extern "C" size_t writePreference(const char* group,const char* key,const void* src,size_t size);
class Preferences {
  const char* group="";
 public:
  bool begin(const char* name,bool) { group=name; return true; }
  size_t getBytesLength(const char* key) { return readPreference(group,key,nullptr,0); }
  size_t getBytes(const char* key,void* dst,size_t size) { return readPreference(group,key,dst,size); }
  size_t putBytes(const char* key,const void* src,size_t size) { return writePreference(group,key,src,size); }
  uint32_t getUInt(const char* key,uint32_t fallback) {
    uint32_t n;
    return getBytes(key,&n,sizeof(n))==sizeof(n)?n:fallback;
  }
  uint8_t getUChar(const char* key,uint8_t fallback) {
    uint8_t n;
    return getBytes(key,&n,sizeof(n))==sizeof(n)?n:fallback;
  }
  size_t putUInt(const char* key,uint32_t n) { return putBytes(key,&n,sizeof(n)); }
  size_t putUChar(const char* key,uint8_t n) { return putBytes(key,&n,sizeof(n)); }
};

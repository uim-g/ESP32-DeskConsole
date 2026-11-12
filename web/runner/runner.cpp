#include "Arduino.h"
#include "Adafruit_ILI9341.h"
#include "esp_sleep.h"
#include "Weather.h"
#if __has_include("Bluetooth.h")
#include "Bluetooth.h"
#endif

void setup();
void loop();
static bool dirty=true, powered=true;
void markDisplayDirty(){dirty=true;}
void displayPower(bool on){powered=on;dirty=true;}
EM_JS(void, render, (const uint16_t* data, bool on), { desk.render(data, on, HEAPU16); });
EM_JS(int, keys, (), { return desk.keys; });
EM_JS(bool, browserOnline, (), { return navigator.onLine; });
EM_JS(void, rgbLedWrite, (int pin,int r,int g,int b), { desk.led(r,g,b); });
EM_JS(size_t, readPreference, (const char* group,const char* key,void* dst,size_t size), {
  return desk.read(UTF8ToString(group),UTF8ToString(key),dst,size,HEAPU8);
});
EM_JS(size_t, writePreference, (const char* group,const char* key,const void* src,size_t size), {
  return desk.write(UTF8ToString(group),UTF8ToString(key),HEAPU8.subarray(src,src+size));
});
int digitalRead(int pin){return (keys() & (1<<(pin-4))) ? LOW : HIGH;}
void presentBrowserDisplay(){
  if(dirty){render(pixels,powered);dirty=false;}
}
void delay(unsigned ms){
  presentBrowserDisplay();
  emscripten_sleep(ms);
}
esp_err_t esp_light_sleep_start(){
  while(!keys()) delay(25);
  return ESP_OK;
}

EM_JS(void, startWeather, (), { desk.startWeather(); });
EM_JS(int, weatherVersion, (), { return desk.weather.version; });
EM_JS(double, weatherNumber, (const char* field,int index), {
  const value=desk.weatherValue(UTF8ToString(field),index);
  return typeof value==="number"?value:NaN;
});
EM_JS(void, weatherText, (const char* field,int index,char* out,int size), {
  let value=desk.weatherValue(UTF8ToString(field),index);
  if(typeof value!=="string") value="";
  if(size===6) value=value.slice(11,16)||"--:--";
  stringToUTF8(value,out,size);
});
EM_JS(bool, weatherStale, (), { return desk.weather.stale; });
void Weather::begin(){startWeather();}
WeatherData Weather::snapshot(){
  static int previous=-1;
  const int version=weatherVersion();
  if(version!=previous && version>0){
    previous=version;
    cached.available=true;
    cached.temp=weatherNumber("temperature_2m",-1);
    cached.feels=weatherNumber("apparent_temperature",-1);
    cached.humidity=weatherNumber("relative_humidity_2m",-1);
    cached.pressure=weatherNumber("surface_pressure",-1);
    cached.wind=weatherNumber("wind_speed_10m",-1);
    cached.rain=weatherNumber("precipitation",-1);
    const auto code=weatherNumber("weather_code",-1);
    cached.code=isnan(code)?-1:static_cast<int>(code);
    weatherText("time",-1,cached.updated,sizeof cached.updated);
    weatherText("sunrise",0,cached.sunrise,sizeof cached.sunrise);
    weatherText("sunset",0,cached.sunset,sizeof cached.sunset);
    for(int i=0;i<3;i++){
      const auto dayCode=weatherNumber("weather_code",i);
      cached.dailyCode[i]=isnan(dayCode)?-1:static_cast<int>(dayCode);
      cached.high[i]=weatherNumber("temperature_2m_max",i);
      cached.low[i]=weatherNumber("temperature_2m_min",i);
      cached.chance[i]=weatherNumber("precipitation_probability_max",i);
      weatherText("time",i,cached.dates[i],sizeof cached.dates[i]);
    }
  }
  cached.stale=weatherStale();
  return cached;
}
#if __has_include("Bluetooth.h")
static uint32_t scanEnds;
bool BluetoothScanner::start(bool named){
  if(busy()) return false;
  namedOnly=named;
  results={};
  results.state=BluetoothSnapshot::State::Scanning;
  scanEnds=millis()+4000;
  return true;
}
bool BluetoothScanner::busy(){return snapshot().state==BluetoothSnapshot::State::Scanning;}
BluetoothSnapshot BluetoothScanner::snapshot(){
  if(results.state==BluetoothSnapshot::State::Scanning && millis()>=scanEnds){
    results.state=BluetoothSnapshot::State::Ready;
    const char* names[]={"Demo headset","","Demo sensor","Demo watch","","Demo keyboard"};
    results.detected=6;
    for(int i=0;i<6;i++){
      if(namedOnly && !*names[i]) continue;
      auto& device=results.devices[results.count++];
      std::snprintf(device.name,sizeof device.name,"%s",names[i]);
      std::snprintf(device.address,sizeof device.address,"02:00:00:00:00:%02X",i+1);
      std::snprintf(device.service,sizeof device.service,"%s",i==2?"180F":"");
      device.rssi=-42-i*7;
    }
  }
  return results;
}
#endif
int main(){
  std::srand(static_cast<unsigned>(time(nullptr)));
  configTzTime(nullptr,nullptr,nullptr);
  setup();
  for(;;) loop();
}

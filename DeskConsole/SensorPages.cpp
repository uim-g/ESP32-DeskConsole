#include "SensorPages.h"
#include <math.h>

#ifndef __EMSCRIPTEN__
#include "SensorBus.h"

void EnvironmentPage::detect() {
  if (!beginSensorBus()) return;
  for (uint8_t address : {0x76, 0x77}) {
    Wire.beginTransmission(address);
    Wire.write(0xD0);  // Bosch chip ID
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, size_t{1}) != 1) continue;
    const int id = Wire.read();
    if (id == 0x60 && bme.begin(address, &Wire)) sensor = Sensor::Bme280;
    if (id == 0x58 && bmp.begin(address)) sensor = Sensor::Bmp280;
    if (sensor != Sensor::None) break;
  }
}
#endif

void EnvironmentPage::draw() {
  String status = "Hardware required";
  float temperature = NAN, pressure = NAN;
#ifndef __EMSCRIPTEN__
  if (sensor == Sensor::None) detect();
  status = "No sensor";
  if (sensor != Sensor::None) {
    const bool isBme = sensor == Sensor::Bme280;
    temperature = isBme ? bme.readTemperature() : bmp.readTemperature();
    pressure = (isBme ? bme.readPressure() : bmp.readPressure()) / 100.0f;
    if (isfinite(temperature) && isfinite(pressure) && pressure > 0) {
      status = isBme ? "BME280" : "BMP280";
    } else {
      temperature = pressure = NAN;
      status = "Read failed";
      sensor = Sensor::None;
    }
  }
#endif
  ctx.ui.environmentGauge(0, temperature);
  ctx.ui.environmentGauge(1, pressure);
  ctx.ui.field(6, 46, 211, 228, 18, status, 4, 0xA69A, true);
}

void MicrophonePage::enter() {
  hasSamples = false;
  sampledAt = waveAt = millis();
  waveNext = 0;
  pendingAmplitude = 0;
  for (auto& amplitude : wave) amplitude = 0;
#ifndef __EMSCRIPTEN__
  digitalWrite(Config::micSelect, LOW);
  pinMode(Config::micSelect, OUTPUT);
  i2s.setPins(Config::micClock, Config::micWordSelect, -1, Config::micData);
  i2s.setTimeout(4);
  started = i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
#endif
}

void MicrophonePage::leave() {
#ifndef __EMSCRIPTEN__
  if (started) i2s.end();
  started = false;
#endif
}

void MicrophonePage::tick(uint32_t now) {
#ifndef __EMSCRIPTEN__
  if (!started || now - sampledAt < 20) return;
  sampledAt = now;
  int32_t samples[320];  // 20 ms at 16 kHz.
  const size_t count = i2s.readBytes(reinterpret_cast<char*>(samples), sizeof(samples)) / sizeof(samples[0]);
  hasSamples = false;
  if (count) {
    // 去除采样窗口内的直流偏移。满量程值是数字参考，不代表声压级。
    float mean = 0;
    for (size_t i = 0; i < count; ++i) mean += samples[i] / 2147483648.0f;
    mean /= count;
    float energy = 0;
    for (size_t i = 0; i < count; ++i) {
      const float centered = samples[i] / 2147483648.0f - mean;
      energy += centered * centered;
    }
    const float rms = sqrtf(energy / count);
    hasSamples = true;
    // Logarithmic scaling makes quiet sounds visible.
    const float normalized = (20.0f * log10f(max(rms, 0.0001f)) + 72.0f) / 60.0f;
    const auto amplitude = static_cast<uint8_t>(lroundf(33 * constrain(normalized, 0.0f, 1.0f)));
    pendingAmplitude = max(pendingAmplitude, amplitude);
  }
  if (now - waveAt >= 40) {
    waveAt = now;
    wave[waveNext] = pendingAmplitude;
    waveNext = (waveNext + 1) % waveColumns;
    pendingAmplitude = 0;
    draw();
  }
#endif
}

void MicrophonePage::draw() {
#ifdef __EMSCRIPTEN__
  const char* status = "Hardware required";
#else
  const char* status = started ? (!hasSamples ? "Waiting for samples" : "I2S - Left channel") : "I2S unavailable";
#endif
  ctx.ui.field(0, 54, 64, 212, 24, status, 0, style().accent, true);
  ctx.ui.microphoneWave(wave, waveColumns, waveNext);
  ctx.ui.field(1, 54, 195, 212, 22, "Input waveform", 4, 0xA69A, true);
}

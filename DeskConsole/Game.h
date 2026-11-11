#pragma once
#include <Arduino.h>
#include <Preferences.h>

// F1 状态机处理计时和存档。屏幕和实体按键由入口负责。
class Game {
 public:
  enum class State { Idle, Lighting, Waiting, GoPending, Go, Result, FalseStart, Timeout };

  void begin() {
    // 只接受与当前结构大小匹配的存档。旧版成绩和校准使用独立键。
    storageReady = storage.begin("desk-f1", false);
    if (storageReady && storage.getBytesLength("stats") == sizeof(stats)) {
      if (storage.getBytes("stats", &stats, sizeof(stats)) != sizeof(stats)) stats = {};
    }
    if (storageReady) {
      lastUs = storage.getUInt("last-us", 0);
      lastOutcome = static_cast<State>(storage.getUChar("last-state", 0));
      Calibration loaded{};
      if (storage.getBytesLength("calibration") == sizeof(loaded) &&
          storage.getBytes("calibration", &loaded, sizeof(loaded)) == sizeof(loaded) &&
          loaded.factor >= 50 && loaded.factor <= 150 &&
          loaded.offsetMs >= -500 && loaded.offsetMs <= 500) calibration = loaded;
    }
    Serial.printf("[F1] Flash=%s rounds=%lu best=%.3f ms; factor=%.2f offset=%+d ms\n",
                  storageReady ? "ready" : "unavailable", static_cast<unsigned long>(total()),
                  best() / 1000.0f, factor(), offsetMs());
  }

  // 清除本局状态，已完成的统计仍保留。
  void reset() {
    state = State::Idle;
    lights = 0;
    resultUs = 0;
  }

  void press(uint32_t pressedUs) {
    // 使用消抖前记录的按下时间，避免把 30 ms 消抖算进成绩。
    switch (state) {
      case State::Idle:
        started = micros();
        state = State::Lighting;
        Serial.printf("[F1] Round started (%s)\n", debug ? "DEBUG" : "NORMAL");
        break;
      case State::Lighting:
      case State::Waiting:
      case State::GoPending:
        finish(State::FalseStart);
        break;
      case State::Go: {
        const int32_t elapsed = static_cast<int32_t>(pressedUs - goUs);
        if (elapsed < 0) finish(State::FalseStart);
        else if (static_cast<uint32_t>(elapsed) >= deadline) finish(State::Timeout);
        else {
          resultUs = elapsed;
          finish(State::Result);
        }
        break;
      }
      default:
        reset();
        Serial.println("[F1] Result dismissed; ready for next round");
        break;
    }
  }

  bool tick(uint32_t now) {
    // 每次只推进一个灯或状态，不用 delay() 等待起跑。
    if (state == State::Lighting && now - started >= 800000UL) {
      started = now;
      if (++lights == 5) {
        state = State::Waiting;
        waitUs = random(1000, 3001) * 1000UL;
      }
      Serial.printf("[F1] Light %u/5\n", static_cast<unsigned>(lights));
      if (state == State::Waiting) Serial.println("[F1] Waiting for lights out");
      return true;
    }
    if (state == State::Waiting && now - started >= waitUs) {
      lights = 0;
      state = State::GoPending;
      return true;
    }
    // 给已开始消抖的按键留出确认时间，成绩仍按 10 秒边界判断。
    if (state == State::Go && now - goUs >= deadline + 30000UL) {
      finish(State::Timeout);
      return true;
    }
    return false;
  }

  void presented(uint32_t now) {
    if (state == State::GoPending) {
      goUs = now;
      state = State::Go;
      Serial.println("[F1] GO: reaction timer started");
    }
  }

  State state = State::Idle;
  uint8_t lights = 0;
  uint32_t resultUs = 0;
  uint32_t best() const { return stats.bestUs; }
  uint32_t total() const { return stats.total; }
  bool saved() const { return storageReady && lastSaveOK; }
  float correctedMs(uint32_t us) const {
    // 原始成绩以微秒保存，补偿只应用于显示的毫秒值。
    const float ms = (us / 1000.0f) * factor() + calibration.offsetMs;
    return ms > 0 ? ms : 0;
  }
  float factor() const { return calibration.factor / 100.0f; }
  int offsetMs() const { return calibration.offsetMs; }
  void adjustCalibration(bool offset, int direction) {
    if (offset) {
      calibration.offsetMs = constrain(calibration.offsetMs + direction, -500, 500);
    } else {
      calibration.factor = constrain(calibration.factor + direction, 50, 150);
    }
  }
  void resetCalibration() { calibration = {}; }
  bool saveCalibration() {
    return storageReady &&
           storage.putBytes("calibration", &calibration, sizeof(calibration)) == sizeof(calibration);
  }
  void clearStats() {
    stats = {};
    lastUs = 0;
    lastOutcome = State::Idle;
    reset();
    saveStats();
    Serial.printf("[F1] Stats cleared; flash=%s\n", saved() ? "saved" : "failed");
  }
  bool debug = false;
  uint32_t lastUs = 0;
  State lastOutcome = State::Idle;
  bool canChangeMode() const {
    return state == State::Idle || state == State::Result ||
           state == State::FalseStart || state == State::Timeout;
  }
  uint32_t liveUs() const { return micros() - goUs; }

 private:
  static constexpr uint32_t deadline = 10000000UL;
  struct Stats {
    uint32_t bestUs = 0;
    uint32_t total = 0;
  } stats;
  struct Calibration {
    int32_t factor = 100;
    int32_t offsetMs = 0;
  } calibration;
  Preferences storage;
  bool storageReady = false;
  bool lastSaveOK = true;
  uint32_t started = 0, waitUs = 0, goUs = 0;

  // 结算时更新上一局、最佳和总局数。中途返回不计入。
  void finish(State outcome) {
    state = outcome;
    lights = 0;
    lastOutcome = outcome;
    lastUs = outcome == State::Result ? resultUs : 0;
    if (stats.total != UINT32_MAX) ++stats.total;
    if (outcome == State::Result && resultUs > 0 && (!stats.bestUs || resultUs < stats.bestUs)) {
      stats.bestUs = resultUs;
    }
    saveStats();
    if (outcome == State::Result) {
      Serial.printf("[F1] Result: RAW=%.3f ms CAL=%.3f ms\n",
                    resultUs / 1000.0f, correctedMs(resultUs));
    } else {
      Serial.println(outcome == State::FalseStart ? "[F1] FALSE START" : "[F1] TIMEOUT");
    }
    Serial.printf("[F1] Rounds=%lu best=%.3f ms; flash=%s\n",
                  static_cast<unsigned long>(total()), best() / 1000.0f,
                  saved() ? "saved" : "failed");
  }

  void saveStats() {
    lastSaveOK = storageReady && storage.putBytes("stats", &stats, sizeof(stats)) == sizeof(stats);
    if (storageReady) {
      const bool lastTimeSaved = storage.putUInt("last-us", lastUs) == sizeof(lastUs);
      const bool lastStateSaved = storage.putUChar("last-state", static_cast<uint8_t>(lastOutcome)) == 1;
      lastSaveOK = lastSaveOK && lastTimeSaved && lastStateSaved;
    }
  }
};

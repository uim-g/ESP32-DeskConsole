#pragma once
#include <Arduino.h>
#include <Preferences.h>

class Game {
 public:
  enum class State { Idle, Lighting, Waiting, GoPending, Go, Result, FalseStart, Timeout };

  void begin() {
    storageReady = storage.begin("desk-f1", false);
    if (storageReady && storage.getBytesLength("stats") == sizeof(stats)) {
      if (storage.getBytes("stats", &stats, sizeof(stats)) != sizeof(stats)) stats = {};
    }
    if (storageReady) {
      lastUs = storage.getUInt("last-us", 0);
      lastOutcome = static_cast<State>(storage.getUChar("last-state", 0));
    }
  }

  void reset() {
    state = State::Idle;
    lights = 0;
    resultUs = 0;
  }

  void press(uint32_t pressedUs) {
    switch (state) {
      case State::Idle:
        started = micros();
        state = State::Lighting;
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
      default: reset(); break;
    }
  }

  bool tick(uint32_t now) {
    if (state == State::Lighting && now - started >= 800000UL) {
      started = now;
      if (++lights == 5) {
        state = State::Waiting;
        waitUs = random(1000, 3001) * 1000UL;
      }
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
    }
  }

  State state = State::Idle;
  uint8_t lights = 0;
  uint32_t resultUs = 0;
  uint32_t best() const { return stats.bestUs; }
  uint32_t total() const { return stats.total; }
  bool saved() const { return storageReady && lastSaveOK; }
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
  Preferences storage;
  bool storageReady = false;
  bool lastSaveOK = true;
  uint32_t started = 0, waitUs = 0, goUs = 0;

  // 一局结束时同时保存两项数据。中途返回不计入总局数。
  void finish(State outcome) {
    state = outcome;
    lights = 0;
    lastOutcome = outcome;
    lastUs = outcome == State::Result ? resultUs : 0;
    if (stats.total != UINT32_MAX) ++stats.total;
    if (outcome == State::Result && resultUs > 0 && (!stats.bestUs || resultUs < stats.bestUs)) {
      stats.bestUs = resultUs;
    }
    lastSaveOK = storageReady && storage.putBytes("stats", &stats, sizeof(stats)) == sizeof(stats);
    if (storageReady) {
      const bool lastTimeSaved = storage.putUInt("last-us", lastUs) == sizeof(lastUs);
      const bool lastStateSaved = storage.putUChar("last-state", static_cast<uint8_t>(outcome)) == 1;
      lastSaveOK = lastSaveOK && lastTimeSaved && lastStateSaved;
    }
  }
};

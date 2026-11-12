#include "Interface.h"
#include "NunitoFonts.h"
#include <new>

namespace {
constexpr int fireworkX = 40, fireworkY = 66, fireworkW = 240, fireworkH = 82;
String duration(uint32_t us) {
  return us ? String(us / 1000.0f, 1) + " ms" : String("--");
}
}

void Interface::game(const Game& game) {
  using State = Game::State;
  if (fireworks && game.state != State::Result) stopFireworks();
  if (lastLights != game.lights) {
    for (int i = 0; i < 5; ++i) {
      const int x = 42 + i * 48;
      if (lastLights < 0) {
        screen.fillRoundRect(x, 68, 44, 78, 9, 0x2104);
        screen.drawRoundRect(x, 68, 44, 78, 9, 0x39E7);
        screen.fillCircle(x + 22, 88, 13, 0x4A49);
      }
      if (lastLights < 0 || (i < game.lights) != (i < lastLights)) {
        screen.fillCircle(x + 22, 126, 13, i < game.lights ? ILI9341_RED : 0x4A49);
      }
    }
    lastLights = game.lights;
  }
  String message;
  String corrected;
  String hint;
  switch (game.state) {
    case State::Idle: message = "Press to start"; break;
    case State::Lighting:
    case State::Waiting: message = "Wait for lights out"; break;
    case State::GoPending:
    case State::Go: message = game.debug && game.state == State::Go ?
        "RAW " + duration(game.liveUs()) : String("GO!"); break;
    case State::Result:
      message = game.debug ? "RAW " + duration(game.resultUs) :
                             String(game.correctedMs(game.resultUs), 1) + " ms";
      if (game.debug) corrected = "CAL " + String(game.correctedMs(game.resultUs), 1) + " ms";
      hint = "Press PLAY to reset";
      break;
    case State::FalseStart: message = "FALSE START"; hint = "Press PLAY to reset"; break;
    case State::Timeout: message = "TIMEOUT"; hint = "Press PLAY to reset"; break;
  }
  field(0, 38, 150, 244, 31, message, 1,
        game.state == State::FalseStart ? ILI9341_RED : ILI9341_WHITE, true);
  gameMode(game.debug);
  field(2, 38, 183, 244, 22, corrected, 0, highlight, true);
  field(3, 38, 207, 244, 19, hint, 4, muted, true);
  field(4, 38, 226, 244, 14, game.saved() ? "" : "Flash save unavailable", 3, muted, true);
}

void Interface::startFireworks() {
  fireworks.reset(new (std::nothrow) GFXcanvas16(fireworkW, fireworkH));
  if (!fireworks || !fireworks->getBuffer()) {
    fireworks.reset();
    return;
  }
  fireworkStarted = millis();
  lastFireworkFrame = fireworkStarted - 40;
}

void Interface::stopFireworks() {
  fireworks.reset();
  screen.fillRect(fireworkX, fireworkY, fireworkW, fireworkH, background);
  lastLights = -1;
}

void Interface::updateFireworks(const Game& game) {
  if (!fireworks) return;
  const uint32_t now = millis();
  const uint32_t elapsed = now - fireworkStarted;
  if (elapsed >= 1350) {
    stopFireworks();
    this->game(game);
    return;
  }
  if (now - lastFireworkFrame < 40) return;
  lastFireworkFrame = now;

  // 只合成灯箱区域。三个烟花错峰展开，粒子减速并向下坠落。
  auto& frame = *fireworks;
  frame.fillScreen(background);
  constexpr int centers[] = {47, 120, 193};
  constexpr uint16_t colors[] = {ILI9341_YELLOW, 0xF94E, 0x65FF};
  for (int burst = 0; burst < 3; ++burst) {
    const int age = static_cast<int>(elapsed) - burst * 165;
    if (age < 0 || age > 850) continue;
    const float t = age / 850.0f;
    const float radius = 37.0f * t * (2.0f - t);
    for (int ray = 0; ray < 12; ++ray) {
      const float angle = ray * 0.523599f + burst * 0.24f;
      const float dx = cosf(angle), dy = sinf(angle);
      const int x = centers[burst] + lroundf(dx * radius);
      const int y = 33 + lroundf(dy * radius + 13 * t * t);
      frame.drawLine(x, y, x - lroundf(dx * 5), y - lroundf(dy * 5), colors[burst]);
      if (t < 0.62f) frame.fillCircle(x, y, 2, ILI9341_WHITE);
    }
  }
  if (elapsed >= 490) {
    frame.setFont(&NunitoSmall);
    frame.setTextColor(ILI9341_YELLOW);
    frame.setCursor(76, 76);
    frame.print("NEW BEST");
  }
  screen.drawRGBBitmap(fireworkX, fireworkY, frame.getBuffer(), fireworkW, fireworkH);
}

void Interface::gameMode(bool debug) {
  // 模式切换只改这块小字。入口单独处理右上标签的退场和入场。
  field(1, 223, 13, 61, 16, debug ? "DEBUG" : "NORMAL", 3,
        debug ? ILI9341_YELLOW : muted, true);
}

void Interface::stats(const Game& game, bool confirmClear) {
  using State = Game::State;
  const String last = game.lastOutcome == State::FalseStart ? String("FALSE START") :
                      game.lastOutcome == State::Timeout ? String("TIMEOUT") : duration(game.lastUs);
  const char* labels[] = {"LAST", "BEST", "ROUNDS"};
  const String values[] = {last, duration(game.best()), String(game.total())};
  for (int i = 0; i < 3; ++i) {
    const int y = 52 + i * 56;
    screen.drawRoundRect(42, y, 236, 46, 8, 0x39E7);
    field(i * 2, 51, y + 4, 86, 38, labels[i], 0, highlight);
    field(i * 2 + 1, 142, y + 4, 127, 38, values[i], 0, ILI9341_WHITE);
  }
  field(6, 38, 213, 244, 24, confirmClear ? "Press ZERO again to clear" :
        !game.saved() ? "Flash save unavailable" : "Times shown are raw", 3,
        confirmClear ? highlight : muted, true);
}

void Interface::calibration(const Game& game, bool editOffset, bool saveFailed) {
  const char* labels[] = {"Multiplier", "Offset (ms)"};
  const String values[] = {String(game.factor(), 2), String(game.offsetMs())};
  for (int i = 0; i < 2; ++i) {
    const int y = 58 + i * 53;
    const bool active = i == (editOffset ? 1 : 0);
    screen.drawRoundRect(42, y, 236, 44, 8, active ? highlight : 0x39E7);
    field(i * 2, 51, y + 4, 123, 36, labels[i], 0, ILI9341_WHITE);
    field(i * 2 + 1, 180, y + 4, 88, 36, values[i], 1, highlight, true);
  }
  field(4, 38, 171, 244, 20, "CAL = RAW x factor + offset", 4, muted, true);
  field(6, 38, 191, 244, 20, "FIELD selects  /  + or - adjusts", 4, muted, true);
  field(5, 38, 206, 244, 24, saveFailed ? "Save failed; press SAVE to retry" :
        "Hold FIELD to reset defaults", 4, saveFailed ? highlight : muted, true);
}

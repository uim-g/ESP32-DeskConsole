#include "F1Pages.h"

void F1Page::showResultLed() {
  using State = Game::State;
  using Cue = StatusLed::Cue;
  switch (game.state) {
    case State::Result: ctx.led.play(game.newRecord ? Cue::Record : Cue::Result); break;
    case State::FalseStart: ctx.led.play(Cue::FalseStart); break;
    case State::Timeout: ctx.led.play(Cue::Timeout); break;
    default: ctx.led.stop(); break;
  }
}

void F1Page::input(Key key, uint32_t us) {
  if (key == Key::Back) {
    game.reset();
    ctx.nav.open(View::Launcher);
  } else if (key == Key::Down) {
    game.press(us);
    draw();
    if (game.newRecord) ctx.ui.startFireworks();
    showResultLed();
  } else if (game.canChangeMode() && key == Key::Action) {
    game.debug = !game.debug;
    Serial.printf("[F1] Mode: %s\n", game.debug ? "DEBUG" : "NORMAL");
    ctx.ui.gameMode(game.debug);
    ctx.ui.replaceTab(2, game.debug ? "SET" : "STATS");
  } else if (game.canChangeMode() && key == Key::Up) {
    ctx.nav.open(game.debug ? View::Calibration : View::Stats);
  }
}

// 推进五灯倒计时与超时，灯熄灭前按住起跑键会判抢跑。先提交熄灯画面再记录起跑时刻。
void F1Page::tick(uint32_t now) {
  const uint32_t us = micros();
  if (game.tick(us)) {
    if (game.state == Game::State::GoPending && ctx.buttons.playHeld()) {
      ctx.buttons.consumeHeldPlay();
      game.press(us);
    }
    draw();
    // 先提交熄灯帧，随后才开始反应计时。
    if (game.state == Game::State::GoPending) {
      Platform::presentDisplay();
      game.presented(micros());
    }
    if (game.canChangeMode()) showResultLed();
  } else if (game.debug && game.state == Game::State::Go && now - refreshed >= 100) {
    refreshed = now;
    draw();
  }
  ctx.ui.updateFireworks(game);
}

// 清空记录需要两次确认，普通返回不修改存档。
void StatsPage::input(Key key, uint32_t) {
  if (key == Key::Back) ctx.nav.open(View::F1);
  else if (key == Key::Action) {
    if (confirmClear) {
      game.clearStats();
      if (game.saved()) ctx.led.play(StatusLed::Cue::Saved);
    } else Serial.println("[F1] Press ZERO again to confirm");
    confirmClear = !confirmClear;
    draw();
  }
}

void CalibrationPage::enter() {
  editOffset = dirty = saveFailed = false;
  wake();
}

void CalibrationPage::wake() {
  repeatKey = Key::None;
  fieldPending = fieldReset = false;
}

void CalibrationPage::adjust(Key key) {
  game.adjustCalibration(editOffset, key == Key::Up ? 1 : -1);
  dirty = true;
  saveFailed = false;
  Serial.printf("[CAL] %s: factor=%.2f offset=%+d ms\n",
                editOffset ? "Offset" : "Multiplier", game.factor(), game.offsetMs());
}

void CalibrationPage::input(Key key, uint32_t) {
  if (key == Key::Back) {
    if (dirty) {
      if (!game.saveCalibration()) {
        Serial.println("[CAL] Flash save failed");
        saveFailed = true;
        draw();
        return;
      }
      Serial.println("[CAL] Saved to flash");
      ctx.led.play(StatusLed::Cue::Saved);
    }
    ctx.nav.open(View::F1);
  } else if (key == Key::Action) {
    repeatKey = Key::None;
    fieldPressedAt = millis();
    fieldPending = true;
    fieldReset = false;
  } else if (vertical(key) && !fieldPending) {
    adjust(key);
    repeatKey = key;
    repeatAt = millis() + 420;
    draw();
  }
}

// FIELD 短按切换编辑项，长按恢复默认；方向键可按住重复调整，退出前由保存动作写入。
void CalibrationPage::tick(uint32_t now) {
  if (fieldPending) {
    if (!ctx.buttons.held(Key::Action)) {
      if (!fieldReset) {
        editOffset = !editOffset;
        Serial.printf("[CAL] Editing: %s\n", editOffset ? "Offset" : "Multiplier");
      }
      fieldPending = false;
      draw();
    } else if (!fieldReset && now - fieldPressedAt >= 800) {
      game.resetCalibration();
      Serial.println("[CAL] Defaults restored: factor=1.00 offset=0 ms; press SAVE to persist");
      dirty = fieldReset = true;
      saveFailed = false;
      draw();
    }
  }
  if (repeatKey != Key::None) {
    if (!ctx.buttons.held(repeatKey)) repeatKey = Key::None;
    else if (due(now, repeatAt)) {
      adjust(repeatKey);
      repeatAt = now + 85;
      draw();
    }
  }
}

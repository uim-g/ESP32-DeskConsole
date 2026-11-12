#pragma once
#include "Page.h"

class F1Page : public Page {
 public:
  F1Page(PageContext& context, Game& game) : Page(context), game(game) {}
  PageStyle style() const override {
    return {"F1 REACTION",ILI9341_BLACK,0xFA69,{"BACK","MODE",game.debug ? "SET" : "STATS","PLAY"}};
  }
  void enter() override { refreshed = millis(); }
  void leave() override { ctx.led.stop(); }
  void draw() override { ctx.ui.game(game); }
  void input(Key key, uint32_t us) override;
  void tick(uint32_t now) override;
  bool canSleep() const override { return game.canChangeMode(); }
 private:
  void showResultLed();
  Game& game;
  uint32_t refreshed = 0;
};

class StatsPage : public RefreshPage {
 public:
  StatsPage(PageContext& context, Game& game) : RefreshPage(context), game(game) {}
  PageStyle style() const override { return {"F1 Statistics",ILI9341_BLACK,0xFB0C,{"BACK","ZERO"}}; }
  void enter() override { RefreshPage::enter(); confirmClear = false; }
  void draw() override { ctx.ui.stats(game, confirmClear); }
  void input(Key key, uint32_t us) override;
 private:
  Game& game;
  bool confirmClear = false;
};

class CalibrationPage : public Page {
 public:
  CalibrationPage(PageContext& context, Game& game) : Page(context), game(game) {}
  PageStyle style() const override { return {"F1 Calibration",ILI9341_BLACK,0xFB0C,{"SAVE","FIELD","+","-"}}; }
  void enter() override;
  void draw() override { ctx.ui.calibration(game, editOffset, saveFailed); }
  void input(Key key, uint32_t us) override;
  void tick(uint32_t now) override;
  void wake() override;
 private:
  void adjust(Key key);
  Game& game;
  Key repeatKey = Key::None;
  uint32_t repeatAt = 0, fieldPressedAt = 0;
  bool editOffset = false, dirty = false, saveFailed = false;
  bool fieldPending = false, fieldReset = false;
};

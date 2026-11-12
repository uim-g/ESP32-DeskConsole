#pragma once
#include "Page.h"

class SettingsPage : public Page {
 public:
  SettingsPage(PageContext& context, SettingsStore& store) : Page(context), store(store) {}
  PageStyle style() const override { return {"Settings",0x10A4,0x97D4,{"BACK","EDIT","^","v"}}; }
  void enter() override { row = 0; editing = saveFailed = false; }
  void draw() override { ctx.ui.settings(editing ? draft : store.current, row, editing, saveFailed); }
  void input(Key key, uint32_t us) override;
  bool repeats() const override { return true; }

 private:
  bool save(const Settings& next);
  SettingsStore& store;
  Settings draft;
  uint8_t row = 0;
  bool editing = false, saveFailed = false;
};

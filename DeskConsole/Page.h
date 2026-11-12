#pragma once
#include "Buttons.h"
#include "Interface.h"
#include "StatusLed.h"

enum class View { Home, Launcher, F1, Weather, Network, Bluetooth, Device, Settings, Stats, Calibration };
struct PageStyle {
  const char* title;
  uint16_t background;
  uint16_t accent;
  const char* tabs[4];
};

class Navigation {
 public:
  virtual void open(View next) = 0;
};

struct PageContext {
  Navigation& nav;
  Interface& ui;
  Buttons& buttons;
  StatusLed& led;
};
using Key = Buttons::Key;
inline bool vertical(Key key) { return key == Key::Up || key == Key::Down; }
inline bool due(uint32_t now, uint32_t at) { return static_cast<int32_t>(now - at) >= 0; }

class Page {
 public:
  explicit Page(PageContext& context) : ctx(context) {}
  virtual PageStyle style() const = 0;
  virtual void enter() {}
  virtual void leave() {}
  virtual void draw() = 0;
  virtual void input(Key key, uint32_t pressedUs) {
    if (key == Key::Back) ctx.nav.open(View::Launcher);
  }
  virtual void tick(uint32_t now) {}
  virtual void wake() {}
  virtual bool repeats() const { return false; }
  virtual bool canSleep() const { return true; }

 protected:
  PageContext& ctx;
};

class RefreshPage : public Page {
 public:
  using Page::Page;
  void enter() override { refreshed = millis(); }
  void wake() override { refreshed = millis(); }
  void tick(uint32_t now) override {
    if (now - refreshed >= 1000) { refreshed = now; draw(); }
  }
 private:
  uint32_t refreshed = 0;
};

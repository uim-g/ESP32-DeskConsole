#pragma once
#include "Page.h"
#include "WifiNetworks.h"

class NetworkPage : public Page {
 public:
  NetworkPage(PageContext& context, WifiNetworks& service) : Page(context), wifi(service) {}
  PageStyle style() const override;
  void enter() override;
  void leave() override { pendingAction = false; }
  void wake() override { pendingAction = false; }
  void draw() override;
  void tick(uint32_t now) override;
  void input(Key key, uint32_t us) override;
  bool repeats() const override;
  bool canSleep() const override { return !pendingAction; }

 private:
  enum class Screen { Overview, Nearby, Saved, Password, Forget, Connecting, Result };
  void show(Screen next);
  void back();
  void move(Key key);
  void select();
  void hold();
  void openPassword(Screen origin);
  void join(const char* password);
  void pick(const char* ssid, NearbyWifi::Security security, Screen origin);
  void drawList(bool saved);
  void drawPassword();
  void passwordAction();
  void erasePassword();
  static String label(const char* name);

  WifiNetworks& wifi;
  Screen screen = Screen::Overview, origin = Screen::Nearby;
  bool internal = false, pendingAction = false, visiblePassword = false, passwordRequired = false;
  uint32_t pressedAt = 0, refreshed = 0;
  int selected = 0, group = 0, item = 1;
  WifiNetworks::Scan lastScan = WifiNetworks::Scan::Idle;
  char ssid[33] = {}, password[65] = {};
  const char* hint = "";
};

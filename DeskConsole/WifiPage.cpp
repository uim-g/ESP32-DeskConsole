#include "WifiPage.h"
#include <time.h>

namespace {
constexpr uint32_t holdMs = 800;
constexpr int rowsPerPage = 3;
constexpr const char* alphabets[] = {
  "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "0123456789",
  "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
};
constexpr const char* groups[] = {"abc", "ABC", "123", "#+="};
constexpr const char* commands[] = {"Space", "Delete", "Show", "Connect"};

String lastUsed(uint32_t stamp) {
  if (!stamp) return "Saved";
  const time_t time = stamp;
  tm local;
  char date[22] = {};
  if (!localtime_r(&time, &local)) return "Saved";
  strftime(date, sizeof(date), "Last %d %b %H:%M", &local);
  return date;
}

bool validPassword(const char* password) {
  const size_t size = strlen(password);
  if (size >= 8 && size <= 63) return true;
  if (size != 64) return false;
  for (size_t i = 0; i < size; ++i) {
    const char c = password[i];
    if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F')) return false;
  }
  return true;
}
}

PageStyle NetworkPage::style() const {
  const char* title = "Wi-Fi";
  const char* action = "OPEN";
  switch (screen) {
    case Screen::Overview: break;
    case Screen::Nearby: title = "Nearby Wi-Fi"; action = "SELECT"; break;
    case Screen::Saved: title = "Saved Wi-Fi"; action = "JOIN"; break;
    case Screen::Password: title = "Join Wi-Fi"; action = "SELECT"; break;
    case Screen::Forget: title = "Forget Wi-Fi"; action = "FORGET"; break;
    case Screen::Connecting: title = "Connecting"; action = nullptr; break;
    case Screen::Result: title = "Wi-Fi result"; action = "OK"; break;
  }
  const bool arrows = repeats();
  return {title, 0x0842, Interface::accent, {"BACK", action, arrows ? "^" : nullptr, arrows ? "v" : nullptr}};
}

void NetworkPage::enter() {
  if (!internal) {
    screen = Screen::Overview;
    selected = 0;
    hint = "";
    erasePassword();
  }
  internal = false;
  refreshed = millis();
}

void NetworkPage::show(Screen next) {
  screen = next;
  hint = "";
  internal = true;
  ctx.nav.open(View::Network);
}

bool NetworkPage::repeats() const {
  return screen == Screen::Overview || screen == Screen::Nearby ||
         screen == Screen::Saved || screen == Screen::Password;
}

void NetworkPage::erasePassword() {
  memset(password, 0, sizeof(password));
  visiblePassword = false;
}

String NetworkPage::label(const char* name) {
  // The display font covers ASCII. Keep the original SSID bytes for the radio.
  char printable[33];
  size_t i = 0;
  for (; name[i] && i < sizeof(printable) - 1; ++i) {
    const unsigned char c = name[i];
    printable[i] = c >= 32 && c <= 126 ? c : '?';
  }
  printable[i] = 0;
  return String(printable);
}

void NetworkPage::drawList(bool saved) {
  if (!saved && wifi.scanState() == WifiNetworks::Scan::Running) {
    ctx.ui.wifiMessage("Scanning nearby networks...", "", "Searching 2.4 GHz");
    return;
  }
  const int count = saved ? wifi.savedCount() : wifi.nearbyCount();
  if (!count) {
    if (saved) ctx.ui.wifiMessage("No saved networks", "", *hint ? hint : "Scan nearby to add one");
    else if (wifi.scanState() == WifiNetworks::Scan::Failed)
      ctx.ui.wifiMessage("Scan failed", "", "Hold SELECT to retry");
    else ctx.ui.wifiMessage("No Wi-Fi networks found", "", *hint ? hint : "Hold SELECT to rescan");
    return;
  }

  const auto network = Platform::network();
  const int page = selected / rowsPerPage;
  const int first = page * rowsPerPage;
  Interface::WifiRow rows[rowsPerPage];
  for (int row = 0; row < rowsPerPage && first + row < count; ++row) {
    const int index = first + row;
    if (saved) {
      const auto& entry = wifi.saved(index);
      rows[row] = {label(entry.ssid), network.connected && network.ssid == entry.ssid ?
                                    String("Connected now") : lastUsed(entry.lastConnected)};
    } else {
      const auto& ap = wifi.nearby(index);
      const bool stored = ap.security != NearbyWifi::Security::Unsupported &&
          wifi.find(ap.ssid, ap.security == NearbyWifi::Security::Personal) >= 0;
      rows[row] = {label(ap.ssid), String(ap.signal) + " dBm / Ch " + ap.channel + " / " +
                                    ap.authentication + (stored ? " / Saved" : "")};
    }
  }
  const String footer = *hint ? String(hint) : String(page + 1) + "/" +
      ((count + rowsPerPage - 1) / rowsPerPage) + (saved ? "  Hold JOIN to forget" : "  Hold SELECT to rescan");
  ctx.ui.wifiList(rows, min(rowsPerPage, count - first), selected % rowsPerPage, footer);
}

void NetworkPage::drawPassword() {
  String masked;
  const size_t length = strlen(password);
  if (visiblePassword) masked = password;
  else for (size_t i = 0; i < min(length, size_t(26)); ++i) masked = masked + '*';
  if (length > 26 && !visiblePassword) masked = masked + "...";

  const int letters = strlen(alphabets[group]);
  const int count = 1 + letters + 4;
  auto token = [&](int index) -> String {
    if (index == 0) return "Group";
    if (index <= letters) return String(alphabets[group][index - 1]);
    if (index == letters + 3 && visiblePassword) return "Hide";
    return commands[index - letters - 1];
  };
  ctx.ui.wifiPassword(label(ssid), masked, length, groups[group],
                      token((item + count - 1) % count), token(item), token((item + 1) % count),
                      *hint ? hint : "UP / DOWN browse  /  SELECT");
}

void NetworkPage::draw() {
  switch (screen) {
    case Screen::Overview: {
      const auto data = Platform::network();
      Interface::WifiRow rows[] = {
        {data.connected ? String("Connected: ") + label(data.ssid.c_str()) : String("Disconnected"),
         data.connected ? data.address + " / " + data.signal + " dBm" :
                          String("Choose a saved or nearby network")},
        {"Scan nearby", "Find 2.4 GHz networks"},
        {"Saved networks", String(wifi.savedCount()) + " saved"}
      };
      ctx.ui.wifiList(rows, 3, selected + 1, *hint ? hint : "UP / DOWN choose  /  OPEN");
      break;
    }
    case Screen::Nearby: drawList(false); break;
    case Screen::Saved: drawList(true); break;
    case Screen::Password: drawPassword(); break;
    case Screen::Forget:
      ctx.ui.wifiMessage("Remove saved network?", label(ssid), *hint ? hint : "FORGET confirms  /  BACK cancels");
      break;
    case Screen::Connecting:
      ctx.ui.wifiMessage("Joining network...", label(ssid), "BACK cancels");
      break;
    case Screen::Result: {
      const auto state = wifi.joinState();
      ctx.ui.wifiMessage(state == WifiNetworks::Join::Connected ? "Connected" :
                         state == WifiNetworks::Join::SaveFailed ? "Connected, not saved" : "Could not connect",
                         label(ssid),
                         state == WifiNetworks::Join::Failed && passwordRequired ? "OK enters password again" :
                         state == WifiNetworks::Join::Failed ? "OK retries  /  BACK returns" :
                         state == WifiNetworks::Join::SaveFailed ? "Check storage or forget an entry" : "Network saved");
      break;
    }
  }
}

void NetworkPage::openPassword(Screen source) {
  origin = source;
  erasePassword();
  group = 0;
  item = 1;
  show(Screen::Password);
}

void NetworkPage::join(const char* secret) {
  if (!wifi.startJoin(ssid, secret)) {
    hint = "Wi-Fi busy. Try again.";
    draw();
    return;
  }
  erasePassword();
  show(Screen::Connecting);
}

void NetworkPage::pick(const char* name, NearbyWifi::Security security, Screen source) {
  if (security == NearbyWifi::Security::Unsupported) {
    hint = "This security type is unsupported";
    draw();
    return;
  }
  origin = source;
  passwordRequired = security == NearbyWifi::Security::Personal;
  snprintf(ssid, sizeof(ssid), "%s", name);
  const int saved = wifi.find(ssid, passwordRequired);
  if (!passwordRequired) join("");
  else if (saved >= 0 && wifi.saved(saved).password[0]) join(wifi.saved(saved).password);
  else openPassword(source);
}

void NetworkPage::back() {
  switch (screen) {
    case Screen::Overview: ctx.nav.open(View::Launcher); break;
    case Screen::Nearby:
    case Screen::Saved: selected = 0; show(Screen::Overview); break;
    case Screen::Connecting: wifi.cancelJoin(); show(origin); break;
    case Screen::Result: selected = 0; show(Screen::Overview); break;
    case Screen::Password: erasePassword(); show(origin); break;
    case Screen::Forget: show(Screen::Saved); break;
  }
}

void NetworkPage::passwordAction() {
  const int letters = strlen(alphabets[group]);
  size_t length = strlen(password);
  hint = "";
  if (item == 0) group = (group + 1) % 4;
  else if (item <= letters || item == letters + 1) {
    if (length == sizeof(password) - 1) hint = "Password field is full";
    else password[length] = item <= letters ? alphabets[group][item - 1] : ' ';
  } else if (item == letters + 2) {
    if (length) password[length - 1] = 0;
  } else if (item == letters + 3) visiblePassword = !visiblePassword;
  else if (!validPassword(password)) hint = "Use 8-63 characters or 64 hex digits";
  else { join(password); return; }
  draw();
}

void NetworkPage::select() {
  switch (screen) {
    case Screen::Overview:
      if (selected == 0) {
        if (wifi.busy()) { hint = "Wi-Fi busy. Try again."; draw(); break; }
        wifi.startScan();
        lastScan = wifi.scanState();
        selected = 0;
        show(Screen::Nearby);
      } else {
        selected = 0;
        show(Screen::Saved);
      }
      break;
    case Screen::Nearby:
      if (selected < wifi.nearbyCount() && wifi.scanState() == WifiNetworks::Scan::Ready) {
        const auto& ap = wifi.nearby(selected);
        pick(ap.ssid, ap.security, Screen::Nearby);
      }
      break;
    case Screen::Saved:
      if (selected < wifi.savedCount()) {
        const auto& entry = wifi.saved(selected);
        pick(entry.ssid, entry.password[0] ? NearbyWifi::Security::Personal : NearbyWifi::Security::Open,
             Screen::Saved);
      }
      break;
    case Screen::Password: passwordAction(); break;
    case Screen::Forget: {
      const int index = wifi.find(ssid, passwordRequired);
      if (!wifi.forget(index)) { hint = "Could not remove saved network"; draw(); break; }
      selected = min(selected, max(0, wifi.savedCount() - 1));
      show(Screen::Saved);
      break;
    }
    case Screen::Connecting: break;
    case Screen::Result:
      if (wifi.joinState() == WifiNetworks::Join::Failed && passwordRequired) openPassword(origin);
      else if (wifi.joinState() == WifiNetworks::Join::Failed) join("");
      else { selected = 0; show(Screen::Overview); }
      break;
  }
}

void NetworkPage::hold() {
  if (screen == Screen::Nearby) {
    if (wifi.startScan()) {
      selected = 0;
      lastScan = wifi.scanState();
      show(Screen::Nearby);
    }
  } else if (screen == Screen::Saved && selected < wifi.savedCount()) {
    const auto& entry = wifi.saved(selected);
    passwordRequired = entry.password[0] != 0;
    snprintf(ssid, sizeof(ssid), "%s", entry.ssid);
    show(Screen::Forget);
  }
}

void NetworkPage::move(Key key) {
  const int step = key == Key::Up ? -1 : 1;
  hint = "";
  if (screen == Screen::Password) {
    const int count = strlen(alphabets[group]) + 5;
    item = (item + step + count) % count;
  } else {
    const int count = screen == Screen::Overview ? 2 :
                      screen == Screen::Saved ? wifi.savedCount() : wifi.nearbyCount();
    if (!count) return;
    selected = (selected + step + count) % count;
  }
  draw();
}

void NetworkPage::input(Key key, uint32_t) {
  if (key == Key::Back) { pendingAction = false; back(); }
  else if (key == Key::Action) {
    if (screen == Screen::Nearby || screen == Screen::Saved) {
      pendingAction = true;
      pressedAt = millis();
    } else select();
  } else if (vertical(key) && repeats() && !pendingAction) move(key);
}

void NetworkPage::tick(uint32_t now) {
  if (pendingAction) {
    const bool held = ctx.buttons.held(Key::Action);
    if (held && now - pressedAt >= holdMs) { pendingAction = false; hold(); return; }
    if (!held) { pendingAction = false; select(); return; }
  }
  if (screen == Screen::Connecting && wifi.joinState() != WifiNetworks::Join::Running) {
    show(Screen::Result);
  } else if (screen == Screen::Nearby && lastScan != wifi.scanState()) {
    lastScan = wifi.scanState();
    selected = 0;
    show(Screen::Nearby);
  } else if (screen == Screen::Overview && now - refreshed >= 1000) {
    refreshed = now;
    draw();
  }
}

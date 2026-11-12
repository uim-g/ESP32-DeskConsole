#include "WifiNetworks.h"
#include "Config.h"
#include <WiFi.h>
#include <time.h>

namespace {
using Security = NearbyWifi::Security;
Security securityOf(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return Security::Open;
    case WIFI_AUTH_WPA_PSK:
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK: return Security::Personal;
    default: return Security::Unsupported;
  }
}
const char* authName(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return "Open";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "Unsupported";
  }
}
}

void WifiNetworks::begin() {
  WiFi.persistent(false);
  storageReady = storage.begin("desk-wifi", false);
  if (storageReady) radioEnabled = storage.getUChar("enabled", 1) != 0;
  if (!storageReady || storage.getBytesLength("profiles") != sizeof(data)) return;
  SavedData loaded;
  if (storage.getBytes("profiles", &loaded, sizeof(loaded)) != sizeof(loaded) ||
      loaded.version != data.version || loaded.count > maxSaved) return;
  for (int i = 0; i < loaded.count; ++i) {
    const auto& entry = loaded.profiles[i];
    if (!entry.ssid[0] || !memchr(entry.ssid, 0, sizeof(entry.ssid)) ||
        !memchr(entry.password, 0, sizeof(entry.password))) return;
  }
  data = loaded;
  hasSavedRecord = true;
  Serial.printf("[WIFI] Loaded %u saved networks\n", static_cast<unsigned>(data.count));
}

void WifiNetworks::connectPreferred() {
  if (!radioEnabled) return;
  WiFi.mode(WIFI_STA);
  if (data.count) startJoin(data.profiles[0].ssid, data.profiles[0].password);
  else if (!hasSavedRecord && strcmp(Config::ssid, "YOUR_WIFI_NAME") != 0 && Config::ssid[0]) {
    startJoin(Config::ssid, Config::password);
  }
}

bool WifiNetworks::setEnabled(bool enabled) {
  if (enabled == radioEnabled) return true;
  if (!storageReady || !WiFi.mode(enabled ? WIFI_STA : WIFI_OFF)) return false;
  if (storage.putUChar("enabled", enabled) != 1) {
    WiFi.mode(radioEnabled ? WIFI_STA : WIFI_OFF);
    if (radioEnabled) {
      cancelJoin();
      WiFi.scanDelete();
      scanning = Scan::Idle;
      connectPreferred();
    }
    return false;
  }
  radioEnabled = enabled;
  if (enabled) {
    connectPreferred();
  } else {
    WiFi.scanDelete();
    scanning = Scan::Idle;
    joining = Join::Idle;
    disconnecting = false;
    found = 0;
    target = {};
  }
  return true;
}

int WifiNetworks::find(const char* ssid, bool secured) const {
  for (int i = 0; i < data.count; ++i) {
    const auto& entry = data.profiles[i];
    if ((entry.password[0] != 0) == secured && strcmp(entry.ssid, ssid) == 0) return i;
  }
  return -1;
}

bool WifiNetworks::save(const SavedData& next) {
  if (!storageReady || storage.putBytes("profiles", &next, sizeof(next)) != sizeof(next)) return false;
  data = next;
  hasSavedRecord = true;
  return true;
}

bool WifiNetworks::remember() {
  const int previous = find(target.ssid, target.password[0] != 0);
  const auto now = time(nullptr);
  target.lastConnected = now >= 1577836800 ? static_cast<uint32_t>(now) :
      previous >= 0 ? data.profiles[previous].lastConnected : 0;
  if (previous == 0 && strcmp(target.password, data.profiles[0].password) == 0 &&
      target.lastConnected == data.profiles[0].lastConnected) return true;
  // 更新已有记录时保留容量，新网络满额时提示保存失败。
  if (previous < 0 && data.count == maxSaved) return false;
  SavedData next = data;
  const int end = previous >= 0 ? previous : next.count++;
  for (int i = end; i > 0; --i) next.profiles[i] = next.profiles[i - 1];
  next.profiles[0] = target;
  return save(next);
}

bool WifiNetworks::forget(int index) {
  if (index < 0 || index >= data.count) return false;
  SavedData next = data;
  for (int i = index; i + 1 < next.count; ++i) next.profiles[i] = next.profiles[i + 1];
  next.profiles[--next.count] = {};
  const bool current = WiFi.status() == WL_CONNECTED && WiFi.SSID() == data.profiles[index].ssid;
  if (!save(next)) return false;
  if (current) WiFi.setAutoReconnect(false);
  return true;
}

bool WifiNetworks::startJoin(const char* ssid, const char* password) {
  if (!radioEnabled || busy() || !*ssid || strlen(ssid) >= sizeof(target.ssid) ||
      strlen(password) >= sizeof(target.password)) return false;
  target = {};
  snprintf(target.ssid, sizeof(target.ssid), "%s", ssid);
  snprintf(target.password, sizeof(target.password), "%s", password);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  joining = Join::Running;
  joinStarted = millis();
  disconnecting = WiFi.status() == WL_CONNECTED;
  if (disconnecting) WiFi.disconnect(false, false, 0);
  else beginJoin();
  return true;
}

void WifiNetworks::beginJoin() {
  disconnecting = false;
  WiFi.setMinSecurity(target.password[0] ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN);
  Serial.println("[WIFI] Starting connection attempt");
  WiFi.begin(target.ssid, target.password[0] ? target.password : nullptr);
}

void WifiNetworks::cancelJoin() {
  if (joining != Join::Running) return;
  WiFi.disconnect(false, false, 0);
  target = {};
  disconnecting = false;
  joining = Join::Idle;
}

bool WifiNetworks::startScan() {
  if (!radioEnabled || busy()) return false;
  found = 0;
  WiFi.scanDelete();
  const int result = WiFi.scanNetworks(true);
  if (result == WIFI_SCAN_RUNNING) scanning = Scan::Running;
  else finishScan(result);
  return scanning != Scan::Failed;
}

void WifiNetworks::finishScan(int count) {
  for (int i = 0; i < count && i < 256; ++i) {
    const auto ssid = WiFi.SSID(i);
    if (!ssid.length() || ssid.length() >= sizeof(NearbyWifi::ssid)) continue;
    const auto auth = WiFi.encryptionType(i);
    NearbyWifi candidate;
    snprintf(candidate.ssid, sizeof(candidate.ssid), "%s", ssid.c_str());
    candidate.signal = WiFi.RSSI(i);
    candidate.channel = WiFi.channel(i);
    candidate.security = securityOf(auth);
    candidate.authentication = authName(auth);
    int duplicate = -1;
    for (int j = 0; j < found; ++j) {
      if (candidate.security == results[j].security && strcmp(results[j].ssid, candidate.ssid) == 0) {
        duplicate = j;
        break;
      }
    }
    if (duplicate >= 0) {
      if (results[duplicate].signal >= candidate.signal) continue;
      for (int j = duplicate; j + 1 < found; ++j) results[j] = results[j + 1];
      --found;
    }
    int pos = 0;
    while (pos < found && results[pos].signal >= candidate.signal) ++pos;
    if (pos >= maxNearby) continue;
    if (found < maxNearby) ++found;
    for (int j = found - 1; j > pos; --j) results[j] = results[j - 1];
    results[pos] = candidate;
  }
  WiFi.scanDelete();
  scanning = count < 0 ? Scan::Failed : Scan::Ready;
}

void WifiNetworks::tick(uint32_t now) {
  if (scanning == Scan::Running) {
    const int result = WiFi.scanComplete();
    if (result != WIFI_SCAN_RUNNING) finishScan(result);
  }
  if (joining != Join::Running) return;
  if (!disconnecting && WiFi.status() == WL_CONNECTED && strcmp(WiFi.SSID().c_str(), target.ssid) == 0) {
    joining = remember() ? Join::Connected : Join::SaveFailed;
    WiFi.setAutoReconnect(true);
    target = {};
    return;
  }
  if (now - joinStarted >= 12000) {
    WiFi.disconnect(false, false, 0);
    disconnecting = false;
    joining = Join::Failed;
    target = {};
  } else if (disconnecting && WiFi.status() != WL_CONNECTED) {
    beginJoin();
  }
}

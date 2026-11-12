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

// 读取独立保存的开关和网络记录，先校验结构版本、记录数量以及字符数组结束符。无线驱动自身的持久化被关闭，历史由本模块管理。
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

// 开机或唤醒优先尝试最近成功的一条记录。第一次使用时可以尝试 Config 的默认网络；关闭无线时直接返回。
void WifiNetworks::connectPreferred() {
  if (!radioEnabled) return;
  WiFi.mode(WIFI_STA);
  if (data.count) startJoin(data.profiles[0].ssid, data.profiles[0].password);
  else if (!hasSavedRecord && strcmp(Config::ssid, "YOUR_WIFI_NAME") != 0 && Config::ssid[0]) {
    startJoin(Config::ssid, Config::password);
  }
}

// 先让驱动切换无线模式，再保存开关。保存失败尝试恢复旧状态；成功关闭则清除本次扫描和连接状态。
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

// 只有 NVS 返回完整写入长度才替换内存里的记录，避免界面误报保存成功。
bool WifiNetworks::save(const SavedData& next) {
  if (!storageReady || storage.putBytes("profiles", &next, sizeof(next)) != sizeof(next)) return false;
  data = next;
  hasSavedRecord = true;
  return true;
}

// 只在确认连接成功后写入历史。已有记录移动到最前，新记录最多保存八条；日期尚未校时则保留已有时间。
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

// 移除目标记录并压紧数组。若删除的是当前连接，则关闭驱动自动重连，当前会话可以继续使用。
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

// 把目标凭据复制进本次连接缓存。若当前已联网先请求断开，随后由 tick 推进下一阶段。
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

// 启动异步扫描，由主循环检查结果；扫描和连接尝试共用无线资源，同一时刻只处理一项。
bool WifiNetworks::startScan() {
  if (!radioEnabled || busy()) return false;
  found = 0;
  WiFi.scanDelete();
  const int result = WiFi.scanNetworks(true);
  if (result == WIFI_SCAN_RUNNING) scanning = Scan::Running;
  else finishScan(result);
  return scanning != Scan::Failed;
}

// 按名称和安全类别去重，只保留信号较强的同类热点。结果按 RSSI 从强到弱排列，最多十六条。
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

// 处理扫描完成、等待旧连接断开、连接成功和超时。目标网络确认连接后才写入历史；超时约十二秒。
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

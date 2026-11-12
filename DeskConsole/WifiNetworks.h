#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct WifiProfile {
  char ssid[33] = {};
  char password[65] = {};
  uint32_t lastConnected = 0;
};

struct NearbyWifi {
  char ssid[33] = {};
  int32_t signal = 0;
  uint8_t channel = 0;
  enum class Security : uint8_t { Open, Personal, Unsupported };
  Security security = Security::Unsupported;
  const char* authentication = "";
};

class WifiNetworks {
 public:
  enum class Scan { Idle, Running, Ready, Failed };
  enum class Join { Idle, Running, Connected, SaveFailed, Failed };
  void begin();
  void connectPreferred();
  bool enabled() const { return radioEnabled; }
  bool setEnabled(bool enabled);
  void tick(uint32_t now);
  bool startScan();
  bool startJoin(const char* ssid, const char* password);
  void cancelJoin();
  bool forget(int index);
  int find(const char* ssid, bool secured) const;
  const WifiProfile& saved(int i) const { return data.profiles[i]; }
  int savedCount() const { return data.count; }
  int nearbyCount() const { return found; }
  const NearbyWifi& nearby(int i) const { return results[i]; }
  Scan scanState() const { return scanning; }
  Join joinState() const { return joining; }
  bool busy() const { return scanning == Scan::Running || joining == Join::Running; }

 private:
  static constexpr int maxSaved = 8, maxNearby = 16;
  struct SavedData {
    uint8_t version = 1;
    uint8_t count = 0;
    WifiProfile profiles[maxSaved];
  } data;
  bool save(const SavedData& next);
  bool remember();
  void finishScan(int count);
  void beginJoin();
  Preferences storage;
  NearbyWifi results[maxNearby];
  WifiProfile target;
  int found = 0;
  uint32_t joinStarted = 0;
  Scan scanning = Scan::Idle;
  Join joining = Join::Idle;
  bool radioEnabled = true;
  bool storageReady = false, hasSavedRecord = false, disconnecting = false;
};

#include "Bluetooth.h"
#include <BLEDevice.h>
#include <BLEScan.h>

// 在临界区标记扫描已启动，随后创建后台任务。创建失败时恢复可再次扫描的状态。
bool BluetoothScanner::start(bool filterUnnamed) {
  portENTER_CRITICAL(&lock);
  const bool running = results.state == BluetoothSnapshot::State::Scanning;
  if (!running) {
    namedOnly = filterUnnamed;
    results.state = BluetoothSnapshot::State::Scanning;
  }
  portEXIT_CRITICAL(&lock);
  if (running) return false;
  if (xTaskCreate(worker, "ble-scan", 8192, this, 1, nullptr) == pdPASS) return true;

  portENTER_CRITICAL(&lock);
  results.state = BluetoothSnapshot::State::Failed;
  portEXIT_CRITICAL(&lock);
  return false;
}

bool BluetoothScanner::busy() {
  portENTER_CRITICAL(&lock);
  const bool running = results.state == BluetoothSnapshot::State::Scanning;
  portEXIT_CRITICAL(&lock);
  return running;
}

BluetoothSnapshot BluetoothScanner::snapshot() {
  portENTER_CRITICAL(&lock);
  BluetoothSnapshot copy = results;
  portEXIT_CRITICAL(&lock);
  return copy;
}

// 在后台执行 BLE 扫描，按过滤条件保留信号较强的结果，最后一次性发布快照。
void BluetoothScanner::worker(void* context) {
  auto& scanner = *static_cast<BluetoothScanner*>(context);
  portENTER_CRITICAL(&scanner.lock);
  const bool namedOnly = scanner.namedOnly;
  portEXIT_CRITICAL(&scanner.lock);
  BluetoothSnapshot found;
  found.state = BluetoothSnapshot::State::Failed;
  Serial.println("[BLE] Active scan started (4 s)");
  const bool initialized = BLEDevice::init("");
  BLEScan* scan = initialized ? BLEDevice::getScan() : nullptr;
  if (scan) {
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(60);
    if (BLEScanResults* devices = scan->start(4, false)) {
      found.state = BluetoothSnapshot::State::Ready;
      found.detected = devices->getCount();
      for (int i = 0; i < found.detected; ++i) {
        auto device = devices->getDevice(i);
        const auto name = device.getName();
        if (namedOnly && !name.length()) continue;
        BluetoothResult entry;
        entry.rssi = device.getRSSI();
        int pos = found.count;
        while (pos > 0 && found.devices[pos - 1].rssi < entry.rssi) --pos;
        if (pos >= BluetoothSnapshot::limit) continue;
        const auto address = device.getAddress().toString();
        snprintf(entry.name, sizeof(entry.name), "%s", name.length() ? name.c_str() : "Unnamed");
        snprintf(entry.address, sizeof(entry.address), "%s", address.c_str());
        if (device.haveServiceUUID()) {
          const auto uuid = device.getServiceUUID().toString();
          snprintf(entry.service, sizeof(entry.service), "%s", uuid.c_str());
        }
        if (found.count < BluetoothSnapshot::limit) ++found.count;
        for (int j = found.count - 1; j > pos; --j) found.devices[j] = found.devices[j - 1];
        found.devices[pos] = entry;
      }
    }
    scan->clearResults();
  }
  // 关闭控制器但保留再次初始化的能力，方便下一次扫描和轻度休眠。
  if (initialized) BLEDevice::deinit(false);
  Serial.printf("[BLE] Scan %s; detected=%d retained=%d\n",
                found.state == BluetoothSnapshot::State::Ready ? "complete" : "failed",
                found.detected, found.count);
  portENTER_CRITICAL(&scanner.lock);
  scanner.results = found;
  portEXIT_CRITICAL(&scanner.lock);
  vTaskDelete(nullptr);
}

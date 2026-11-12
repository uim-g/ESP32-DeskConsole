#pragma once
#include <cstddef>
struct esp_partition_t { size_t size; };
inline const esp_partition_t* esp_ota_get_running_partition() {
  static constexpr esp_partition_t app{3*1024*1024};
  return &app;
}

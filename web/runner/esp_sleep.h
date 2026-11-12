#pragma once
using esp_err_t=int;
constexpr esp_err_t ESP_OK=0;
inline esp_err_t esp_sleep_enable_gpio_wakeup(){return ESP_OK;}
esp_err_t esp_light_sleep_start();

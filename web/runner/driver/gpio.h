#pragma once
#include "esp_sleep.h"
using gpio_num_t=int;
constexpr int GPIO_INTR_LOW_LEVEL=0;
inline esp_err_t gpio_wakeup_enable(gpio_num_t,int){return ESP_OK;}

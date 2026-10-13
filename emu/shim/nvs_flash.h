#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
esp_err_t nvs_flash_erase();
esp_err_t nvs_flash_init();
inline esp_err_t nvs_flash_deinit() { return 0; }

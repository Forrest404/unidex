#pragma once
#include <stdint.h>
#include <string.h>
enum esp_mac_type_t { ESP_MAC_WIFI_STA, ESP_MAC_WIFI_SOFTAP };
inline int esp_read_mac(uint8_t *mac, esp_mac_type_t) { static const uint8_t m[6] = {0x7c, 0xdf, 0xa1, 0x00, 0xde, 0x01}; memcpy(mac, m, 6); return 0; }

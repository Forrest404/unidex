#pragma once
enum esp_reset_reason_t { ESP_RST_POWERON = 1 };
inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }

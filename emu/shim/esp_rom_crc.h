#pragma once
#include <stdint.h>
#include <stddef.h>
// The ROM's CRC-32 (reflected, 0xEDB88320), the same value the device gives.
inline uint32_t esp_rom_crc32_le(uint32_t crc, const uint8_t *buf, uint32_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *buf++;
    for (int k = 0; k < 8; k++) crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
  }
  return ~crc;
}

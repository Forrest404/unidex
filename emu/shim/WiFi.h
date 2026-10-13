// No radio in the browser: a scan finds nothing and connecting fails (net_emu.cpp says why on screen).
#pragma once
#include <Arduino.h>
enum wifi_auth_mode_t { WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK,
                        WIFI_AUTH_WPA2_ENTERPRISE, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK };
enum wifi_mode_t { WIFI_OFF, WIFI_STA, WIFI_AP };
class WiFiClass {
 public:
  void mode(wifi_mode_t m) { m_ = m; }
  wifi_mode_t getMode() { return m_; }
  int scanNetworks(bool = false, bool = false, bool = false, int = 0) { return 0; }
  String SSID(int) { return String(); }
  const uint8_t *BSSID(int) { static const uint8_t z[6] = {0}; return z; }
  int RSSI(int) { return 0; }
  wifi_auth_mode_t encryptionType(int) { return WIFI_AUTH_OPEN; }
  void scanDelete() {}
 private:
  wifi_mode_t m_ = WIFI_OFF;
};
extern WiFiClass WiFi;

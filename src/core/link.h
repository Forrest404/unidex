#pragma once
#include <stddef.h>
#include <stdint.h>

// Device-to-device radio (ESP-NOW): short packets straight to other unidex devices nearby, no router. Only on
// while something asks for it (the Pet's Meet screen); it owns the WiFi radio meanwhile (netClaim) and keeps the
// device awake. Each session uses a new random radio address, so a device can't be followed by its chip address.
struct LinkPacket {
  uint8_t mac[6];  // who sent it (their address for this session)
  int8_t rssi;     // signal strength in dBm (about -25 touching, -60 across a room), 0 if unknown
  uint32_t at;     // millis() when the radio received it (the main loop may get to it later)
  uint8_t len;
  uint8_t data[250];
};

const char *linkStart();  // nullptr when on, else a short reason for the screen ("WiFi busy: a note is sending")
void linkStop();
bool linkOn();
bool linkBroadcast(const uint8_t *data, size_t len);                // to every device in range
bool linkSend(const uint8_t mac[6], const uint8_t *data, size_t len);  // to one (it confirms it heard)
bool linkPoll(LinkPacket &out);  // the next packet received, if any (call from the main loop)

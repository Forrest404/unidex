#pragma once
#include <stdint.h>

// "Open on phone": the device's own WiFi hotspot and a small website with the notes, for a phone or a
// computer nearby. Nothing goes through the internet. The hotspot has a new password every time it starts;
// the Notes screen shows it as a QR code the phone's camera joins with. Never at the same time as normal
// WiFi; it stops itself after a few minutes with nobody using it.
bool phoneStart();          // false if the WiFi is busy (a note sending)
void phoneStop();
bool phoneOn();
void phonePoll();           // call often while on: answers the phone; stops after the idle time
void phoneActivity();       // a button press counts as use (restarts the idle time)
const char *phoneSsid();    // "unidex-1A2B" (the same for this device every time)
const char *phonePassword();
const char *phoneJoinCode();  // what the QR code holds: WIFI:T:WPA;S:<ssid>;P:<password>;;
uint32_t phoneServed();     // pages served since it started
uint32_t phoneConnectedMs();  // how long since a phone first asked for anything (0: none yet)

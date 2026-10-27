#pragma once
#include <stdint.h>
#include "../../core/app.h"

// The phone portal ("Open on phone", and Settings > hold B): the device's own WiFi hotspot and a small website
// for a phone or a computer nearby, with the notes, the device's settings and its setup (WiFi, keys, GitHub).
// Nothing from the phone goes through the internet. The hotspot has a new password every time it starts; the screen
// shows it as a QR code the phone's camera joins with. Started from Settings, the device first joins the saved WiFi
// (for the Apps tab, which installs from the store over it) and puts the hotspot on its channel. It stops itself
// after a few minutes with nobody using it, or when the page's Done is pressed.
enum class PhoneTab : uint8_t { Notes, Settings, Setup };  // the page it opens on

const char *phoneStart(PhoneTab tab);  // nullptr, or why not ("Sending a note: try soon")
void phoneStop();
bool phoneOn();
Redraw phoneTick();          // call often while on: answers the phone; stops after the idle time or Done
// The QR code, the name and password to type, and how to stop. With `computer` (the first start's "Set it up"): a
// smaller code and that address for setting up on a computer instead, above the caller's hints.
void phoneDraw(const char *title, const char *computer = nullptr);
void phoneActivity();        // a button press counts as use (restarts the idle time)
bool phoneChanged();         // setup (WiFi, keys) was saved since it started: worth checking once it's off
bool phoneDone();            // it stopped because Done was pressed on the page (not after the idle time)
const char *phoneSsid();     // "unidex-1A2B" (the same for this device every time)
const char *phonePassword();
const char *phoneJoinCode(); // what the QR code holds: WIFI:T:WPA;S:<ssid>;P:<password>;;

#pragma once
#include <Arduino.h>

// Shared by the Pet screen (pet.cpp) and its USB commands (usb.cpp).
uint32_t petLookBits();  // the saved look, as pet_logic.h packs it
String petName();        // "" if none is set
void petChanged();       // the website saved a new look or name: the Pet screen redraws

// The website's Tools page (site/pet.js) talks to these over USB. Called from usbsync for every line starting
// with "P "; returns false if it isn't one of them.
//   P GET                        -> "OK P <look, hex> <name, hex>"
//   P SET <look hex> <name hex>  -> "OK P SET" or "ERR"  (name: up to 12 printable characters; empty = no name)
bool petUsb(const char *line);

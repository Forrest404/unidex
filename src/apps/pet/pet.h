#pragma once
#include <Arduino.h>

// Shared by the Pet screen (pet.cpp) and its USB commands (usb.cpp).
uint32_t petLookBits();  // the saved look, as pet_logic.h packs it
String petName();        // "" if none is set
void petChanged();       // the website saved a new look or name: the Pet screen redraws
enum class PetMood : uint8_t { Normal, Happy, Blink };
// A Pet's look, scale x 32 px, its top left at x0, y0 (parts off the screen are skipped); Happy and Blink swap in
// the happy or closed eyes.
void petDraw(uint32_t look, int scale, int16_t x0, int16_t y0, PetMood mood = PetMood::Normal);
void petDrawHeart(int16_t cx, int16_t cy, int scale);  // a small heart centred at cx, cy (9 x 8 at scale 1)
// A name tag over a Pet's head, like a game's player name: white text in a black box centred at cx, its bottom
// edge at `bottom`. Nothing for an empty name.
void petDrawTag(const char *name, int16_t cx, int16_t bottom);
// A speech bubble: the text in a white rounded box with a black edge and a small tail pointing down at cx; the
// box is centred at cx but kept on the screen; its tail's tip at `bottom`.
void petDrawBubble(const char *text, int16_t cx, int16_t bottom);

// The website's Tools page (site/pet.js) talks to these over USB. Called from usbsync for every line starting
// with "P "; returns false if it isn't one of them.
//   P GET                        -> "OK P <look, hex> <name, hex>"
//   P SET <look hex> <name hex>  -> "OK P SET" or "ERR"  (name: up to 12 printable characters; empty = no name)
bool petUsb(const char *line);

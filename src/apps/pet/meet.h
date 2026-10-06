#pragma once
#include "../../core/app.h"

// The Pet's Meet screen (hold B on the Pet): with another unidex in reach, B connects them and the two screens
// become one room where the Pets visit, swap and talk (meet.cpp). The radio (src/core/link.h) is only on while
// this screen is open, and stops after 2 minutes without a press (5 while connected).
void meetEnter();
void meetLeave();
Redraw meetButton(Event e);
void meetDraw();
Redraw meetTick();
// Test build: "meet:<on|off>:<Pets heard>:<searching|connected>:<left|right>:<x0>,<x1>:<strength both ways,
// dBm>:<latest dBm>:<one in reach 0|1>:f<friends>:<ask|list>"
bool meetBack();  // hold A: closes the friends list or a question; false when Meet itself should close
const char *meetDetail();

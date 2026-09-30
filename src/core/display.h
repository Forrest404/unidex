#pragma once
#include <GxEPD2_BW.h>

// 1.54" 200x200 SSD1681 panel.
extern GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display;

// initial=false after a deep-sleep wake: the panel kept its image, so skip the clearing full refresh.
void displayInit(bool initial);

// Draws a whole screen with draw() and refreshes the panel, then hibernates it.
// Partial refresh normally; full refresh when asked (app switches) or every 10 partials,
// to clear e-ink ghosting.
void displayShow(void (*draw)(), bool full);

// Animation frame (the Chooser spin only): partial refresh, not counted toward the rule above and
// no hibernate. Always finish an animation with a full displayShow() to clear the ghosting.
void displayFrame(void (*draw)());

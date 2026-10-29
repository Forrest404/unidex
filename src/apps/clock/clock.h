#pragma once
#include <Arduino.h>
#include "voice_logic.h"

// For the agent: does what a command asks, as Clock by voice does (Clock opens on what it changed), and says what
// it did or why not ("Timer 10:00", "3 alarms on: turn one off").
String clockCommand(const clockvoice::Command &c);
// The timer, the alarms and the stopwatch in a line ("no timer; alarm 1 7:00 Weekdays on; ...; stopwatch at zero").
String clockState();

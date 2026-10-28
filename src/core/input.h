#pragma once
#include <stdint.h>

// A = BOOT (next/scroll, long = home), B = PWR (select/action, long = app extra).
// Reset = both held for 1 s, then let go; if the loop is stuck (screen frozen), both held 3 s, then let go.
enum class Event { None, AShort, ALong, BShort, BLong, Reset };

void inputInit();  // the press that woke the board from deep sleep counts as input (not a power-on press)
Event inputPoll();  // call often; returns at most one event per call
bool inputAnyDown();
bool inputBHeld();  // B is down right now (raw pin, no debounce): for hold-to-talk, which polls it in a loop
// B presses counted as they happen (an interrupt), each with when it went down: for a game that draws frames while
// B is pressed, where a quick press during a refresh mustn't be missed or timed late. Count while on; take returns
// the presses since the last take (up to max, the earliest first).
void inputCountB(bool on);
int inputTakeBPresses(uint32_t *at, int max);
#if UNIDEX_DEV
void inputVirtualHoldB(uint32_t ms);  // test build: inputBHeld() also reads true for the next `ms`
void inputVirtualEvent(Event e);      // test build: inputPoll() returns e next (X HOLD A during a game)
#endif

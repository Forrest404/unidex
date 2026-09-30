#pragma once

// A = BOOT (next/scroll, long = home), B = PWR (select/action, long = app extra).
enum class Event { None, AShort, ALong, BShort, BLong };

void inputInit();  // the press that woke the board from deep sleep counts as input (not a power-on press)
Event inputPoll();  // call often; returns at most one event per call
bool inputAnyDown();

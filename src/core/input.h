#pragma once

// A = BOOT (next/scroll, long = home), B = PWR (select/action, long = app extra).
enum class Event { None, AShort, ALong, BShort, BLong };

void inputInit();  // a button already held (the wake press) is ignored until released
Event inputPoll();  // call often; returns at most one event per call
bool inputAnyDown();

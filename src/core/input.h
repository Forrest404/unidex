#pragma once

// A = BOOT (next/scroll, long = home), B = PWR (select/action, long = app extra).
enum class Event { None, AShort, ALong, BShort, BLong };

void inputInit();
Event inputPoll();  // call often; returns at most one event per call

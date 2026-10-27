#pragma once
#include <stdint.h>

// Something that just happened in another app, for the Pet to say the next time it's opened (within 2 hours):
// "Nice one!" for a reminder ticked off, "New best: 42!" for a game, "Got it!" for a note saved. Only the latest is
// kept. Safe to call from the Notes task.
enum class PetNotice : uint8_t { None, ReminderDone, NewBest, NoteSaved };
void petNotice(PetNotice what, int32_t value = 0);

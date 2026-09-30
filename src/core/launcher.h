#pragma once
#include "input.h"

void launcherBegin(bool woke);  // cold boot: splash + home; wake: nothing (the screen holds)
void launcherHandle(Event e);
void launcherPoll();  // call every loop: opens a badge that just arrived over USB

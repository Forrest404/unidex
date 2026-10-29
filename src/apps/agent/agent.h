#pragma once
#include <Arduino.h>
#include "../../core/app.h"

// The agent: hold B on the home screen and say what you want ("set a timer for 10 minutes", "remind me to call Sam
// at 5", "turn the volume down"), or ask something. Your words are transcribed, a model (Claude with an Anthropic
// key, else OpenAI) picks the actions (agent_logic.h), and the device does them: small ones at once, a setting or
// deleting after a yes. Its screens are a hidden app (not on the home carousel): Exit goes back home.
extern const App agentApp;

// A key to transcribe with (Notes' setting) and one for the model: the home screen offers "hold B: agent".
bool agentReady();
// Records while B is held, then works it out over WiFi and shows the answer (blocking: a few seconds).
void agentListen();

#if UNIDEX_DEV
void agentAsk(const char *words);   // test build, X AGENT: as if said (the network part for real)
void agentReply(const char *json);  // test build, X AGENTREPLY: as if the model answered this (no network)
#endif

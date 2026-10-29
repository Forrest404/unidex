#pragma once
#include <Arduino.h>
#include <vector>
#include "../../core/net.h"

// The online half of Notes. Every call expects WiFi to be up already (netConnect) and returns nullptr on
// success or a short reason that fits the screen ("OpenAI: bad key").

struct NoteText {
  String transcript;            // what Whisper heard
  String title, summary, body;  // from the cleanup model; body empty = use the transcript
  std::vector<String> topics;
  String eventTitle, eventStart, eventEnd;  // a dated plan in the note, if any ("YYYY-MM-DDTHH:MM")
  bool eventAllDay = false;
};

// `audio` is the whole WAV file (header and samples) as body parts. The service is the one chosen on the website
// (OpenAI Whisper or ElevenLabs Scribe, the "transcribe" setting).
const char *cloudTranscribe(const NetPart *audio, int nParts, String &text);
// The chosen transcriber's key setting ("openai_key" or "elevenlabs_key"), and nullptr when it's saved or else
// the line for the screen ("Setup: add OpenAI key").
const char *cloudTranscribeKey();
const char *cloudTranscribeMissing();
// Title, summary, cleaned body, topics and event, with the provider chosen on the website. With cleanup
// off (or no transcript) it only fills a short title from the first words.
const char *cloudCleanup(NoteText &note, const String &nowLocal);
// The agent: what the device can do and its state (`system`), and what was said. The model's answer in `reply` (JSON,
// agent_logic.h). Claude if an Anthropic key is saved, else OpenAI.
const char *cloudAgent(const char *system, const String &words, String &reply);
// Pushes the note to <gh_dir>/<name>.md in the GitHub repo, adding " 2", " 3"... if the name is taken.
// `path` gets the path it was saved at.
const char *cloudPush(const String &name, const String &markdown, String &path);
// Checks one service with the saved settings: "wifi", "openai", "elevenlabs", "anthropic" or "github". Connects
// WiFi itself.
const char *cloudTest(const char *what, String &detail);

String noteMarkdown(const NoteText &note, const String &id, const String &createdUtc);

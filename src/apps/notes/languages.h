#pragma once
#include <cstring>

// The languages a note can be set to (note_lang: Settings > Note language, or the phone portal's Transcription
// card). "" is Auto: the transcription service works it out. The codes are ISO 639-1, which Whisper ("language")
// and Scribe ("language_code") both take. No Arduino here, so tools/notetest can check it.
namespace languages {

struct Language {
  const char *code, *name;
};

const Language LIST[] = {
  {"", "Auto"},       {"en", "English"},   {"es", "Spanish"}, {"fr", "French"},  {"de", "German"},
  {"it", "Italian"},  {"pt", "Portuguese"}, {"nl", "Dutch"},  {"pl", "Polish"},  {"tr", "Turkish"},
  {"ru", "Russian"},  {"uk", "Ukrainian"}, {"ar", "Arabic"},  {"hi", "Hindi"},   {"zh", "Chinese"},
  {"ja", "Japanese"}, {"ko", "Korean"},
};
const int COUNT = sizeof(LIST) / sizeof(LIST[0]);

// Its place in LIST, or -1 if it isn't one (0 is Auto).
inline int find(const char *code) {
  for (int i = 0; i < COUNT; i++)
    if (strcmp(LIST[i].code, code) == 0) return i;
  return -1;
}

}  // namespace languages

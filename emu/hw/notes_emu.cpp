// Notes without the cloud, the hotspot or the USB commands: the list, viewer and card side run for real.
#include "../../src/apps/notes/job.h"
#include "../../src/apps/notes/cloud.h"
#include "../../src/apps/notes/phone.h"
#include "../../src/apps/notes/usb.h"
bool jobBusy() { return false; }
int16_t *jobTakeBuffer(size_t, size_t &got) { got = 0; return nullptr; }
void jobGiveBuffer(int16_t *) {}
bool jobCanTake() { return false; }
bool jobStartNote(const String &, int16_t *, size_t, bool) { return false; }
bool jobStartSweep(bool) { return false; }
bool jobStartReminders(const String &, int16_t *, size_t) { return false; }
bool jobTakeRemindersDone(String &, bool &) { return false; }
JobStatus jobStatus() { JobStatus s = {}; return s; }
size_t jobStackLeft() { return 0; }
bool jobTakeShown(String &, String &) { return false; }
static const char *NO = "Not in the demo";
const char *cloudTranscribe(const NetPart *, int, String &) { return NO; }
const char *cloudTranscribeKey() { return "openai_key"; }
const char *cloudTranscribeMissing() { return nullptr; }
const char *cloudCleanup(NoteText &, const String &) { return NO; }
const char *cloudPush(const String &, const String &, String &) { return NO; }
const char *cloudTest(const char *) { return NO; }
String noteMarkdown(const NoteText &, const String &, const String &) { return String(); }
bool phoneStart() { return false; }
void phoneStop() {}
bool phoneOn() { return false; }
void phonePoll() {}
void phoneActivity() {}
const char *phoneSsid() { return "unidex-demo"; }
const char *phonePassword() { return ""; }
const char *phoneJoinCode() { return ""; }
uint32_t phoneServed() { return 0; }
uint32_t phoneConnectedMs() { return 0; }
bool notesUsb(const char *) { return false; }

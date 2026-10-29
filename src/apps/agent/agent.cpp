#include "agent.h"
#if UNIDEX_EMU  // the emulator's Arduino stand-in has no Stream or Printable (and this reads from a String anyway)
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PRINTABLE 0
#endif
#include <ArduinoJson.h>
#include <vector>
#include "agent_logic.h"
#include "../apps.h"
#include "../clock/clock.h"
#include "../notes/cloud.h"
#include "../notes/job.h"
#include "../notes/record.h"
#include "../notes/store.h"
#include "../pet/pet.h"
#include "../reminders/reminders.h"
#include "../timetable/timetable.h"
#include "../../core/audio.h"
#include "../../core/battery.h"
#include "../../core/clock.h"
#include "../../core/credentials.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/net.h"
#include "../../core/power.h"
#include "../../core/theme.h"

static const uint32_t LISTEN_SECONDS = 15;  // a request is a sentence or two

enum Screen : uint8_t { IDLE, WORKING, ANSWER, ASKING };
static uint8_t screen;           // (RAM only: after a deep sleep it's IDLE, "hold B to ask")
static String heard, said, step;  // what you said, the answer, what it's doing now
static std::vector<String> lines;            // "+ Timer 10:00", "x No app "Mail""
static std::vector<agent::Action> waiting;   // big changes, asked one at a time
static int openAfter = -1;                   // an app to open once the answer's done (APPS index)

bool agentReady() {
  return !cloudTranscribeMissing() && (credHas("anthropic_key") || credHas("openai_key"));
}

// --- drawing ---

static void drawAnswer() {
  drawHeader("Agent");
  int16_t y = CONTENT_TOP + 12;
  if (heard.length()) {
    display.setFont(FONT_TINY);
    display.setCursor(MARGIN, y);
    display.print(fitText(("You: " + heard).c_str(), display.width() - 2 * MARGIN));
    y += 16;
  }
  display.setFont(FONT_SMALL);
  String text[5];
  const int n = min(wrapText(said.c_str(), display.width() - 2 * MARGIN, text, 5), 5 - (int)min<size_t>(lines.size(), 2));
  for (int i = 0; i < n; i++, y += 17) {
    display.setCursor(MARGIN, y);
    display.print(text[i]);
  }
  display.setFont(FONT_TINY);
  y += 2;
  for (size_t i = 0; i < lines.size() && y < HINTS_TOP - 4; i++, y += 13) {
    display.setCursor(MARGIN, y);
    display.print(fitText(lines[i].c_str(), display.width() - 2 * MARGIN));
  }
}

static void draw() {
  if (screen == WORKING) {
    drawHeader("Agent");
    drawEmpty(step.c_str(), heard.length() ? fitText(heard.c_str(), display.width() - 2 * MARGIN).c_str() : "",
              "");
    drawHints("", "", "");
    return;
  }
  if (screen == IDLE) {
    drawHeader("Agent");
    drawEmpty("Ask me anything", "Hold B and say what", "you want done");
    drawHints("done", "", "ask");
    return;
  }
  drawAnswer();
  if (screen == ASKING) {
    display.setFont(FONT_SMALL);
    drawSheet(agent::confirmText(waiting.front()).c_str(),
              fitText(agent::confirmLine(waiting.front()).c_str(), 164).c_str());
    drawHints("no", "yes", "");
  } else {
    drawHints("done", "", "ask again");
  }
}

#if UNIDEX_DEV
static const char *agentDetail();
#endif

static void show(Screen s) {
  screen = s;
#if UNIDEX_DEV
  devSetDetail(agentDetail);  // (an interruption skips onEnter)
#endif
  if (launcherShowing(&agentApp)) displayShow(draw, false);
  else launcherInterrupt(&agentApp);  // (draws it, full)
}

static void working(const char *what) {
  step = what;
  show(WORKING);
}

// --- doing things ---

static int appIndex(const String &name) {
  for (int i = 0; i < APP_COUNT; i++)
    if (String(APPS[i]->name).equalsIgnoreCase(name)) return i;
  return -1;
}

// Does one action; the line for the list: "+ what it did" or "x why not".
static String doAction(const agent::Action &a) {
  using clockvoice::Kind;
  clockvoice::Command c;
  const char *err = nullptr;
  switch (a.kind) {
    case agent::TIMER: c.kind = Kind::TIMER, c.seconds = a.seconds; break;
    case agent::TIMER_STOP: c.kind = Kind::TIMER_STOP; break;
    case agent::ALARM: c.kind = Kind::ALARM, c.hour = a.hour, c.minute = a.minute, c.repeat = a.repeat; break;
    case agent::ALARMS_OFF: c.kind = Kind::ALARMS_OFF; break;
    case agent::STOPWATCH:
      c.kind = a.text == "start" ? Kind::STOPWATCH_START : a.text == "stop" ? Kind::STOPWATCH_STOP : Kind::STOPWATCH_RESET;
      break;
    case agent::REMINDER: err = remindersAdd(a.text, a.due); break;
    case agent::REMINDER_DONE:
    case agent::REMINDER_DELETE: err = remindersFinish(a.text, a.kind == agent::REMINDER_DELETE); break;
    case agent::EVENT: err = timetableAddEvent(agent::eventLine(a).c_str()); break;
    case agent::OPEN: {
      const int i = appIndex(a.text.c_str());
      if (i < 0) err = "No such app";
      else openAfter = i;
      break;
    }
    case agent::SETTING:
      switch (a.setting) {
        case agent::VOLUME: audioSetVolume(a.value); break;
        case agent::ALERT_SOUND: audioSetAlertsMuted(!a.value); break;
        case agent::RINGTONE: audioSetRingtone(a.value); break;
        case agent::DARK_SCREEN: displaySetInverted(a.value); break;
        case agent::SLEEP_AFTER: powerSetSleepSeconds(a.value); break;
        default: break;
      }
      break;
    default: break;
  }
  if (c.kind != Kind::NONE) {
    const String done = clockCommand(c);  // Clock's own words: "Timer 10:00", "3 alarms on: turn one off"
    const bool ok = !done.startsWith("3 alarms") && !done.startsWith("No timer");
    return (ok ? "+ " : "x ") + done;
  }
  if (err) return String("x ") + err;
  return ("+ " + agent::doneText(a)).c_str();
}

// After the last question: the answer stays up, or the app asked for opens.
static void settle() {
  if (!waiting.empty()) return show(ASKING);
  if (openAfter >= 0) {
    const int i = openAfter;
    openAfter = -1;
    if (said.length()) launcherToast(said.c_str());
    launcherOpen(APPS[i]);
    return;
  }
  show(ANSWER);
}

// The model's answer: what it says, and its actions (checked; small ones done now, big ones asked).
static void answer(const String &reply) {
  lines.clear();
  waiting.clear();
  openAfter = -1;
  const int from = reply.indexOf('{'), to = reply.lastIndexOf('}');
  JsonDocument doc;
  if (from < 0 || to <= from || deserializeJson(doc, reply.substring(from, to + 1))) {
    said = "Sorry, that didn't work: try again";
    return settle();
  }
  said = asciiText(doc["say"] | "");
  said = agent::oneLine(said.c_str(), agent::MAX_SAY).c_str();
  int n = 0;
  for (JsonObject o : doc["actions"].as<JsonArray>()) {
    if (++n > agent::MAX_ACTIONS) {
      lines.push_back("x Only 5 at a time");
      break;
    }
    agent::Raw raw;
    raw.kind = (o["do"] | "");
    for (JsonPair p : o)  // (a null is no value, not the word "null")
      if (strcmp(p.key().c_str(), "do") && !p.value().isNull())
        raw.fields.push_back({p.key().c_str(), p.value().as<String>().c_str()});
    agent::Action a;
    std::string why;
    if (!agent::validate(raw, a, why)) {
      lines.push_back(("x " + why).c_str());
      continue;
    }
    if (a.kind == agent::REMINDER_DONE || a.kind == agent::REMINDER_DELETE) {  // which one, by its words
      std::vector<std::string> titles;
      for (const reminders::Reminder &r : remindersList(a.kind == agent::REMINDER_DELETE)) titles.push_back(r.title);
      const int i = agent::matchTitle(titles, a.text);
      if (i < 0) {
        lines.push_back(("x No reminder \"" + agent::oneLine(a.text, 20) + "\"").c_str());
        continue;
      }
      a.text = titles[i];
    }
    if (agent::needsAsking(a)) waiting.push_back(a);
    else lines.push_back(doAction(a));
  }
  if (!said.length()) said = lines.empty() && waiting.empty() ? "Done" : "";
  settle();
}

// --- what the model is told ---

static String systemText() {
  agent::State s;
  if (clockValid()) {
    const struct tm t = clockLocal();
    char now[40];
    strftime(now, sizeof now, "%A %Y-%m-%d %H:%M", &t);
    s.now = now;
  }
  s.owner = petOwnerName().c_str();
  const String agenda = timetableAgenda(7);
  for (int from = 0, to; from < (int)agenda.length() && (to = agenda.indexOf('\n', from)) >= 0; from = to + 1)
    s.soon.push_back(agenda.substring(from, to).c_str());
  for (const reminders::Reminder &r : remindersList()) {
    if (s.reminders.size() >= 10) break;
    s.reminders.push_back(r.title + (r.due.empty() ? "" : " (due " + r.due + ")"));
  }
  s.clock = clockState().c_str();
  char settings[120];
  snprintf(settings, sizeof settings, "volume %d, alert_sound %s, ringtone %s, dark_screen %s, sleep_after %d",
           audioVolume(), audioAlertsMuted() ? "off" : "on", audioRingtoneName(audioRingtone()),
           display.inverted ? "on" : "off", powerSleepSeconds());
  s.settings = settings;
  return String(agent::PROMPT) + "\n" + agent::context(s).c_str();
}

// --- listening ---

static void fail(const char *why) {
  lines.clear();
  waiting.clear();
  said = why;
  show(ANSWER);
}

// From the words on: WiFi, the model, then its answer. `samples` (if any) are transcribed first and given back.
static void think(int16_t *samples, size_t count) {
  working("Connecting...");
  netClaim(true);  // the WiFi is ours until the answer's back: Notes' background sends wait
  powerHold();
  const char *err = netConnect();
  if (samples) {
    if (!err) {
      working("Listening...");
      uint8_t wav[44];
      storeWavHeader(wav, count * 2);
      const NetPart audio[] = {NetPart::bytes(wav, sizeof wav), NetPart::bytes(samples, count * 2)};
      err = cloudTranscribe(audio, 2, heard);
    }
    jobGiveBuffer(samples);
    heard = asciiText(heard);
    heard.trim();
    if (!err && !heard.length()) err = "Heard nothing";
  }
  String reply;
  if (!err) {
    working("Thinking...");
    err = cloudAgent(systemText().c_str(), heard, reply);
  }
  netOff();
  netClaim(false);
  powerRelease();
  if (err) return fail(err);
  answer(reply);
}

// Why it can't listen now, or nullptr.
static const char *cantListen() {
  if (!agentReady()) return "Add your keys on the Setup page";
  if (!credWifiAny()) return "The agent needs WiFi: Setup";
  if (jobBusy() || netClaimed()) return "WiFi busy: try again soon";
  if (const char *low = batteryTooLow()) return low;
  if (!jobCanTake()) return "Still saving a note";
  return nullptr;
}

void agentListen() {
  heard = "";
  if (const char *why = cantListen()) return fail(why);
  int16_t *samples;
  size_t count;
  if (const char *err = recordWhileHeld(LISTEN_SECONDS, samples, count)) return fail(err);
  think(samples, count);
}

#if UNIDEX_DEV
void agentAsk(const char *words) {
  heard = words;
  if (const char *why = cantListen()) return fail(why);
  think(nullptr, 0);
}

void agentReply(const char *json) {
  heard = "(test)";
  answer(json);
}

// X STATE's detail: "agent:<idle|working|answer|asking>:<waiting>:<what it said>|<line>|<line>..."
static const char *agentDetail() {
  static String d;
  static const char *const NAMES[] = {"idle", "working", "answer", "asking"};
  d = String("agent:") + NAMES[screen] + ":" + String((int)waiting.size()) + ":" + said;
  for (const String &l : lines) d += "|" + l;
  return d.c_str();
}
#endif

// --- the app ---

static void onEnter() {
  screen = IDLE;
#if UNIDEX_DEV
  devSetDetail(agentDetail);
#endif
}

static Redraw onButton(Event e) {
#if UNIDEX_DEV
  devSetDetail(agentDetail);
#endif
  if (screen == ASKING) {
    if (e != Event::BShort && e != Event::AShort) return Redraw::None;
    const agent::Action a = waiting.front();
    waiting.erase(waiting.begin());
    lines.push_back(e == Event::BShort ? doAction(a) : String("x Left as it was"));
    if (waiting.empty() && said.endsWith("?")) said = "";  // the question's answered: the list says how
    const bool full = e == Event::BShort && a.kind == agent::SETTING && a.setting == agent::DARK_SCREEN;
    settle();
    return full ? Redraw::Full : Redraw::None;  // (a dark screen needs a full refresh)
  }
  if (e == Event::AShort) return Redraw::Exit;
  if (e == Event::BLong) agentListen();
  return Redraw::None;
}

static void onExit() {
#if UNIDEX_DEV
  devSetDetail(nullptr);
#endif
}

static Redraw onBack() { return Redraw::Exit; }

extern const App agentApp = {"Agent", ICON_APPS, onEnter, onButton, draw, onExit, onBack, nullptr, nullptr, false};

// Pet: a small creature you dress up (body, eyes, mouth, hat, extra), saved in NVS as one number ("pet_look").
// Main screen: A = say hi (happy eyes and a heart), B = edit, hold B = meet other Pets (meet.cpp), hold A = back.
// It blinks every few seconds for a minute after the last press. Edit: A = next row, B = change it (next option;
// Random: a new look; Done: save), hold B = previous option, hold A = save and back.
#include <esp_random.h>
#include "meet.h"
#include "pet.h"
#include "pet_logic.h"
#include "talk_logic.h"
#include "../reminders/reminders.h"
#include "../timetable/timetable.h"
#include "../../core/app.h"
#include "../../core/clock.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const int MAIN_SCALE = 4, EDIT_SCALE = 3;  // 128 px on the main screen, 96 px while editing
static const int16_t EDIT_AVATAR_Y = CONTENT_TOP + 4, ROW_Y = HINTS_TOP - 14;
static const int NAME_ROW = pet::LAYERS, OWNER_ROW = pet::LAYERS + 1, RANDOM_ROW = pet::LAYERS + 2;
static const int DONE_ROW = pet::LAYERS + 3, ROWS = pet::LAYERS + 4;

enum Screen : uint8_t { MAIN, EDIT, MEET, NAME };
RTC_DATA_ATTR static uint8_t screen, row;
RTC_DATA_ATTR static uint32_t draft;  // the look being edited (kept through sleep)
RTC_DATA_ATTR static char places[pet::MAX_NAME + 1];  // the name being edited, a letter a place (pet_logic.h)
RTC_DATA_ATTR static uint8_t place;                    // the place being chosen
RTC_DATA_ATTR static bool ownerNaming;                 // the name being edited is yours, not the Pet's
static bool blinking, happy;
static uint8_t heart;  // the heart beside it while it's happy: 0 none, else its scale (it pops: 1, then 2)
static uint32_t nextBlinkAt, openAt, lastPressAt;

static const int16_t MAIN_Y = CONTENT_TOP + (HINTS_TOP - CONTENT_TOP - pet::SIZE * MAIN_SCALE) / 2;  // centred
// With something to say: the Pet keeps its size but moves left, its drawing against the screen's edge, and a bubble
// at the top right, in the room beside its head, points at it.
static const int16_t TALK_TOP = CONTENT_TOP + 3;
static const int16_t TALK_AT = MAIN_Y + 10 * MAIN_SCALE + 2;  // where the tail points: about its eyes
static const uint32_t AWAKE_MS = 60000;  // blinking stops this long after the last press

static const char *const HEART[] = {  // beside its head while it's happy
  ".##...##.",
  "####.####",
  "#########",
  "#########",
  ".#######.",
  "..#####..",
  "...###...",
  "....#....",
};

uint32_t petLookBits() { return storageGetInt("pet_look", pet::pack(pet::DEFAULT_LOOK)); }

String petName() { return storageGetString("pet_name"); }

String petOwnerName() { return storageGetString("owner_name"); }

static uint32_t changes;  // counts saves from the website; the screen and the home line each notice a new one

void petChanged() { changes++; }

// What it says about your day (talk_logic.h), from what the Timetable and Reminders keep for the home screen.
// The home line asks on every pass, so it's worked out at most once a second (and your name read once a change).
static const talk::Talk &todaysTalk() {
  static talk::Talk said;
  static uint32_t at, seenChanges;
  static bool worked;
  static String owner;
  if (!worked || seenChanges != changes) owner = petOwnerName();
  if (worked && seenChanges == changes && millis() - at < 1000) return said;
  worked = true;
  at = millis();
  seenChanges = changes;
  const time_t now = time(nullptr);
  talk::Agenda a;
  a.owner = owner.c_str();
  a.clock = clockValid();
  if (a.clock) {
    const struct tm t = clockLocal();
    a.minute = t.tm_hour * 60 + t.tm_min;
    a.wday = t.tm_wday;
    TimetableSoon soon[3];
    a.eventCount = timetableSoon(soon);
    for (int i = 0; i < a.eventCount; i++) {  // whole minutes, rounded up as the Timetable's line does
      const time_t start = soon[i].start - now, end = soon[i].end - now;
      a.events[i] = {soon[i].title, soon[i].room, start > 0 ? (long)(start + 59) / 60 : (long)start / 60,
                     (long)(end + 59) / 60};
    }
    RemindersNext r;
    if (remindersNext(&r)) {
      a.late = r.late;
      a.lateTitle = r.lateTitle;
      const long today = reminders::daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) * 1440L + a.minute;
      if (r.nextMin >= 0 && r.nextHasTime) a.reminderIn = r.nextMin - today;
      a.reminderTitle = r.nextTitle;
    }
  }
  said = talk::say(a);
  return said;
}

void petDraw(uint32_t look, int scale, int16_t x0, int16_t y0, PetMood mood) {
  pet::Look l = pet::unpack(look);
  if (mood == PetMood::Happy) l.part[pet::EYES_LAYER] = pet::HAPPY_EYES;
  pet::drawLook(l, mood == PetMood::Blink, [&](int x, int y, bool ink) {
    display.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, ink ? BLACK : WHITE);
  });
}

void petDrawTag(const char *name, int16_t cx, int16_t bottom) {
  if (!name || !*name) return;
  display.setFont(FONT_TINY);
  const int16_t w = textWidth(name) + 8, h = 14, x = cx - w / 2, y = bottom - h;
  display.fillRoundRect(x - 1, y - 1, w + 2, h + 2, 4, WHITE);  // a white rim keeps it clear of what's behind
  display.fillRoundRect(x, y, w, h, 3, BLACK);
  display.setTextColor(WHITE);
  display.setCursor(x + 4, y + 10);
  display.print(name);
  display.setTextColor(BLACK);
}

void petDrawBubble(const char *text, int16_t cx, int16_t bottom) {
  if (!text || !*text) return;
  display.setFont(FONT_TINY);
  const int16_t w = textWidth(text) + 10, h = 15, y = bottom - 4 - h;
  int16_t x = cx - w / 2;
  x = max<int16_t>(1, min<int16_t>(x, display.width() - 1 - w));
  display.fillRoundRect(x, y, w, h, 5, WHITE);
  display.drawRoundRect(x, y, w, h, 5, BLACK);
  const int16_t tx = max<int16_t>(x + 6, min<int16_t>(cx, x + w - 7));  // the tail, under the speaker
  display.fillTriangle(tx - 3, y + h - 1, tx + 3, y + h - 1, tx, bottom, WHITE);
  display.drawLine(tx - 3, y + h - 1, tx, bottom, BLACK);
  display.drawLine(tx + 3, y + h - 1, tx, bottom, BLACK);
  display.setCursor(x + 5, y + 11);
  display.print(text);
}

void petDrawHeart(int16_t cx, int16_t cy, int scale) {
  const int16_t left = cx - 9 * scale / 2, top = cy - 4 * scale;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 9; x++)
      if (HEART[y][x] == '#') display.fillRect(left + x * scale, top + y * scale, scale, scale, BLACK);
}

static void drawAvatar(const pet::Look &look, int scale, int16_t y0, bool eyesClosed = false, int16_t x0 = INT16_MIN) {
  if (x0 == INT16_MIN) x0 = (display.width() - pet::SIZE * scale) / 2;  // centred
  pet::drawLook(look, eyesClosed, [&](int x, int y, bool ink) {
    display.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, ink ? BLACK : WHITE);
  });
}

static void save() { storagePutInt("pet_look", pet::pack(pet::unpack(draft))); }  // skips the write if unchanged

static uint32_t randomNumber() { return devSeed() ? (uint32_t)rand() : esp_random(); }  // test build: repeatable

// The bubble beside the Pet: the words wrapped to its width (the small font, or the tiny one if a word is too wide
// or it needs more than 5 lines), each line centred, the box level with its face where it fits, the tail pointing at
// it.
static std::string talkShown;  // what it said on the last draw

// Where the look draws: its first column, and each row's last (-1 for an empty row), so the Pet can sit against the
// edge and the bubble take the room beside it, which is widest by its head (most Pets are wider lower down).
struct Outline {
  int first;
  int8_t last[pet::SIZE];
};

static Outline outlineOf(const pet::Look &look) {
  Outline o;
  o.first = pet::SIZE - 1;
  memset(o.last, -1, sizeof o.last);
  pet::drawLook(look, false, [&](int x, int y, bool) { o.first = min(o.first, x), o.last[y] = max<int>(o.last[y], x); });
  if (o.first == pet::SIZE - 1) o.first = 0;
  return o;
}

// The screen x just right of the Pet (drawn from x0) in the rows between screen y0 and y1.
static int16_t rightOf(const Outline &o, int16_t x0, int16_t y0, int16_t y1) {
  int last = -1;
  for (int r = max(0, (y0 - MAIN_Y) / MAIN_SCALE); r <= min(pet::SIZE - 1, (y1 - MAIN_Y) / MAIN_SCALE); r++)
    last = max<int>(last, o.last[r]);
  return x0 + (last + 1) * MAIN_SCALE;
}

// True if no word of the text is wider than w in the current font (wrapText would split it).
static bool wordsFit(const char *text, int16_t w) {
  String word;
  for (const char *c = text;; c++) {
    if (*c && *c != ' ') {
      word += *c;
      continue;
    }
    if (word.length() && textWidth(word.c_str()) > w) return false;
    word = "";
    if (!*c) return true;
  }
}

static void drawTalk(const char *text, const Outline &o, int16_t x0) {
  static const int MAX_LINES = 8;
  const int16_t right = display.width() - 1;
  String lines[MAX_LINES];
  int n = 0;
  int16_t lineH = 16, h = 70, left = 0;
  for (int pass = 0; pass < 2; pass++) {  // the room depends on how tall it is, and that on how wide
    left = rightOf(o, x0, TALK_TOP, max<int16_t>(TALK_TOP + h, TALK_AT)) + 5;
    const int16_t inner = right - left - 6;
    for (const GFXfont *font : {FONT_SMALL, FONT_TINY}) {  // the small font unless a word or the lines don't fit
      display.setFont(font);
      lineH = font == FONT_SMALL ? 16 : 12;
      n = min(MAX_LINES, wrapText(text, inner, lines, font == FONT_SMALL ? 6 : MAX_LINES));
      if (font == FONT_SMALL && wordsFit(text, inner) && n <= 6) break;
    }
    h = n * lineH + 6;
  }
  const int16_t top = TALK_TOP, cx = (left + right + 1) / 2, bottom = top + h - 1;
  const int16_t tip = rightOf(o, x0, TALK_AT, TALK_AT) + 1;  // just right of its face
  display.fillRoundRect(left, top, right - left + 1, h, 6, WHITE);
  display.drawRoundRect(left, top, right - left + 1, h, 6, BLACK);
  if (bottom - 6 >= TALK_AT) {  // a tall bubble: the tail from its side
    display.fillTriangle(left, TALK_AT - 6, left, TALK_AT + 2, tip, TALK_AT, WHITE);
    display.drawLine(left, TALK_AT - 6, tip, TALK_AT, BLACK);
    display.drawLine(left, TALK_AT + 2, tip, TALK_AT, BLACK);
  } else {  // a short one: down from its bottom left
    display.fillTriangle(left + 5, bottom, left + 13, bottom, tip, TALK_AT, WHITE);
    display.drawLine(left + 5, bottom, tip, TALK_AT, BLACK);
    display.drawLine(left + 13, bottom, tip, TALK_AT, BLACK);
  }
  for (int i = 0; i < n; i++) {
    display.setCursor(cx - textWidth(lines[i].c_str()) / 2, top + lineH - 1 + i * lineH);
    display.print(lines[i]);
  }
}

static void drawMain() {
  const String name = petName();
  drawHeader("Pet", name.length() ? name.c_str() : nullptr);
  const talk::Talk &said = todaysTalk();
  const String hi = happy && petOwnerName().length() ? "Hi " + petOwnerName() + "!" : "";  // A: hi, by name
  const char *text = hi.length() ? hi.c_str() : said.text.c_str();
  talkShown = said.text;
  pet::Look look = pet::unpack(petLookBits());
  if (happy || said.mood == talk::HAPPY) look.part[pet::EYES_LAYER] = pet::HAPPY_EYES;
  const bool asleep = said.mood == talk::ASLEEP && !happy;
  if (*text) {
    const Outline o = outlineOf(look);
    const int16_t x0 = -o.first * MAIN_SCALE;  // its drawing from the screen's edge
    drawAvatar(look, MAIN_SCALE, MAIN_Y, blinking || asleep, x0);
    drawTalk(text, o, x0);
    if (heart) petDrawHeart(display.width() - 22, HINTS_TOP - 14, heart);  // under the bubble
  } else {
    drawAvatar(look, MAIN_SCALE, MAIN_Y, blinking);
    if (heart) petDrawHeart(175, CONTENT_TOP + 16, heart);
  }
  drawHints("hi", "dress up", "meet");
}

// A: it stays put (moving leaves smears on e-ink) and gets happy eyes while a heart pops up beside it, small then
// big; then back to normal with a refresh that drives every pixel, so nothing is left behind.
static Redraw sayHi() {
  powerHold();
  happy = true;
  heart = 1;
  displayFrame(drawMain);
  heart = 2;
  displayFrame(drawMain);
  delay(700);
  happy = false;
  heart = 0;
  displayClean(drawMain);
  powerRelease();
  return Redraw::None;
}

#if UNIDEX_DEV
static const char *detail();
#endif

static void onEnter() {
  screen = MAIN;
  lastPressAt = millis();
#if UNIDEX_DEV
  devSetDetail(detail);  // (and in tick, for a wake from sleep, which skips this)
#endif
}

static Redraw onButton(Event e) {
  lastPressAt = millis();
  if (screen == MEET) return meetButton(e);
  if (screen == MAIN) {
    if (e == Event::AShort) return sayHi();
    if (e == Event::BLong) {
      screen = MEET;
      meetEnter();
      return Redraw::Full;
    }
    if (e != Event::BShort) return Redraw::None;
    draft = petLookBits();
    row = 0;
    screen = EDIT;
    return Redraw::Full;  // the avatar changes size: a full refresh leaves no ghost
  }
  if (screen == NAME) {
    if (e == Event::AShort || e == Event::BLong) {
      places[place] = pet::stepChar(places[place], e == Event::AShort ? 1 : -1);
    } else if (e == Event::BShort && pet::nameDoneAt(places, place)) {
      char name[pet::MAX_NAME + 1];
      pet::nameFromPlaces(places, name);
      const char *key = ownerNaming ? "owner_name" : "pet_name";
      if (*name) storagePutString(key, name);
      else storageRemoveKey(key);
      petChanged();
      screen = EDIT;
    } else if (e == Event::BShort) {
      place++;
    } else {
      return Redraw::None;
    }
    return Redraw::Partial;
  }
  pet::Look look = pet::unpack(draft);
  if (e == Event::BShort && (row == NAME_ROW || row == OWNER_ROW)) {
    ownerNaming = row == OWNER_ROW;
    pet::placesFromName((ownerNaming ? petOwnerName() : petName()).c_str(), places);
    place = 0;
    screen = NAME;
    return Redraw::Partial;
  }
  if (e == Event::AShort) {
    row = (row + 1) % ROWS;
  } else if (e == Event::BShort && row == DONE_ROW) {
    save();
    screen = MAIN;
    return Redraw::Full;
  } else if (e == Event::BShort && row == RANDOM_ROW) {
    if (devSeed()) srand(devSeed() + draft);
    look = pet::randomLook(randomNumber);
  } else if ((e == Event::BShort || e == Event::BLong) && row < pet::LAYERS) {
    pet::stepPart(look, row, e == Event::BShort ? 1 : -1);
  } else {
    return Redraw::None;
  }
  draft = pet::pack(look);
  return Redraw::Partial;
}

static Redraw onBack() {
  lastPressAt = millis();
  if (screen == MAIN) return Redraw::Exit;
  if (screen == MEET) {
    if (meetBack()) return Redraw::Partial;  // the friends list or its question closes first
    meetLeave();
    screen = MAIN;
    return Redraw::Full;
  }
  if (screen == NAME) {  // back without changing the name
    screen = EDIT;
    return Redraw::Partial;
  }
  save();
  screen = MAIN;
  return Redraw::Full;
}

static void drawEdit() {
  const pet::Look look = pet::unpack(draft);
  drawHeader("Dress up");
  drawAvatar(look, EDIT_SCALE, EDIT_AVATAR_Y);
  for (int r = 0; r < ROWS; r++) {  // which row of the editor this is: one dot each
    const int16_t x = display.width() / 2 + (r * 2 - (ROWS - 1)) * 5, y = EDIT_AVATAR_Y + pet::SIZE * EDIT_SCALE + 5;
    if (r == row) display.fillCircle(x, y, 3, BLACK);
    else display.drawPixel(x, y, BLACK), display.drawCircle(x, y, 1, BLACK);
  }
  display.setFont(FONT_SMALL);
  char right[16];
  if (row < pet::LAYERS) {
    const pet::LayerParts lp = pet::partsOf(row);
    display.setCursor(MARGIN, ROW_Y);
    display.print(pet::LAYER_NAMES[row]);
    snprintf(right, sizeof right, "%s  %d/%d", lp.parts[look.part[row]].name, look.part[row] + 1, lp.count);
    drawRight(right, ROW_Y);
    drawHints("next", "change", "previous");
  } else if (row == NAME_ROW || row == OWNER_ROW) {
    display.setCursor(MARGIN, ROW_Y);
    display.print(row == OWNER_ROW ? "Your name" : "Name");
    const String name = row == OWNER_ROW ? petOwnerName() : petName();
    drawRight(name.length() ? name.c_str() : "none", ROW_Y);
    drawHints("next", "change", "");
  } else if (row == RANDOM_ROW) {
    display.setCursor(MARGIN, ROW_Y);
    display.print("Random look");
    drawHints("next", "shuffle", "");
  } else {
    display.setCursor(MARGIN, ROW_Y);
    display.print("Done");
    drawHints("next", "save", "");
  }
}

// The name a letter at a time: the place being chosen in a black box (a blank place shows as a short line).
static void drawName() {
  drawHeader(ownerNaming ? "Your name" : "Name");
  display.setFont(FONT_MEDIUM);
  int len = pet::MAX_NAME;
  while (len > 0 && places[len - 1] == ' ') len--;
  const int shown = max(len, place + 1);
  int16_t widths[pet::MAX_NAME], total = 0;
  for (int i = 0; i < shown; i++) {
    const char c[2] = {places[i], 0};
    widths[i] = places[i] == ' ' ? 10 : textWidth(c);
    total += widths[i] + 2;
  }
  const int16_t baseline = CONTENT_TOP + 62;
  int16_t x = (display.width() - total) / 2;
  for (int i = 0; i < shown; i++) {
    const char c[2] = {places[i], 0};
    if (i == place) {
      display.fillRoundRect(x - 2, baseline - 21, widths[i] + 4, 28, 3, BLACK);
      display.setTextColor(WHITE);
    } else if (places[i] == ' ') {
      display.drawFastHLine(x + 1, baseline + 2, widths[i] - 2, BLACK);
    }
    display.setCursor(x, baseline);
    display.print(c);
    display.setTextColor(BLACK);
    x += widths[i] + 2;
  }
  display.setFont(FONT_SMALL);
  const bool done = pet::nameDoneAt(places, place);
  char counter[24];
  snprintf(counter, sizeof counter, "letter %d of %d", place + 1, pet::MAX_NAME);
  display.setFont(FONT_TINY);
  if (done && len) snprintf(counter, sizeof counter, "%d letter%s", len, len == 1 ? "" : "s");
  drawCentered(done && !len ? "no name" : counter, CONTENT_TOP + 104);
  drawHints("letter", done ? "done" : "next", "previous");
}

static void draw() {
  if (screen == NAME) drawName();
  else if (screen == EDIT) drawEdit();
  else if (screen == MEET) meetDraw();
  else drawMain();
}

#if UNIDEX_DEV
// Test build (X STATE detail): "main:<saved look>" or "edit:<row>:<look being edited>", looks in hex.
static const char *detail() {
  static char buf[96];
  if (screen == MEET) return meetDetail();
  if (screen == NAME) {
    char name[pet::MAX_NAME + 1];
    pet::nameFromPlaces(places, name);
    snprintf(buf, sizeof buf, "%s:%d:%s", ownerNaming ? "owner" : "name", place, name);
    return buf;
  }
  if (screen == EDIT) snprintf(buf, sizeof buf, "edit:%d:%05lx", row, (unsigned long)pet::pack(pet::unpack(draft)));
  else snprintf(buf, sizeof buf, "main:%05lx:%s", (unsigned long)pet::pack(pet::unpack(petLookBits())), todaysTalk().text.c_str());
  return buf;
}
#endif

static Redraw tick() {
#if UNIDEX_DEV
  devSetDetail(detail);  // here rather than onEnter, which a wake from sleep skips
#endif
  static uint32_t seen;
  if (seen != changes) {  // a new look or name from the website
    seen = changes;
    if (screen == EDIT) draft = petLookBits();
    return Redraw::Partial;
  }
  if (screen == MEET) return meetTick();
  if (screen == MAIN && todaysTalk().text != talkShown) return Redraw::Partial;  // a new minute changed it
#if UNIDEX_DEV
  if (devManualFrames()) return Redraw::None;  // test build: no blinking, for repeatable screenshots
#endif
  // Blink: eyes shut for one refresh every 2.5-6 s. On battery the device naps between presses, so it asks to
  // be woken in time for the next one.
  const uint32_t now = millis();
  if (screen != MAIN) {
    blinking = false;
    return Redraw::None;
  }
  if (blinking) {
    if ((int32_t)(now - openAt) < 0) {
      powerWakeWithin(openAt - now);
      return Redraw::None;
    }
    blinking = false;
    nextBlinkAt = now + 2500 + esp_random() % 3500;
    return Redraw::Tick;
  }
  if (now - lastPressAt > AWAKE_MS || todaysTalk().mood == talk::ASLEEP) return Redraw::None;
  if (nextBlinkAt == 0) nextBlinkAt = now + 2500 + esp_random() % 3500;
  if ((int32_t)(now - nextBlinkAt) < 0) {
    powerWakeWithin(nextBlinkAt - now);
    return Redraw::None;
  }
  blinking = true;
  openAt = now + 120;
  return Redraw::Tick;
}

static void onExit() {
  if (screen == MEET) {  // leaving the app (e.g. a new badge opened the Badge app): radio off
    meetLeave();
    screen = MAIN;
  }
#if UNIDEX_DEV
  devSetDetail(nullptr);
#endif
}

static void status(char *out, size_t len) {
  static int8_t named = -1;  // looked up once, and again after a change (not on every redraw of the home screen)
  static uint32_t seen;
  static char name[16];
  if (named < 0 || seen != changes) {
    seen = changes;
    strlcpy(name, petName().c_str(), sizeof name);
    named = name[0] != 0;
  }
  const talk::Talk &said = todaysTalk();
  if (said.home) {  // about now: the next class, a late reminder
    snprintf(out, len, "%s", said.text.c_str());
    return;
  }
  const int friends = meetFriendCount();
  if (friends && named) snprintf(out, len, "%s, %d friend%s", name, friends, friends == 1 ? "" : "s");
  else if (friends) snprintf(out, len, "%d friend%s", friends, friends == 1 ? "" : "s");
  else if (named) snprintf(out, len, "Say hi to %s!", name);
  else snprintf(out, len, "Say hi!");
}

extern const App petApp = {"Pet", ICON_PET, onEnter, onButton, draw, onExit, onBack, status, tick};

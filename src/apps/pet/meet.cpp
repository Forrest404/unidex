#include "meet.h"
#include <esp_random.h>
#include "meet_logic.h"
#include "pet.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/link.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

// Searching: your Pet waits on its own screen. With another device in reach (up to about 30 cm), B connects the
// two, and from then on the two screens are one room, 400 px wide: the device whose Pet id is lower is the left
// half. The left device runs the show: it sends each act (meet_logic.h) and both play it on the same beat.

static const uint32_t HELLO_MS = 1000, STOP_MS = 120000, STOP_CONNECTED_MS = 300000;
static const uint32_t BEAT_MS = 500, LEAD_MS = 1300;  // a frame a beat; an act starts this long after it's sent
static const uint32_t PARTED_MS = 3000;               // weak this long: they've been parted
static const int SCALE = 3, SIZE = 32 * SCALE;        // Pets at 96 px
static const int16_t PY = HINTS_TOP - 4 - SIZE;       // their top, low enough for a tag and a bubble above

static meet::NearbyList nearby;
static const char *error;  // why the radio didn't start
static bool stopped;       // stopped by itself: no press for a while
static uint32_t lastPress, nextHelloAt, nextBlinkAt, openAt;
static bool blinking;

// Connected
static bool connected, amLeft;
static meet::Nearby partner;  // the other Pet (copied: kept while it walks away)
static int16_t pos[2];        // where Pet 0 (left screen's) and Pet 1 stand, in room x
static uint32_t partedSince, lastActAt, showGap;  // lastActAt: when the last act ended
static meet::Script script;
static const meet::Frame *current;  // the frame being shown during an act

// This device's Pet id: random, made the first time, kept in NVS. Sent instead of anything that identifies the
// device itself.
static uint32_t myId() {
  static uint32_t id;
  if (!id) {
    id = storageGetInt("pet_id", 0);
    if (!id) {
      id = esp_random() | 1;
      storagePutInt("pet_id", id);
    }
  }
  return id;
}

static int mine() { return amLeft ? 0 : 1; }  // which of the room's two Pets is this device's
static int16_t offset() { return amLeft ? 0 : meet::SCREEN_W; }

// --- drawing the room ---

static void drawBang(int16_t x, int16_t y) {
  display.fillRect(x, y, 5, 13, BLACK);
  display.fillRect(x, y + 16, 5, 5, BLACK);
}

// One of the room's Pets as `a` says, if any of it is on this screen. Its tag and bubble only when its middle is
// here (the other screen draws them otherwise).
static void drawPet(int i, const meet::Actor &a, bool blink) {
  const int16_t sx = a.x - offset(), y = PY + a.dy, mid = sx + SIZE / 2;
  if (sx <= -SIZE || sx >= display.width()) return;
  const bool me = i == mine();
  const String myName = petName();
  const char *name = me ? myName.c_str() : partner.name;
  const PetMood mood = a.mood == meet::HAPPY ? PetMood::Happy : blink ? PetMood::Blink : PetMood::Normal;
  petDraw(me ? petLookBits() : partner.look, SCALE, sx, y, mood);
  if (mid < 0 || mid >= display.width()) return;
  petDrawTag(name, mid, y - 3);
  if (a.bang) drawBang(sx + SIZE - 4, y - 24);
  if (a.heart) petDrawHeart(sx + SIZE + 2, y + 14, a.heart);
  if (a.bubble) {
    char text[24];
    const char *other = me ? partner.name : myName.c_str();
    if (a.bubble == meet::SAY_HI_NAME) snprintf(text, sizeof text, "Hi %s!", *other ? other : "friend");
    else strlcpy(text, meet::PHRASES[(a.bubble - meet::FIRST_PHRASE) % meet::PHRASE_COUNT], sizeof text);
    petDrawBubble(text, mid, y - 19);
  }
}

static void drawHeaderConnected() {
  char where[24];
  const char *name = partner.name[0] ? partner.name : "friend";
  if (amLeft) snprintf(where, sizeof where, "%s ->", name);  // which way round to hold them
  else snprintf(where, sizeof where, "<- %s", name);
  drawHeader("Meet", where);
}

static void drawActFrame() {
  drawHeaderConnected();
  for (int i = 0; i < 2; i++) drawPet(i, current->pet[i], false);
}

// --- acts ---

// Plays an act: the frames on a beat counted from startAt, so two devices that start together stay in step.
static void play(uint8_t act, uint8_t actor, uint32_t seed, uint32_t startAt) {
  script.build(act, actor, seed, pos[0], pos[1]);
  if (!devManualFrames()) {  // test build (X MANUAL 1): straight to the end, for repeatable screenshots
    for (int k = 0; k < script.count; k++) {
      while ((int32_t)(millis() - (startAt + k * BEAT_MS)) < 0) {
#if UNIDEX_DEV
        devShotDuringAnimation();
#endif
        delay(5);
      }
      current = &script.frames[k];
      displayFrame(drawActFrame);
#if UNIDEX_DEV
      Serial.printf("XW %d\n", k);  // test build: each frame as it shows, so two boards' timing can be compared
      if (Serial) Serial.flush();
#endif
    }
    current = nullptr;
  }
  pos[0] = script.x[0], pos[1] = script.x[1];
  if (!devManualFrames()) displayClean(meetDraw);  // drives every pixel: no trail left behind
  nearby.touchAll(millis());  // neither device sent anything meanwhile: that isn't "gone quiet"
  lastActAt = millis();
  showGap = 8000 + esp_random() % 4000;
}

static void send(uint8_t type, uint8_t act = 0, uint8_t actor = 0, uint32_t seed = 0) {
  meet::Message m = {type, myId(), partner.id, seed, act, actor};
  uint8_t packet[24];
  const size_t n = meet::encodeMessage(m, packet);
  for (int i = 0; i < 3; i++, delay(15)) linkBroadcast(packet, n);  // three times: one may get lost
}

// The left device starts an act for both; the right one asks it to.
static void startAct(uint8_t act, uint8_t actor) {
  if (!amLeft) {
    send(meet::ASK, act, actor);
    return;
  }
  const uint32_t seed = esp_random();
  send(meet::ACT, act, actor, seed);
  play(act, actor, seed, millis() + LEAD_MS);
}

static bool swapped() { return pos[0] >= meet::SCREEN_W; }

static void connect(const meet::Nearby &who) {
  partner = who;
  connected = true;
  amLeft = myId() < who.id;
  pos[0] = meet::HOME[0], pos[1] = meet::HOME[1];
  partedSince = 0;
  lastActAt = millis();  // also: the other copies of the CONNECT are old news from here
  showGap = 3000;  // the greeting comes first
  if (amLeft) startAct(meet::GREET, 0);
}

// They were parted: each Pet goes home on its own screen (no need to stay in step: the other can't see this one).
static void disconnect() {
  if (swapped()) {
    const uint32_t seed = esp_random();
    play(meet::SWAP, 0, seed, millis());
  }
  connected = false;
  pos[0] = meet::HOME[0], pos[1] = meet::HOME[1];
}

// --- radio ---

static void start() {
  nearby.clear();
  stopped = connected = false;
  error = linkStart();
  lastPress = millis();
  nextHelloAt = 0;
}

void meetEnter() { start(); }

void meetLeave() {
  connected = false;
  linkStop();
}

static void sayHello() {
  meet::Hello h = {};
  h.id = myId();
  h.look = petLookBits();
  strlcpy(h.name, petName().c_str(), sizeof h.name);
  const int best = nearby.strongest();  // tell it how strongly it's heard here, so both can use both directions
  if (best >= 0) h.heard = meet::packHeard(nearby.at(best).id, nearby.median(best));
  uint8_t packet[meet::HELLO_LEN];
  linkBroadcast(packet, meet::encodeHello(h, packet));
}

Redraw meetButton(Event e) {
  lastPress = millis();
  if (!linkOn()) {
    if (e != Event::BShort) return Redraw::None;
    start();  // look again
    return Redraw::Partial;
  }
  if (!connected) {
    const int i = nearby.firstInReach();
    if (e != Event::BShort || i < 0) return Redraw::None;
    partner = nearby.at(i);
    send(meet::CONNECT);
    connect(partner);
    return Redraw::None;
  }
  if (e == Event::AShort) startAct(meet::SAY, mine());
  else if (e == Event::BShort) startAct(swapped() ? meet::SWAP : meet::VISIT, mine());  // when swapped: go home
  else if (e == Event::BLong) startAct(meet::SWAP, mine());
  return Redraw::None;
}

Redraw meetTick() {
  if (!linkOn()) return Redraw::None;
  const uint32_t now = millis();
  if (now - lastPress > (connected ? STOP_CONNECTED_MS : STOP_MS) && !devManualFrames()) {  // save the battery
    linkStop();
    stopped = true;
    connected = false;
    nearby.clear();
    return Redraw::Partial;
  }
  if ((int32_t)(now - nextHelloAt) >= 0) {
    sayHello();
    // Faster while another device is around, so its distance is known within a second or two. A little jitter
    // keeps two devices from colliding.
    const uint32_t every = nearby.count() ? HELLO_MS / 4 : HELLO_MS;
    nextHelloAt = now + every - every / 10 + esp_random() % (every / 5);
  }
  bool changed = false;
  LinkPacket p;
  meet::Hello h;
  meet::Message m;
  bool gotAct = false, gotAsk = false;
  meet::Message act = {}, ask = {};
  uint32_t actAt = 0, connectFrom = 0;
  while (linkPoll(p)) {
    if (meet::decodeHello(p.data, p.len, h)) {
      if (h.id != myId()) changed |= nearby.heard(h, p.mac, now, p.rssi, myId());
    } else if (meet::decodeMessage(p.data, p.len, m) && m.to == myId() && (int32_t)(p.at - lastActAt) > 0) {
      // (Each message is sent three times; copies that came in before the last act ended are old news.)
      if (m.type == meet::CONNECT && !connected && nearby.find(m.from) >= 0) connectFrom = m.from;
      else if (m.type == meet::ACT && connected && m.from == partner.id && !gotAct)
        gotAct = true, act = m, actAt = p.at;
      else if (m.type == meet::ASK && connected && amLeft && m.from == partner.id) gotAsk = true, ask = m;
    }
  }
  changed |= nearby.forget(now);

  if (!connected) {
    if (connectFrom) {  // they pressed B with this device in reach
      connect(nearby.at(nearby.find(connectFrom)));
      return Redraw::None;
    }
  } else {
    if (gotAct) {  // from the left device: play it in step with it
      play(act.act, act.actor, act.seed, actAt + LEAD_MS);
      return Redraw::None;
    }
    if (gotAsk) {
      startAct(ask.act, ask.actor);
      return Redraw::None;
    }
    const int i = nearby.find(partner.id);
    if (i >= 0) {
      const bool look = nearby.at(i).look != partner.look || strcmp(nearby.at(i).name, partner.name);
      if (look) memcpy(partner.name, nearby.at(i).name, sizeof partner.name), partner.look = nearby.at(i).look;
      changed = look;
    }
    if (i >= 0 && nearby.parted(i)) {
      if (!partedSince) partedSince = now;
    } else {
      partedSince = 0;
    }
    if (i < 0 || (partedSince && now - partedSince > PARTED_MS)) {
      disconnect();
      return Redraw::Partial;
    }
    if (amLeft && now - lastActAt > showGap) {  // the show: something happens every 8-12 s
      const uint8_t pick = esp_random() % 6;
      if (swapped()) startAct(pick < 3 ? meet::SAY : meet::SWAP, esp_random() % 2);
      else startAct(pick < 2 ? meet::VISIT : pick == 2 ? meet::TRIP : pick == 3 ? meet::SWAP : meet::SAY,
                    esp_random() % 2);
      return Redraw::None;
    }
  }

  // Blink every few seconds.
#if UNIDEX_DEV
  if (devManualFrames()) return changed ? Redraw::Partial : Redraw::None;
#endif
  if (blinking && (int32_t)(now - openAt) >= 0) {
    blinking = false;
    nextBlinkAt = now + 2500 + esp_random() % 3500;
    return Redraw::Tick;
  }
  if (!blinking && (int32_t)(now - nextBlinkAt) >= 0) {
    if (nextBlinkAt == 0) {
      nextBlinkAt = now + 2500 + esp_random() % 3500;
    } else {
      blinking = true;
      openAt = now + 120;
      return Redraw::Tick;
    }
  }
  return changed ? Redraw::Partial : Redraw::None;
}

void meetDraw() {
  if (error || !linkOn()) {
    drawHeader("Meet");
    if (error) drawEmpty("Can't meet now", error);
    else drawEmpty("Meet stopped", stopped ? "No one came by." : "The radio is off.");
    drawHints("", "look again", "");
    return;
  }
  if (!connected) {
    const int reach = nearby.firstInReach(), near = reach >= 0 ? reach : nearby.strongest();
    drawHeader("Meet", reach >= 0 ? "press B" : near >= 0 ? "come closer" : "looking");
    if (near >= 0) {  // how close the nearest one is: 5 dots, all filled when it's in reach
      const int span = 60 + meet::NearbyList::REACH_DBM;  // -60 dBm: none filled; REACH_DBM: all five
      const int filled = reach >= 0 ? 5 : constrain((nearby.strength(near) + 60) * 5 / span, 0, 4);
      for (int d = 0; d < 5; d++) {
        const int16_t x = display.width() - MARGIN - 4 - (4 - d) * 11, y = CONTENT_TOP + 9;
        if (d < filled) display.fillCircle(x, y, 4, BLACK);
        else display.drawCircle(x, y, 3, BLACK);
      }
    }
    meet::Actor a = {};
    a.x = meet::HOME[0];
    amLeft = true;  // searching: your own screen is all there is (connect() sets the real side)
    drawPet(0, a, blinking);
    char meetWho[24] = "";
    if (reach >= 0) {
      const char *who = nearby.at(reach).name;
      snprintf(meetWho, sizeof meetWho, "meet %s", *who ? who : "them");
    }
    drawHints("", meetWho, "");
    return;
  }
  drawHeaderConnected();
  for (int i = 0; i < 2; i++) {
    meet::Actor a = {};
    a.x = pos[i];
    drawPet(i, a, blinking && i == mine());
  }
  drawHints("say", swapped() ? "home" : "visit", "swap");
}

const char *meetDetail() {
  static char buf[80];
  const int i = connected ? nearby.find(partner.id) : (nearby.count() ? 0 : -1);
  const int latest = i >= 0 && nearby.at(i).count ? nearby.at(i).readings[(nearby.at(i).count - 1) % 8] : 0;
  snprintf(buf, sizeof buf, "meet:%s:%d:%s:%s:%d,%d:%d:%d:%d", linkOn() ? "on" : "off", nearby.count(),
           connected ? "connected" : "searching", connected ? (amLeft ? "left" : "right") : "", pos[0], pos[1],
           i >= 0 ? nearby.strength(i) : 0, latest, nearby.firstInReach() >= 0);
  return buf;
}

#include "meet.h"
#include <esp_random.h>
#include "friends_logic.h"
#include "meet_logic.h"
#include "play_logic.h"
#include "send_logic.h"
#include "../badge/badge_file.h"
#include "pet.h"
#include "../../core/clock.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/input.h"
#include "../../core/launcher.h"
#include "../../core/link.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

// Searching: your Pet waits on its own screen. With another device in reach (up to about 30 cm), B connects the
// two, and from then on the two screens are one room, 400 px wide: the device whose Pet id is lower is the left
// half. The left device runs the show: it sends each act (meet_logic.h) and both play it on the same beat.

static const uint32_t HELLO_MS = 1000, LONELY_HELLO_MS = 2000, STOP_MS = 60000, STOP_CONNECTED_MS = 120000;
static const uint32_t BEAT_MS = 550, LEAD_MS = 1300;  // a frame a beat (a refresh takes ~0.4 s); then the start delay
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
static uint32_t partedSince, showGap;
static uint32_t lastActAt;  // when the last act ended (or they connected): message copies from before it are old news
static uint32_t calmSince;  // since when nothing has happened here: the show waits showGap after it
static meet::Script script;
static const meet::Frame *current;  // the frame being shown during an act

// A copy of the Pet's number and its friends on the SD card, so a board whose memory was wiped (Settings > Reset >
// Everything keeps them, but a full Install erases everything) still knows its friends, and they still know it.
static const char *const BACKUP = "/pet/friends.bin", *const BACKUP_TMP = "/pet/friends.tmp";
struct Backup {
  char magic[4];  // "UDXF"
  uint32_t id;
  friends::List list;
};

static bool readBackup(Backup &b) {
  if (!storageCardMount()) return false;
  fs::File f = storageOpen(BACKUP);
  if (!f || f.size() != sizeof b || f.read((uint8_t *)&b, sizeof b) != sizeof b) return false;
  return memcmp(b.magic, "UDXF", 4) == 0 && b.id && b.list.version == friends::VERSION && b.list.count <= friends::MAX;
}

// This device's Pet number: random, made the first time (or brought back from the card), kept in NVS. Sent instead
// of anything that identifies the device itself.
static uint32_t myId() {
  static uint32_t id;
  if (!id) {
    id = storageGetInt("pet_id", 0);
    if (!id) {
      Backup b;
      id = readBackup(b) ? b.id : esp_random() | 1;
      storagePutInt("pet_id", id);
    }
  }
  return id;
}

// Friends: asked after a first greeting ("Be friends?"); both must say yes. Kept in NVS ("pet_friends").
static friends::List buddies;
static bool buddiesLoaded, asking;  // asking: the "Be friends?" question is on screen
static uint8_t myAnswer, theirAnswer;  // 0 not yet, 1 yes, 2 not now (3 from them: yes, settled already)
static uint32_t nextAnswerAt;          // a yes is repeated until the question is settled
static uint32_t askedAt, nextQuestionAt;  // the left device repeats the question until the right one answers
static const uint32_t ASK_MS = 30000;  // the question goes away after this long
// The friends list (A on the searching screen)
static bool listing, confirmRemove;
static int listAt;

static void writeBackup();

static friends::List &friendsList() {
  if (!buddiesLoaded) {
    friends::List saved;
    Backup b;
    const bool onCard = readBackup(b);
    if (storageGetBytes("pet_friends", &saved, sizeof saved) == sizeof saved && saved.version == friends::VERSION) {
      buddies.load(&saved, sizeof saved);
      // Friends made before the card copy existed (or a card swapped in): copy them over now.
      if (buddies.count && (!onCard || b.id != myId() || memcmp(&b.list, &buddies, sizeof buddies) != 0))
        writeBackup();
    } else if (onCard && b.id == myId()) {  // the memory was wiped: back from the card
      buddies.load(&b.list, sizeof b.list);
      storagePutBytes("pet_friends", &buddies, sizeof buddies);
    }
    buddiesLoaded = true;
  }
  return buddies;
}

// Saved in NVS, and copied to the card (written whole to a temp file, then renamed, so a copy is never half written).
static void saveFriends() {
  storagePutBytes("pet_friends", &buddies, sizeof buddies);
  writeBackup();
}

static void writeBackup() {
  if (!storageCardMount()) return;
  Backup b;
  memcpy(b.magic, "UDXF", 4);
  b.id = myId();
  b.list = buddies;
  fs::File f = storageOpen(BACKUP_TMP, "w");
  if (!f) return;
  const bool ok = f.write((const uint8_t *)&b, sizeof b) == sizeof b;
  f.close();
  if (ok) storageReplace(BACKUP_TMP, BACKUP, sizeof b);
  else storageRemove(BACKUP_TMP);
}

void meetForgetFriends() {
  friendsList().count = 0;
  saveFriends();
}

static uint32_t clockNow() { return clockValid() ? (uint32_t)time(nullptr) : 0; }

// A friend met (again): counted, look and name brought up to date, saved.
static void meetFriend(const meet::Nearby &who) {
  friendsList().meet(who.id, who.look, who.name, clockNow());
  saveFriends();
}

static bool isFriend(uint32_t id) { return friendsList().find(id) >= 0; }

int meetFriendCount() { return friendsList().count; }

int meetFriends(char (*names)[13], uint32_t *lastMet, int max) {
  const friends::List &l = friendsList();
  const int n = min<int>(l.count, max);
  for (int i = 0; i < n; i++) {
    strlcpy(names[i], l.f[i].name, 13);
    lastMet[i] = l.f[i].lastMet;
  }
  return n;
}

// Sending badges to a friend (hold B in the room: a menu with "Swap screens" and "Send a badge").
static bool menuOpen, picking, waitingVerdict, receiving, previewing;
static int menuRow, pickAt, pickCount;
static String pickNames[32];
static uint32_t waitingSince, rxLast, partnerBusyUntil;
static send::Offer offer;      // what's coming in
static send::Receiver rx;
static uint8_t *rxBuf;         // its bytes (malloc'd for the transfer, freed after)
static Bmp rxBmp;              // the received badge, checked, for the preview

static bool busyHere() { return asking || menuOpen || picking || waitingVerdict || receiving || previewing; }

// The newer acts the other device knows (its FEATURES, kept from whoever sent them last: it can arrive before this
// side has connected). An act it hasn't announced is never started.
static uint32_t featuresFrom, featuresBits;
static uint32_t theirFeatures() { return featuresFrom == partner.id ? featuresBits : 0; }
static bool canPlay(uint8_t act) {
  if (act == meet::HUG || act == meet::HANDSHAKE) return theirFeatures() & meet::F_GREETINGS;
  if (act == meet::SLEEPOVER) return theirFeatures() & meet::F_SLEEPOVER;
  if (act == meet::HIGH_FIVE || act == meet::SLAP || act == meet::MISS) return theirFeatures() & meet::F_HIGH_FIVE;
  if (act == meet::CATCH || act == meet::TUG) return theirFeatures() & meet::F_GAMES;
  return act >= meet::GREET && act < meet::ACT_COUNT;
}
static bool greetDue;            // the left device greets once it knows what the other can do (or after a moment)
static uint32_t greetBy;
static bool roomAsleep;          // after a sleepover: both asleep side by side until something happens
static uint32_t ignoreBUntil;    // a B still down from a high five isn't also a VISIT
static int32_t myPress, theirPress;  // a high five: each press, in ms from "Now!" (meet::NO_PRESS: none)
// Hold B's menu: what the other Pet can play decides how many rows.
static const char *const MENU_ROWS[] = {"Swap screens", "Send a badge", "High five", "Catch", "Tug of war"};
static const uint8_t MENU_ACTS[] = {meet::SWAP, 0, meet::HIGH_FIVE, meet::CATCH, meet::TUG};  // (0: a badge)
static int menuRows() {
  return theirFeatures() & meet::F_GAMES ? 5 : theirFeatures() & meet::F_HIGH_FIVE ? 3 : 2;
}
static void rxFree() {
  free(rxBuf);
  rxBuf = nullptr;
  receiving = previewing = false;
}

// This device's Pet id: random, made the first time, kept in NVS. Sent instead of anything that identifies the
// device itself.

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
  const PetMood mood = a.mood == meet::HAPPY                 ? PetMood::Happy
                       : blink || a.mood == meet::SLEEPY       ? PetMood::Blink
                                                               : PetMood::Normal;
  petDraw(me ? petLookBits() : partner.look, SCALE, sx, y, mood);
  if (mid < 0 || mid >= display.width()) return;
  petDrawTag(name, mid, y - 3);
  if (a.bang) drawBang(sx + SIZE - 4, y - 24);
  if (a.heart) petDrawHeart(sx + SIZE + 2, y + 14, a.heart);
  if (a.bubble) {
    char text[24];
    const char *other = me ? partner.name : myName.c_str();
    if (a.bubble == meet::SAY_HI_NAME) snprintf(text, sizeof text, "Hi %s!", *other ? other : "friend");
    else if (a.bubble == meet::SAY_HI_AGAIN) snprintf(text, sizeof text, "Hi again %s!", *other ? other : "friend");
    else if (a.bubble == meet::SAY_FRIENDS) strlcpy(text, "Friends!", sizeof text);
    else if (meet::saying(a.bubble)) strlcpy(text, meet::saying(a.bubble), sizeof text);
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
  if (isFriend(partner.id)) {  // a little heart before the name: friends
    display.setFont(FONT_SMALL);
    petDrawHeart(display.width() - MARGIN - textWidth(where) - 9, 11, 1);
  }
}

static void drawActFrame() {
  drawHeaderConnected();
  for (int i = 0; i < 2; i++) drawPet(i, current->pet[i], false);
}

// --- acts ---

static void keepAlive();  // sends a HELLO when one is due, during a long animation or transfer (below)

// Plays an act: the frames on a beat counted from startAt, so two devices that start together stay in step.
static void pressMessage(const LinkPacket &p);
static void tell(uint8_t type, uint8_t act, uint8_t actor, uint32_t seed);
static void startAct(uint8_t act, uint8_t actor);

// A high five, while its frames play: B pressed (from a beat before "Now!") is this side's press; the right device
// sends it on (PRESS); the left one keeps the other's, to decide when the act ends.
static void highFiveListen(uint32_t nowAt) {
  static bool wasDown;
  const bool down = inputBHeld();
  const int32_t at = (int32_t)(millis() - nowAt);
  if (down && !wasDown && myPress == meet::NO_PRESS && at >= meet::PRESS_FROM_MS) {
    myPress = at;
    if (!amLeft) tell(meet::PRESS, meet::HIGH_FIVE, 0, (uint32_t)myPress);
  }
  wasDown = down;
  LinkPacket p;
  while (linkPoll(p)) pressMessage(p);  // (HELLOs are kept too)
}

// --- the games (catch, tug of war): live, on the fast waveform, until they end (play_logic.h) ---

static const uint32_t GAME_FRAME_MS = 140;  // ~7 frames a second, as Games
static duo::Game game;
static duo::Scene scene;  // the frame being drawn
static duo::Rallies rallies;  // the best catch rally with each Pet ("pet_rallies")
static bool ralliesLoaded;
static bool gameToast;  // how it went is in a toast: the room redraws once more to show it

static duo::Rallies &ralliesSaved() {
  if (!ralliesLoaded) {
    if (storageGetBytes("pet_rallies", &rallies, sizeof rallies) != sizeof rallies ||
        rallies.count > duo::Rallies::MAX)
      rallies = {};
    ralliesLoaded = true;
  }
  return rallies;
}

static void drawBall(int16_t x, int16_t y) {
  if (x < -6 || x > display.width() + 6) return;
  display.fillCircle(x, y, 5, BLACK);
  display.fillCircle(x - 2, y - 2, 1, WHITE);  // a shine
}

// The rope between their paws, and its knot (the gap between the screens is the middle).
static void drawRope() {
  const int16_t y = PY + duo::PAW_Y, o = offset();
  const int16_t from = scene.pet[0].x + meet::PET_W - 6 - o, to = scene.pet[1].x + 6 - o;
  display.fillRect(from, y - 1, to - from, 3, BLACK);
  display.fillCircle(scene.knotX - o, y, 5, BLACK);
  display.drawCircle(scene.knotX - o, y, 7, WHITE);
}

static void drawGameFrame() {
  char right[24] = "";
  if (game.act == meet::CATCH) {
    if (game.over) snprintf(right, sizeof right, "best %d", scene.best);
    else if (scene.best > scene.rally) snprintf(right, sizeof right, "%d, best %d", scene.rally, scene.best);
    else snprintf(right, sizeof right, "rally %d", scene.rally);
  } else if (scene.secondsLeft >= 0) {
    snprintf(right, sizeof right, "%ds", scene.secondsLeft);
  }
  drawHeader(game.act == meet::CATCH ? "Catch" : "Tug of war", right);
  if (scene.rope) drawRope();
  for (int i = 0; i < 2; i++) drawPet(i, scene.pet[i], false);
  if (scene.ball) drawBall(scene.ballX - offset(), PY + scene.ballY);
  drawHints("", game.act == meet::CATCH ? "catch" : "pull", "");
}

static void tellGame(const duo::Out &o, int copies) {
  meet::Message m = {meet::GAME, myId(), partner.id, o.value, o.ev, o.actor};
  uint8_t packet[24];
  const size_t n = meet::encodeMessage(m, packet);
  for (int i = 0; i < copies; i++) {
    if (i) delay(4);
    linkBroadcast(packet, n);
  }
}

// Plays a game from startAt (both devices, as an act): B presses go to the game as they happen, the other's GAME
// packets as they come, a frame every GAME_FRAME_MS; holding A ends it for both.
static void runGame(uint8_t act, uint32_t seed, uint32_t startAt) {
  game.begin(act, seed, mine(), pos[0], pos[1]);
  inputCountB(true);
  displayFastFrames(true);
  uint32_t nextFrameAt = startAt, resendAt = 0;
#if UNIDEX_DEV
  int announced = -1;  // the test build says when each throw to this Pet leaves, so a test can catch it
#endif
  for (;;) {
    const uint32_t now = millis();
    const int32_t t = (int32_t)(now - startAt);
    uint32_t at[8];
    for (int i = 0, n = inputTakeBPresses(at, 8); i < n; i++) game.press((int32_t)(at[i] - startAt));
    LinkPacket p;
    meet::Hello h;
    meet::Message m;
    while (linkPoll(p)) {
      if (meet::decodeHello(p.data, p.len, h)) {
        if (h.id != myId()) nearby.heard(h, p.mac, millis(), p.rssi, myId());
      } else if (meet::decodeMessage(p.data, p.len, m) && m.type == meet::GAME && m.to == myId() &&
                 m.from == partner.id) {
        game.heard(m.act, m.actor, m.seed, (int32_t)(p.at - startAt));
      }
    }
    const Event e = inputPoll();
    if (e == Event::ALong || e == Event::Reset) game.leave(t);
    game.tick(t);
    duo::Out o;
    if (game.takeOut(o)) tellGame(o, 3), resendAt = millis() + 200;
    else if ((int32_t)(now - resendAt) >= 0 && game.latest(o)) tellGame(o, 1), resendAt = now + 200;
    if (game.done(t)) break;
#if UNIDEX_DEV
    if (act == meet::CATCH && !game.over && game.cur.to() == mine() && t >= game.cur.at && announced != game.k) {
      announced = game.k;
      Serial.printf("XG throw %d arrives %ld\n", game.k, (long)(game.cur.arrives() - t));
      if (Serial) Serial.flush();
    }
#endif
    if (t >= 0 && (int32_t)(now - nextFrameAt) >= 0) {
      game.scene(t, scene);
      displayFrame(drawGameFrame);
      nextFrameAt = now + GAME_FRAME_MS;
#if UNIDEX_DEV
      Serial.printf("XG t=%ld k=%d rally=%d best=%d drops=%d pulls=%d,%d shift=%d winner=%d over=%d\n", (long)t,
                    game.k, game.rally, game.best, game.drops, game.pulls[0], game.pulls[1], game.shift(),
                    game.winner, game.over);
      if (Serial) Serial.flush();
#endif
    }
    keepAlive();
#if UNIDEX_DEV
    devShotDuringAnimation();
#endif
    delay(2);
  }
  inputCountB(false);
  displayFastFrames(false);
#if UNIDEX_DEV
  Serial.printf("XG end rally=%d best=%d winner=%d quit=%d\n", game.rally, game.best, game.winner, game.quit);
#endif
  char text[40];
  const char *who = partner.name[0] ? partner.name : "them";
  if (game.quit) return;
  if (act == meet::CATCH && game.best > 0) {
    if (ralliesSaved().record(partner.id, game.best)) {
      storagePutBytes("pet_rallies", &rallies, sizeof rallies);
      snprintf(text, sizeof text, "New best with %s: %d!", who, game.best);
    } else {
      snprintf(text, sizeof text, "Best rally: %d", game.best);
    }
    launcherToast(text);
    gameToast = true;
  } else if (act == meet::TUG && game.winner >= 0) {
    if (game.winner == 2) strlcpy(text, "A tie!", sizeof text);
    else if (game.winner == mine()) strlcpy(text, "You won!", sizeof text);
    else snprintf(text, sizeof text, "%s won!", partner.name[0] ? partner.name : "They");
    launcherToast(text);
    gameToast = true;
  }
}

static uint8_t lastAct;  // for the test build's detail

static void play(uint8_t act, uint8_t actor, uint32_t seed, uint32_t startAt) {
  lastAct = act;
  script.build(act, actor, seed, pos[0], pos[1]);
  roomAsleep = false;
  const bool aGame = act == meet::CATCH || act == meet::TUG;
  if (aGame) {
    if (!devManualFrames()) runGame(act, seed, startAt);
    script.x[0] = meet::HOME[0], script.x[1] = meet::HOME[1];  // back home after
  }
  const bool highFive = act == meet::HIGH_FIVE;
  const uint32_t nowAt = startAt + script.nowBeat * BEAT_MS;  // (a high five's "Now!")
  if (highFive) myPress = theirPress = meet::NO_PRESS;
  if (!devManualFrames() && !aGame) {  // test build (X MANUAL 1): straight to the end, for repeatable screenshots
    for (int k = 0; k < script.count; k++) {
      while ((int32_t)(millis() - (startAt + k * BEAT_MS)) < 0) {
#if UNIDEX_DEV
        devShotDuringAnimation();
#endif
        keepAlive();  // so the other device never thinks this one has gone, even if it isn't playing along
        if (highFive) highFiveListen(nowAt);
        delay(5);
      }
      current = &script.frames[k];
      displayFrame(drawActFrame);
#if UNIDEX_DEV
      Serial.printf("XW %d%s\n", k, highFive && k == script.nowBeat ? " now" : "");  // test build: each frame as it
      // shows, so two boards' timing can be compared (and a test can press in time for a high five)
      if (Serial) Serial.flush();
#endif
    }
    current = nullptr;
    if (highFive) {  // the last beat ends the window: a press just after it still counts
      const uint32_t until = startAt + script.count * BEAT_MS;
      while ((int32_t)(millis() - until) < 0) highFiveListen(nowAt), delay(5);
    }
  }
  pos[0] = script.x[0], pos[1] = script.x[1];
  if (!devManualFrames()) displayClean(meetDraw);  // drives every pixel: no trail left behind
  nearby.touchAll(millis());  // neither device sent anything meanwhile: that isn't "gone quiet"
  lastActAt = calmSince = millis();
  showGap = 8000 + esp_random() % 4000;
  // Friends or not is decided by the left device's greeting, so the two sides can never disagree for long: a
  // friends' greeting adds the friend here if this side had lost it; a strangers' greeting asks both again.
  if ((act == meet::GREET_FRIEND || act == meet::HUG || act == meet::HANDSHAKE || act == meet::SLEEPOVER) &&
      !isFriend(partner.id))
    meetFriend(partner);
  if (act == meet::SLEEPOVER) roomAsleep = true;  // and they stay asleep (drawRoomOnly) until something happens
  if (highFive) {
    ignoreBUntil = millis() + 1000;
    if (amLeft) startAct(meet::highFive(myPress, theirPress), 0);  // the left side decides how it went
  }
  if (act == meet::FRIENDS) {  // the celebration settles it here too, even if their yes never arrived
    if (!isFriend(partner.id)) meetFriend(partner);
    asking = false;
  }
  if (act == meet::GREET && !asking) {  // a first meeting: ask both owners (unless their QUESTION already did)
    asking = true;
    myAnswer = theirAnswer = 0;
    askedAt = nextQuestionAt = millis();
    if (!devManualFrames()) displayClean(meetDraw);  // the question on screen
  }
}

static void tell(uint8_t type, uint8_t act = 0, uint8_t actor = 0, uint32_t seed = 0);
static void tell(uint8_t type, uint8_t act, uint8_t actor, uint32_t seed) {
  meet::Message m = {type, myId(), partner.id, seed, act, actor};
  uint8_t packet[24];
  const size_t n = meet::encodeMessage(m, packet);
  for (int i = 0; i < 3; i++, delay(15)) linkBroadcast(packet, n);  // three times: one may get lost
}

// The left device starts an act for both; the right one asks it to.
static void startAct(uint8_t act, uint8_t actor) {
  if (!canPlay(act)) return;  // one the other device doesn't know
  if (!amLeft) {
    tell(meet::ASK, act, actor);
    return;
  }
  const uint32_t seed = esp_random();
  tell(meet::ACT, act, actor, seed);
  play(act, actor, seed, millis() + LEAD_MS);
}

static bool swapped() { return pos[0] >= meet::SCREEN_W; }

// since: when this meeting began (this side's first CONNECT sent, or theirs received). Anything that came in after
// it is news: the other device may greet at once, before this side has finished sending its CONNECT copies (an
// older left device doesn't wait), and that greeting mustn't be taken for an old copy and dropped.
static void connect(const meet::Nearby &who, uint32_t since) {
  partner = who;
  connected = true;
  amLeft = myId() < who.id;
  pos[0] = meet::HOME[0], pos[1] = meet::HOME[1];
  partedSince = 0;
  lastActAt = since;  // copies of the CONNECT itself (type CONNECT) are ignored once connected anyway
  calmSince = millis();
  showGap = 3000;  // the greeting comes first
  asking = listing = confirmRemove = false;
  roomAsleep = false;
  // Friends if either side has the other (its FEATURES may already be here): one that forgot adds it back
  const bool friendsAlready = meet::friendsNow(isFriend(who.id), featuresFrom == who.id ? featuresBits : 0);
  if (friendsAlready) meetFriend(who);  // met again: counted on each side
  tell(meet::FEATURES, 0, 0, meet::MY_FEATURES | (friendsAlready ? meet::F_FRIEND : 0));  // what this side knows
  if (amLeft) greetDue = true, greetBy = millis() + 800;  // greets once it knows theirs (or soon anyway)
}

// The left device's greeting: strangers meet, friends meet again (a wave, a hug, the secret handshake, or at night
// a sleepover).
static uint8_t greeting() {
  if (!isFriend(partner.id)) return meet::GREET;
  int met = 1;
  const friends::List &l = friendsList();
  for (int i = 0; i < l.count; i++)
    if (l.f[i].id == partner.id) met = l.f[i].met;
  const struct tm t = clockLocal();
  const bool night = clockValid() && meet::isNight(t.tm_hour * 60 + t.tm_min);
  return meet::friendsGreeting(met, night, theirFeatures());
}

// The right device tells the left one when it's busy (a menu, choosing a badge, a preview), so the show waits.
static void tellBusy() {
  static bool told;
  const bool busy = busyHere();
  if (amLeft || busy == told) return;
  told = busy;
  tell(meet::ASK, 0, busy);  // act 0: not an act, "busy" (actor 1) or "free" (0)
}

// A packet that comes in during a high five: a HELLO is kept (as meetTick does); the other's PRESS, on the left
// device, is its press; anything else waits for the room (it's dropped, as during any act).
static void pressMessage(const LinkPacket &p) {
  meet::Hello h;
  meet::Message m;
  if (meet::decodeHello(p.data, p.len, h)) {
    if (h.id != myId()) nearby.heard(h, p.mac, millis(), p.rssi, myId());
  } else if (meet::decodeMessage(p.data, p.len, m) && m.type == meet::PRESS && m.to == myId() &&
             m.from == partner.id && amLeft && theirPress == meet::NO_PRESS) {
    theirPress = (int32_t)m.seed;
  }
}

// Sends the chosen badge: an offer, then piece by piece, each acknowledged (send_logic.h). Blocks for about a
// second. Returns a reason it failed, or nullptr.
static const char *sendBadge(const char *name) {
  Bmp b;
  if (!badgeLoad(name, b)) return "That badge can't be read";
  const int i = nearby.find(partner.id);
  if (i < 0) return "They've gone";
  const uint8_t *mac = nearby.at(i).mac;
  uint8_t pkt[250];
  // Waits up to ms for an ACK from the partner; keeps hearing HELLOs meanwhile. Returns its index, or -1.
  auto waitAck = [&](uint32_t ms) -> int {
    const uint32_t until = millis() + ms;
    LinkPacket p;
    meet::Hello h;
    uint32_t from, to;
    uint16_t idx;
    while ((int32_t)(millis() - until) < 0) {
      keepAlive();
      if (!linkPoll(p)) {
        delay(2);
        continue;
      }
      if (meet::decodeHello(p.data, p.len, h) && h.id != myId()) nearby.heard(h, p.mac, millis(), p.rssi, myId());
      else if (send::decodeAck(p.data, p.len, from, to, idx) && from == partner.id && to == myId()) return idx;
    }
    return -1;
  };
  send::Offer o = {myId(), partner.id, (uint32_t)b.size, send::crc32(0, b.data, b.size), ""};
  strlcpy(o.name, name, sizeof o.name);
  int answer = -1;
  for (int tries = 0; tries < 8 && answer != send::READY && answer != send::BUSY; tries++) {
    linkSend(mac, pkt, send::encodeOffer(o, pkt));
    answer = waitAck(250);
  }
  if (answer == send::BUSY) return "They're busy: try again";
  if (answer != send::READY) return "No answer: try again";
  for (int piece = 0; piece < send::pieces(b.size); piece++) {
    bool acked = false;
    for (int tries = 0; tries < send::TRIES && !acked; tries++) {
      linkSend(mac, pkt, send::encodeData(myId(), partner.id, piece, b.data, b.size, pkt));
      for (int ack; !acked && (ack = waitAck(send::ACK_WAIT_MS)) >= 0;) acked = ack == piece;
    }
    if (!acked) return "It didn't get through: try again";
  }
  nearby.touchAll(millis());
  return nullptr;
}

static void drawSheetFrame();  // the room with the current sheet over it (below)

// A badge coming in from the friend in the room: the offer, its pieces, and what they did with ours.
static void receivePacket(const LinkPacket &p, uint32_t now, bool &changed) {
  uint8_t pkt[24];
  send::Offer o;
  send::Piece d;
  uint32_t from, to;
  uint8_t verdict;
  if (send::decodeOffer(p.data, p.len, o) && o.from == partner.id && o.to == myId()) {
    const bool ok = isFriend(o.from) && !busyHere() && (rxBuf = (uint8_t *)malloc(o.size));
    if (ok) {
      offer = o;
      rx.begin(rxBuf, o.size, o.crc);
      receiving = true;
      rxLast = now;
      changed = true;
    }
    linkSend(p.mac, pkt, send::encodeAck(myId(), o.from, ok ? send::READY : send::BUSY, pkt));
  } else if (send::decodeData(p.data, p.len, d) && receiving && d.from == partner.id && d.to == myId()) {
    const int ack = rx.take(d);
    rxLast = now;
    if (ack >= 0) linkSend(p.mac, pkt, send::encodeAck(myId(), d.from, ack, pkt));
    if (rx.complete()) {
      receiving = false;
      if (rx.intact() && bmpCheck(rxBuf, rx.size, rxBmp)) {
        previewing = true;  // shown full screen: B keep, A no thanks
      } else {
        for (int i = 0; i < 3; i++, delay(15))
          linkBroadcast(pkt, send::encodeVerdict(myId(), partner.id, send::FAILED, pkt));
        rxFree();
        launcherToast("It arrived damaged");
      }
      tellBusy();
      changed = true;
    }
  } else if (send::decodeVerdict(p.data, p.len, from, to, verdict) && waitingVerdict && from == partner.id &&
             to == myId()) {
    waitingVerdict = false;
    char text[40];
    const char *who = partner.name[0] ? partner.name : "They";
    const char *what = verdict == send::KEPT        ? "%s kept it!"
                       : verdict == send::NO_THANKS ? "%s: no thanks"
                                                    : "%s: not saved";
    snprintf(text, sizeof text, what, who);
    launcherToast(text);
    tellBusy();
    changed = true;
  }
}

// Both said yes: saved as friends on this side; the left device starts the celebration for both.
static void becomeFriends() {
  asking = false;
  meetFriend(partner);
  if (amLeft) startAct(meet::FRIENDS, theirFeatures() & meet::F_QUICK_GREET ? 1 : 0);  // (quick if it knows how)
}

// They were parted: each Pet goes home on its own screen (no need to stay in step: the other can't see this one).
static void disconnect() {
  if (swapped()) {
    const uint32_t seed = esp_random();
    play(meet::SWAP, 0, seed, millis());
  }
  connected = asking = menuOpen = picking = waitingVerdict = greetDue = roomAsleep = false;
  featuresFrom = featuresBits = 0;  // the next meeting starts afresh (a stale "not friends" mustn't decide it)
  rxFree();
  pos[0] = meet::HOME[0], pos[1] = meet::HOME[1];
  char text[32];
  snprintf(text, sizeof text, "%s went home", partner.name[0] ? partner.name : "Your friend");
  launcherToast(text);
}

// --- radio ---

static void start() {
  nearby.clear();
  featuresFrom = featuresBits = 0;
  stopped = connected = false;
  error = linkStart();
  lastPress = millis();
  nextHelloAt = 0;
}

void meetEnter() { start(); }

void meetLeave() {
  connected = asking = listing = confirmRemove = menuOpen = picking = waitingVerdict = false;
  rxFree();
  linkStop();
}

bool meetBack() {
  if (menuOpen) menuOpen = false;
  else if (picking) picking = false;
  else if (previewing) {  // hold A on a badge someone sent: no thanks
    uint8_t pkt[16];
    linkBroadcast(pkt, send::encodeVerdict(myId(), partner.id, send::NO_THANKS, pkt));
    rxFree();
  } else if (confirmRemove) confirmRemove = false;
  else if (listing) listing = false;
  else return false;
  return true;
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

static void keepAlive() {
  powerAlive();  // the loop watchdog: an act or a badge transfer can take longer than it allows
  if ((int32_t)(millis() - nextHelloAt) < 0) return;
  sayHello();
  nextHelloAt = millis() + HELLO_MS / 4;
}

Redraw meetButton(Event e) {
  lastPress = millis();
  if (!linkOn()) {
    if (e != Event::BShort) return Redraw::None;
    start();  // look again
    return Redraw::Partial;
  }
  if (listing) {  // the friends list
    friends::List &l = friendsList();
    if (confirmRemove) {
      if (e == Event::BShort) {
        l.remove(listAt);
        saveFriends();
        confirmRemove = false;
        if (listAt >= l.count) listAt = 0;
        if (!l.count) listing = false;
      } else if (e == Event::AShort) {
        confirmRemove = false;
      }
      return Redraw::Partial;
    }
    if (e == Event::AShort && l.count) listAt = (listAt + 1) % l.count;
    else if (e == Event::BLong && l.count) confirmRemove = true;
    else return Redraw::None;
    return Redraw::Partial;
  }
  if (!connected) {
    if (e == Event::AShort && friendsList().count) {
      listing = true;
      listAt = 0;
      return Redraw::Partial;
    }
    const int i = nearby.firstInReach();
    if (e != Event::BShort || i < 0) return Redraw::None;
    partner = nearby.at(i);
    const uint32_t since = millis();
    tell(meet::CONNECT);
    connect(partner, since);
    return Redraw::None;
  }
  if (menuOpen) {  // hold B's menu: "Swap screens" / "Send a badge"
    if (e == Event::AShort) {
      menuRow = (menuRow + 1) % menuRows();
    } else if (e == Event::BShort) {
      menuOpen = false;
      if (MENU_ACTS[menuRow]) {
        tellBusy();
        startAct(MENU_ACTS[menuRow], mine());
        return Redraw::None;
      }
      if (!isFriend(partner.id)) {
        launcherToast("Only friends swap badges");
      } else if (!(pickCount = badgeList(pickNames, 32))) {
        launcherToast("No badges on the card");
      } else {
        picking = true;
        pickAt = 0;
        tellBusy();
        return Redraw::Full;  // a badge fills the screen
      }
    } else {
      return Redraw::None;
    }
    tellBusy();
    return Redraw::Partial;
  }
  if (picking) {  // choosing a badge to send
    if (e == Event::AShort) {
      pickAt = (pickAt + 1) % pickCount;
      return Redraw::Partial;
    }
    if (e != Event::BShort) return Redraw::None;
    picking = false;
    waitingVerdict = true;  // (shows "Sending..." while it goes)
    displayFrame(drawSheetFrame);
    const char *problem = sendBadge(pickNames[pickAt].c_str());
    if (problem) {
      waitingVerdict = false;
      launcherToast(problem);
    } else {
      waitingSince = millis();
    }
    tellBusy();
    return Redraw::Full;
  }
  if (previewing) {  // a badge from a friend: B keep, A no thanks
    if (e != Event::BShort && e != Event::AShort) return Redraw::None;
    uint8_t verdict = send::NO_THANKS;
    if (e == Event::BShort) {
      const String name = badgeFreeName(badgeNameOk(offer.name) ? offer.name : "from-a-friend.bmp");
      if (!storageCardMount()) {
        verdict = send::FAILED;
        launcherToast("Needs an SD card");
      } else if (!badgeSave(name.c_str(), rxBuf, rx.size)) {
        verdict = send::FAILED;
        launcherToast("Couldn't save it");
      } else {
        verdict = send::KEPT;
        launcherToast(("Kept: " + name).c_str());
      }
    }
    uint8_t pkt[16];
    for (int i = 0; i < 3; i++, delay(15)) linkBroadcast(pkt, send::encodeVerdict(myId(), partner.id, verdict, pkt));
    rxFree();
    tellBusy();
    return Redraw::Full;
  }
  if (waitingVerdict || receiving) return Redraw::None;
  if (asking) {  // "Be friends?": B yes, A not now
    if (myAnswer || (e != Event::BShort && e != Event::AShort)) return Redraw::None;
    myAnswer = e == Event::BShort ? 1 : 2;
    tell(meet::ANSWER, myAnswer);
    nextAnswerAt = millis() + 300;
    calmSince = millis();  // the room waits its usual gap after the question before playing by itself
    if (myAnswer == 2) asking = false;
    else if (theirAnswer == 1) becomeFriends();
    return Redraw::Partial;
  }
  if (e == Event::BShort && (int32_t)(millis() - ignoreBUntil) < 0) return Redraw::None;
  if (e == Event::AShort) startAct(meet::SAY, mine());
  else if (e == Event::BShort) startAct(swapped() ? meet::SWAP : meet::VISIT, mine());  // when swapped: go home
  else if (e == Event::BLong) {  // the menu: swap, or send a badge
    menuOpen = true;
    menuRow = 0;
    tellBusy();
    return Redraw::Partial;
  }
  return Redraw::None;
}

Redraw meetTick() {
  if (!linkOn()) return Redraw::None;
  if (gameToast) {
    gameToast = false;
    return Redraw::Partial;
  }
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
    // keeps two devices from colliding. Slower while nobody is: the first one heard is still found within 2 s.
    const uint32_t every = nearby.count() ? HELLO_MS / 4 : LONELY_HELLO_MS;
    nextHelloAt = now + every - every / 10 + esp_random() % (every / 5);
  }
  bool changed = false, badgeNews = false;  // badgeNews: a badge arrived, or their answer to ours
  bool questionHere = false;                 // the left device's QUESTION opened "Be friends?" here
  LinkPacket p;
  meet::Hello h;
  meet::Message m;
  bool gotAct = false, gotAsk = false;
  meet::Message act = {}, ask = {};
  uint32_t actAt = 0, connectFrom = 0, connectAt = 0;
  while (linkPoll(p)) {
    if (meet::decodeMessage(p.data, p.len, m) && m.type == meet::FEATURES && m.to == myId()) {
      featuresFrom = m.from, featuresBits = m.seed;  // (before or after connecting)
      if (connected && m.from == partner.id && (m.seed & meet::F_FRIEND) && !isFriend(partner.id))
        meetFriend(partner), changed = true;  // they remember being friends: so does this side again
      continue;
    }
    if (meet::decodeMessage(p.data, p.len, m) && m.type == meet::QUESTION && m.to == myId() && connected &&
        m.from == partner.id && !amLeft) {
      if (isFriend(partner.id)) {
        tell(meet::ANSWER, 3);  // friends already: settled
      } else if (!asking) {  // "Be friends?" here too, even if the greeting never arrived
        asking = true;
        myAnswer = theirAnswer = 0;
        askedAt = now;
        questionHere = true;
      }
      continue;
    }
    if (meet::decodeHello(p.data, p.len, h)) {
      if (h.id != myId()) changed |= nearby.heard(h, p.mac, now, p.rssi, myId());
    } else if (meet::decodeMessage(p.data, p.len, m) && m.to == myId() && (int32_t)(p.at - lastActAt) > 0) {
      // (Each message is sent three times; copies that came in before the last act ended are old news.)
      if (m.type == meet::CONNECT && !connected && nearby.find(m.from) >= 0) connectFrom = m.from, connectAt = p.at;
      else if (m.type == meet::ACT && connected && m.from == partner.id && !gotAct)
        gotAct = true, act = m, actAt = p.at;
      else if (m.type == meet::ASK && connected && amLeft && m.from == partner.id && m.act == 0)
        partnerBusyUntil = m.actor ? now + 60000 : 0, calmSince = now;  // the right one is busy, or done
      else if (m.type == meet::ASK && connected && amLeft && m.from == partner.id) gotAsk = true, ask = m;
      else if (m.type == meet::ANSWER && connected && m.from == partner.id) {  // (FEATURES: below)
        theirAnswer = m.act == 3 ? 1 : m.act;
        if (m.act == 1 && !asking && isFriend(partner.id)) tell(meet::ANSWER, 3);  // they missed ours: settled
      }
    } else if (connected) {
      receivePacket(p, now, badgeNews);
    }
  }
  changed |= nearby.forget(now);
  static bool wasBusy;  // a menu, a badge or a question just ended here: the show waits its usual gap again
  if (wasBusy && !busyHere()) calmSince = now;
  wasBusy = busyHere();

  if (!connected) {
    if (connectFrom) {  // they pressed B with this device in reach
      connect(nearby.at(nearby.find(connectFrom)), connectAt - 1);
      return Redraw::None;
    }
  } else {
    if (questionHere) return Redraw::Full;
    if (asking) {
      if (amLeft && !theirAnswer && (int32_t)(now - nextQuestionAt) >= 0) {  // ask the other side until it answers
        tell(meet::QUESTION);
        nextQuestionAt = now + 650 + esp_random() % 100;
      }
      if (theirAnswer == 2 || now - askedAt > ASK_MS) {  // "not now" from them, or no answer for a while
        asking = false;
        calmSince = now;
        return Redraw::Partial;
      }
      if (myAnswer == 1 && theirAnswer == 1) {
        becomeFriends();
        return amLeft ? Redraw::None : Redraw::Partial;
      }
      if (myAnswer == 1 && (int32_t)(now - nextAnswerAt) >= 0) {  // say yes again, in case it got lost
        tell(meet::ANSWER, 1);
        nextAnswerAt = now + 250 + esp_random() % 150;
      }
    }
    if (badgeNews) return previewing ? Redraw::Full : Redraw::Partial;  // a badge fills the screen
    if (gotAct) {  // from the left device: play it in step with it
      play(act.act, act.actor, act.seed, actAt + LEAD_MS);
      return Redraw::None;
    }
    if (gotAsk) {
      startAct(ask.act, ask.actor);
      return Redraw::None;
    }
    if (greetDue && (featuresFrom == partner.id || (int32_t)(now - greetBy) >= 0)) {
      greetDue = false;
      const uint8_t g = greeting();  // (strangers: the quick greeting when the other knows it)
      startAct(g, g == meet::GREET && (theirFeatures() & meet::F_QUICK_GREET) ? 1 : 0);
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
    if (receiving && now - rxLast > 3000) {  // the pieces stopped coming
      rxFree();
      launcherToast("The badge didn't arrive");
      tellBusy();
      return Redraw::Partial;
    }
    if (waitingVerdict && now - waitingSince > 40000) {  // no answer from them
      waitingVerdict = false;
      tellBusy();
      return Redraw::Partial;
    }
    if (amLeft && !busyHere() && !greetDue && !roomAsleep && (int32_t)(now - partnerBusyUntil) >= 0 &&
        now - calmSince > showGap) {
      // the show: something happens every 8-12 s (not while they sleep); now and then a high five to join in
      if (!swapped() && canPlay(meet::HIGH_FIVE) && esp_random() % 8 == 0) {
        startAct(meet::HIGH_FIVE, 0);
        return Redraw::None;
      }
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
  if (powerSaver()) return changed ? Redraw::Partial : Redraw::None;  // battery saver: no blinks
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

// The friends list: one friend at a time, its avatar and name tag, when you met.
static void drawFriends() {
  const friends::List &l = friendsList();
  const friends::Friend &f = l.f[listAt];
  char of[8], when[40];
  snprintf(of, sizeof of, "%d/%d", listAt + 1, l.count);
  drawHeader("Friends", of);
  if (f.lastMet) {
    const time_t t = f.lastMet;
    struct tm at;
    localtime_r(&t, &at);
    char month[8], day[12];
    strftime(month, sizeof month, "%b", &at);
    snprintf(day, sizeof day, "%d %s", at.tm_mday, month);  // "6 Oct", not "06 Oct"
    snprintf(when, sizeof when, "met %u time%s, last %s", f.met, f.met == 1 ? "" : "s", day);
  } else {
    snprintf(when, sizeof when, "met %u time%s", f.met, f.met == 1 ? "" : "s");
  }
  display.setFont(FONT_TINY);
  drawCentered(when, CONTENT_TOP + 9);
  if (const int best = ralliesSaved().of(f.id)) {  // played catch together
    snprintf(when, sizeof when, "best catch rally %d", best);
    drawCentered(when, CONTENT_TOP + 19);
  }
  const int16_t x = (display.width() - SIZE) / 2;
  petDraw(f.look, SCALE, x, PY);
  petDrawTag(f.name, x + SIZE / 2, PY - 3);
  if (confirmRemove) {
    char title[32];
    snprintf(title, sizeof title, "Remove %s?", f.name[0] ? f.name : "this friend");
    drawSheet(title, "You can be friends", "again next time.");
    drawHints("keep", "remove", "");
  } else {
    drawHints(l.count > 1 ? "next" : "", "", "remove");
  }
}

// A badge with a title above and the button hints below: shrunk to fit between them, so all of it shows.
static void drawBadgeScreen(const Bmp &b, const char *title, const char *right) {
  drawHeader(title, right);
  const int16_t size = HINTS_TOP - CONTENT_TOP - 10;  // 129 px
  badgeDrawFit(b, (display.width() - size) / 2, CONTENT_TOP + 5, size);
}

static void drawRoomOnly() {
  drawHeaderConnected();
  for (int i = 0; i < 2; i++) {
    meet::Actor a = {};
    a.x = pos[i];
    if (roomAsleep) a.mood = meet::SLEEPY, a.bubble = i == 0 ? (uint8_t)meet::SAY_ZZZ : (uint8_t)meet::NO_BUBBLE;
    drawPet(i, a, false);
  }
}

static void drawSheetFrame() {
  drawRoomOnly();
  const char *who = partner.name[0] ? partner.name : "them";
  char line[32];
  if (receiving) {
    snprintf(line, sizeof line, "%s is sending", partner.name[0] ? partner.name : "Your friend");
    drawSheet(line, "a badge...");
  } else if (waitingVerdict) {
    snprintf(line, sizeof line, "Sent to %s.", who);
    drawSheet(line, "Waiting to see if", "they keep it...");
  }
  drawHints("", "", "");
}

void meetDraw() {
  if (listing && !connected && friendsList().count) return drawFriends();
  if (connected && previewing) {
    char from[32];
    snprintf(from, sizeof from, "From %s", partner.name[0] ? partner.name : "a friend");
    drawBadgeScreen(rxBmp, from, nullptr);
    drawHints("no thanks", "keep", "");
    return;
  }
  if (connected && picking) {
    Bmp b;
    char of[8], title[32];
    snprintf(of, sizeof of, "%d/%d", pickAt + 1, pickCount);
    snprintf(title, sizeof title, "Send to %s", partner.name[0] ? partner.name : "friend");
    if (badgeLoad(pickNames[pickAt].c_str(), b)) drawBadgeScreen(b, title, of);
    else drawHeader(title, of), drawEmpty("Can't read it", pickNames[pickAt].c_str());
    drawHints(pickCount > 1 ? "next" : "", "send", "");
    return;
  }
  if (connected && (receiving || waitingVerdict)) return drawSheetFrame();
  if (connected && menuOpen) {  // a small list over the room, the chosen row in black
    drawRoomOnly();
    const int rows = menuRows();
    const int16_t x = MARGIN, w = display.width() - 2 * MARGIN, rowH = rows > 3 ? 24 : 30, h = rows * rowH + 12;
    const int16_t y = max<int>(CONTENT_TOP + 2, HINTS_TOP - 10 - h);
    display.fillRoundRect(x, y, w, h, 6, WHITE);
    display.drawRoundRect(x, y, w, h, 6, BLACK);
    display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 5, BLACK);
    display.setFont(FONT_SMALL);
    for (int i = 0; i < rows; i++) {
      const int16_t ry = y + 6 + i * rowH;
      if (i == menuRow) {
        display.fillRoundRect(x + 6, ry, w - 12, rowH - 2, 4, BLACK);
        display.setTextColor(WHITE);
      }
      drawCentered(MENU_ROWS[i], ry + rowH / 2 - 1);
      display.setTextColor(BLACK);
    }
    drawHints("next", "choose", "");
    return;
  }
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
    if (near < 0) {  // nobody yet: say how this works
      display.setFont(FONT_TINY);
      drawCentered("Open Meet on another unidex", CONTENT_TOP + 9);
    }
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
    drawHints(friendsList().count ? "friends" : "", meetWho, "");
    return;
  }
  drawHeaderConnected();
  for (int i = 0; i < 2; i++) {
    meet::Actor a = {};
    a.x = pos[i];
    if (roomAsleep) a.mood = meet::SLEEPY, a.bubble = i == 0 ? (uint8_t)meet::SAY_ZZZ : (uint8_t)meet::NO_BUBBLE;
    drawPet(i, a, blinking && i == mine());
  }
  if (asking) {  // "Be friends?"
    char line[32];
    snprintf(line, sizeof line, "with %s?", partner.name[0] ? partner.name : "this Pet");
    if (myAnswer == 1) {
      drawSheet("You said yes!", "Waiting for", partner.name[0] ? partner.name : "them");
      drawHints("", "", "");
    } else {
      drawSheet("Be friends", line, "");
      drawHints("not now", "yes", "");
    }
    return;
  }
  drawHints("say", swapped() ? "home" : "visit", "more");
}

const char *meetDetail() {
  static char buf[140];
  const int i = connected ? nearby.find(partner.id) : (nearby.count() ? 0 : -1);
  const int latest = i >= 0 && nearby.at(i).count ? nearby.at(i).readings[(nearby.at(i).count - 1) % 8] : 0;
  snprintf(buf, sizeof buf, "meet:%s:%d:%s:%s:%d,%d:%d:%d:%d", linkOn() ? "on" : "off", nearby.count(),
           connected ? "connected" : "searching", connected ? (amLeft ? "left" : "right") : "", pos[0], pos[1],
           i >= 0 ? nearby.strength(i) : 0, latest, nearby.firstInReach() >= 0);
  const size_t used = strlen(buf);  // then: friends saved, the last act played, the other's FEATURES, asleep, and
  // last (tools look for ":ask" at the end) "Be friends?" on screen, the friends list open...
  char menu[8] = "";
  if (menuOpen) snprintf(menu, sizeof menu, "menu%d", menuRow);  // and which row is chosen
  snprintf(buf + used, sizeof buf - used, ":f%d:a%d:t%lu:%s:%s%s%s%s%s%s%s", friendsList().count, lastAct,
           (unsigned long)theirFeatures(), roomAsleep ? "asleep" : "", asking ? "ask" : "", listing ? "list" : "",
           menu, picking ? "pick" : "", waitingVerdict ? "wait" : "", receiving ? "recv" : "",
           previewing ? "preview" : "");
  return buf;
}

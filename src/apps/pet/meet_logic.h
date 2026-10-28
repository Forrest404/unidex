#pragma once
// Pets meeting over the radio: the packets, the list of Pets nearby, and the acts two Pets play across two screens
// held side by side. No Arduino code, so it can be checked on a computer (tools/meettest).
//
// Every packet: "UDX", version, type, then the type's fields (numbers little-endian).
//   HELLO   (to everyone, 1-4 times a second while Meet is open): id u32 (random, made once per device; not its
//           radio address), look u32 (pet_logic.h's pack), name length u8 + up to 12 bytes, heard u32 (the Pet
//           this device hears best: the low 24 bits of its id, then how strongly (dBm, the top 8 bits))
//   CONNECT (someone pressed B with another device in reach): from u32, to u32
//   ACT     (the left device starts an act; both play it from when it arrived): from u32, to u32, act u8, actor u8,
//           seed u32
//   ASK     (the right device asks the left one for an act): from u32, to u32, act u8, actor u8
//   ANSWER  (be friends? this device's answer): from u32, to u32, act u8 (1 yes, 2 not now, 3 yes and already
//           settled: the reply to a 1 heard after this side became friends), actor u8 (0). A yes is repeated
//           every ~0.3 s until the question is settled, so a lost or collided packet can't leave one side behind.
//   FEATURES (both, 3 times on connecting): from u32, to u32, 0, 0, bits u32: the newer acts this device knows
//           (F_*), and F_FRIEND if it has the other as a friend. An act the other hasn't announced is never started,
//           so a Pet on older firmware (which ignores types it doesn't know) only ever gets the acts it has. Friends
//           on either side are friends on both: one that forgot (a reset, a removal) adds the other back.
//   QUESTION (the left device, about every 0.7 s while "Be friends?" is up and the right hasn't answered): from
//           u32, to u32, 0, 0: the right device asks its owner too, even if the greeting that asks never reached it.
//   PRESS   (the right device, during a high five): from u32, to u32, act u8, 0, when u32: its press, in ms from the
//           "Now!" beat (signed), sent 3 times. The left device decides and starts SLAP or MISS.
//   GAME    (both, during catch or tug of war): from u32, to u32, event u8, its byte, its value u32 (play_logic.h).
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace meet {

static const uint8_t VERSION = 2;
static const size_t MAX_NAME = 12;
enum Type : uint8_t {
  HELLO = 1, CONNECT = 3, ACT = 4, ASK = 5, ANSWER = 6, FEATURES = 11, PRESS = 12, QUESTION = 13, GAME = 14
};
// FEATURES bits: the newer acts, and (not a feature) "you're my friend".
enum : uint32_t { F_GREETINGS = 1, F_SLEEPOVER = 2, F_HIGH_FIVE = 4, F_GAMES = 8, F_FRIEND = 0x80000000u };
static const uint32_t MY_FEATURES = F_GREETINGS | F_SLEEPOVER | F_HIGH_FIVE | F_GAMES;

struct Hello {
  uint32_t id, look, heard;
  char name[MAX_NAME + 1];
};

inline void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = v >> (8 * i);
}
inline uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static const size_t HEADER = 5, HELLO_LEN = HEADER + 4 + 4 + 1 + MAX_NAME + 4;

inline size_t header(uint8_t *out, Type t) {
  out[0] = 'U', out[1] = 'D', out[2] = 'X', out[3] = VERSION, out[4] = t;
  return HEADER;
}

// The type of a packet from another device, or 0 if it isn't one of ours (or a version we don't know).
inline uint8_t typeOf(const uint8_t *p, size_t len) {
  if (len < HEADER || p[0] != 'U' || p[1] != 'D' || p[2] != 'X' || p[3] != VERSION) return 0;
  return p[4];
}

inline size_t encodeHello(const Hello &h, uint8_t *out) {
  header(out, HELLO);
  put32(out + 5, h.id);
  put32(out + 9, h.look);
  const size_t n = strnlen(h.name, MAX_NAME);
  out[13] = n;
  memset(out + 14, 0, MAX_NAME);
  memcpy(out + 14, h.name, n);
  put32(out + 14 + MAX_NAME, h.heard);
  return HELLO_LEN;
}

// False for anything malformed: wrong type or length, a name that's too long or not printable, or id 0.
inline bool decodeHello(const uint8_t *p, size_t len, Hello &h) {
  if (typeOf(p, len) != HELLO || len != HELLO_LEN || p[13] > MAX_NAME) return false;
  h.id = get32(p + 5);
  h.look = get32(p + 9);
  for (size_t i = 0; i < p[13]; i++)
    if (p[14 + i] < 0x20 || p[14 + i] > 0x7E) return false;
  memcpy(h.name, p + 14, p[13]);
  h.name[p[13]] = 0;
  h.heard = get32(p + 14 + MAX_NAME);
  return h.id != 0;
}

// CONNECT, ACT and ASK: from, to, then (ACT, ASK) act and actor, then (ACT, FEATURES, PRESS, GAME) seed (or bits,
// or when, or value).
struct Message {
  uint8_t type;
  uint32_t from, to, seed;
  uint8_t act, actor;
};

inline size_t encodeMessage(const Message &m, uint8_t *out) {
  header(out, (Type)m.type);
  put32(out + 5, m.from);
  put32(out + 9, m.to);
  if (m.type == CONNECT) return 13;
  out[13] = m.act, out[14] = m.actor;
  if (m.type == ASK || m.type == ANSWER || m.type == QUESTION) return 15;  // (ACT, FEATURES, PRESS, GAME: 19)
  put32(out + 15, m.seed);
  return 19;
}

inline bool decodeMessage(const uint8_t *p, size_t len, Message &m) {
  const uint8_t t = typeOf(p, len);
  const size_t want = t == CONNECT                                          ? 13
                      : t == ASK || t == ANSWER || t == QUESTION             ? 15
                      : t == ACT || t == FEATURES || t == PRESS || t == GAME ? 19
                                                                             : 0;
  if (!want || len != want) return false;
  m = {};
  m.type = t;
  m.from = get32(p + 5);
  m.to = get32(p + 9);
  if (t != CONNECT) m.act = p[13], m.actor = p[14];
  if (t == ACT || t == FEATURES || t == PRESS || t == GAME) m.seed = get32(p + 15);
  return m.from && m.to;
}

// --- who is nearby, and how close ---

// "heard" for a HELLO: which Pet this device hears best, and how strongly.
inline uint32_t packHeard(uint32_t id, int dbm) { return (id & 0xFFFFFF) | (uint32_t)(uint8_t)(int8_t)dbm << 24; }
// How strongly the sender hears `me` (dBm), or 0 if the Pet it hears best is another one.
inline int heardOf(uint32_t heard, uint32_t me) {
  return heard && (heard & 0xFFFFFF) == (me & 0xFFFFFF) ? (int8_t)(heard >> 24) : 0;
}

// The Pets heard recently, in the order they were first heard; one gone quiet for FORGET_MS drops off. Distance
// comes from the signal strength: single readings jump by 10 dB or more, and it depends on how the boards face each
// other as much as on distance (measured on two boards: touching -27 dBm, turned round -17, 15 cm -35, 30 cm -30,
// 1 m -49). So each device takes the median of its last 8 readings, the other device sends its own median back
// (HELLO "heard"), and "in reach" is the average of the two above REACH_DBM: typically up to about 15 cm. A
// person's press is what makes two Pets meet; "parted" (below APART_DBM) is when they've been pulled apart.
struct Nearby {
  uint32_t id, look, lastHeard;
  char name[MAX_NAME + 1];
  uint8_t mac[6];
  int8_t readings[8];  // the latest strengths, the newest at [(count - 1) % 8]
  uint8_t count;
  int8_t theirs;  // how strongly it hears this device (its own median, from its HELLO), 0 if not known
};

class NearbyList {
 public:
  static const int MAX = 8;
  static const uint32_t FORGET_MS = 4000;
  static const int REACH_DBM = -33, APART_DBM = -46;

  // Records a HELLO heard at `now` with strength `rssi` (dBm, 0 = unknown). True if the list changed in a way
  // the screen shows (a new Pet, a new look or name, or one coming into or out of reach).
  bool heard(const Hello &h, const uint8_t mac[6], uint32_t now, int8_t rssi = 0, uint32_t me = 0) {
    int i = find(h.id);
    const bool isNew = i < 0;
    if (isNew) {
      if (n == MAX) return false;
      i = n++;
      pets[i] = {};
    }
    Nearby &p = pets[i];
    const bool wasInReach = !isNew && inReach(i);
    const bool changed = isNew || p.look != h.look || strcmp(p.name, h.name) != 0;
    p.id = h.id, p.look = h.look, p.lastHeard = now;
    p.theirs = (int8_t)heardOf(h.heard, me);
    memcpy(p.name, h.name, sizeof p.name);
    memcpy(p.mac, mac, 6);
    if (rssi) {
      p.readings[p.count % 8] = rssi;
      if (p.count < 255) p.count++;
      else p.count = 248;  // keeps count % 8 going round
    }
    return changed || inReach(i) != wasInReach;
  }

  // The median of the latest readings (up to 8), or 0 with none.
  int median(int i) const {
    const Nearby &p = pets[i];
    const int k = p.count < 8 ? p.count : 8;
    if (!k) return 0;
    int8_t s[8];
    memcpy(s, p.readings, k);
    for (int a = 1; a < k; a++)
      for (int b = a; b > 0 && s[b - 1] > s[b]; b--) {
        const int8_t t = s[b];
        s[b] = s[b - 1], s[b - 1] = t;
      }
    return k % 2 ? s[k / 2] : (s[k / 2 - 1] + s[k / 2]) / 2;
  }

  // The strength both ways: the average of this device's median and the other's, or just this one's if the other
  // hasn't said. 0 with no readings.
  int strength(int i) const {
    const int mine = median(i);
    if (!mine) return 0;
    return pets[i].theirs ? (mine + pets[i].theirs) / 2 : mine;
  }

  // In reach: at least 4 readings and strength() at or above REACH_DBM (or no strength known at all).
  bool inReach(int i) const {
    if (pets[i].count == 0) return true;
    return pets[i].count >= 4 && strength(i) >= REACH_DBM;
  }
  bool parted(int i) const { return pets[i].count >= 4 && strength(i) < APART_DBM; }

  // The Pet heard most strongly (for HELLO "heard"), or -1.
  int strongest() const {
    int best = -1;
    for (int i = 0; i < n; i++)
      if (pets[i].count && (best < 0 || median(i) > median(best))) best = i;
    return best;
  }

  // Drops Pets not heard for FORGET_MS. True if any dropped off.
  bool forget(uint32_t now) {
    int kept = 0;
    for (int i = 0; i < n; i++)
      if (now - pets[i].lastHeard < FORGET_MS) pets[kept++] = pets[i];
    const bool changed = kept != n;
    n = kept;
    return changed;
  }

  // Counts every Pet as just heard: after an act, when both devices were too busy to send or read anything.
  void touchAll(uint32_t now) {
    for (int i = 0; i < n; i++) pets[i].lastHeard = now;
  }

  int firstInReach() const {
    for (int i = 0; i < n; i++)
      if (inReach(i)) return i;
    return -1;
  }
  int count() const { return n; }
  const Nearby &at(int i) const { return pets[i]; }
  int find(uint32_t id) const {
    for (int i = 0; i < n; i++)
      if (pets[i].id == id) return i;
    return -1;
  }
  void clear() { n = 0; }

 private:
  Nearby pets[MAX];
  int n = 0;
};

// --- the room: two screens side by side as one, 400 px wide ---
// The left screen shows x 0-199, the right one 200-399. Pet 0 lives on the left screen, Pet 1 on the right.
// An act is a list of frames, one per beat; both devices build the same list from (act, actor, seed, where the Pets
// stand), so whatever happens across the gap lines up.

static const int ROOM_W = 400, SCREEN_W = 200, PET_W = 96;
static const int16_t HOME[2] = {(SCREEN_W - PET_W) / 2, SCREEN_W + (SCREEN_W - PET_W) / 2};  // 52 and 252

enum Act : uint8_t {
  GREET = 1, VISIT, SWAP, TRIP, SAY, GREET_FRIEND, FRIENDS,
  HUG, HANDSHAKE,              // friends meeting again, the 5th time and on, the 10th and on (F_GREETINGS)
  SLEEPOVER,                   // friends meeting at night (F_SLEEPOVER)
  HIGH_FIVE, SLAP, MISS,       // a high five: the countdown, then how it went (F_HIGH_FIVE)
  CATCH, TUG,                  // the games, played by play_logic.h rather than as frames (F_GAMES)
  ACT_COUNT
};
enum Mood : uint8_t { NORMAL, HAPPY, SLEEPY };  // SLEEPY: eyes closed

// What a Pet says: "Hi <the other one's name>!", "Hi again <name>!", "Friends!", or a phrase.
enum : uint8_t { NO_BUBBLE = 0, SAY_HI_NAME = 1, SAY_HI_AGAIN = 2, SAY_FRIENDS = 3, FIRST_PHRASE = 4 };
// Words for the newer acts, numbered clear of the phrases (FIRST_PHRASE + PHRASE_COUNT is below 100).
enum : uint8_t {
  SAY_HUG = 100, SAY_SECRET, SAY_ZZZ, SAY_NIGHT, SAY_HIGH_FIVE, SAY_3, SAY_2, SAY_1, SAY_NOW, SAY_SLAP, SAY_OOPS,
  SAY_CATCH, SAY_PULL, SAY_I_WIN, SAY_HEHE, SAY_TIE, SAY_GOOD_GAME
};
static const char *const SAYINGS[] = {"Hug!",   "Secret handshake!", "Zzz",    "Night night!", "High five?", "3",
                                      "2",      "1",                 "Now!",   "Slap!",        "Oops!",      "Catch!",
                                      "Pull!",  "I win!",            "Hehe",   "Tie!",         "Good game!"};
inline const char *saying(uint8_t b) { return b >= SAY_HUG && b <= SAY_GOOD_GAME ? SAYINGS[b - SAY_HUG] : nullptr; }
static const char *const PHRASES[] = {"Nice hat!", "Wanna play?", "Yay!",   "I like your eyes", "Let's go!",
                                      "See you!",  "Hehe",        "Cute!",  "Best friends?",    "Over here!",
                                      "Ooh!",      "Me too!",     "Hello!", "You're fun!"};
static const int PHRASE_COUNT = sizeof PHRASES / sizeof *PHRASES;

struct Actor {
  int16_t x;       // room x of the Pet's left edge
  int8_t dy;       // up (negative) for a step's bob
  uint8_t mood;    // NORMAL or HAPPY
  uint8_t bubble;  // NO_BUBBLE, SAY_HI_NAME, or FIRST_PHRASE + phrase index
  bool bang;       // a "!" over its head
  uint8_t heart;   // a heart beside it (its scale, 0 = none)
};
struct Frame {
  Actor pet[2];
};

static const int MAX_FRAMES = 48;

// Small repeatable random numbers, the same on both devices for the same seed.
struct Dice {
  uint32_t s;
  uint32_t next() { return s = s * 1664525u + 1013904223u; }
  int below(int n) { return (int)((next() >> 8) % (uint32_t)n); }
};

class Script {
 public:
  Frame frames[MAX_FRAMES];
  int count = 0;
  int16_t x[2];  // where the Pets stand: at the start, then (after build) at the end
  int nowBeat = -1;  // HIGH_FIVE: the "Now!" frame (-1 for other acts)

  // Builds the act from the Pets standing at x0, x1. actor: which Pet does it (VISIT, SAY).
  void build(uint8_t act, uint8_t actor, uint32_t seed, int16_t x0, int16_t x1) {
    count = 0;
    nowBeat = -1;
    x[0] = x0, x[1] = x1;
    dice.s = seed | 1;
    lastPhrase = 255;
    actor &= 1;
    const int other = 1 - actor;
    switch (act) {
      case GREET:
        hold(2, [&](Frame &f) { f.pet[0].bang = f.pet[1].bang = true; });
        hold(3, [&](Frame &f) { f.pet[0].bubble = SAY_HI_NAME; });
        hold(3, [&](Frame &f) { f.pet[1].bubble = SAY_HI_NAME; });
        happyTogether(2);
        break;
      case GREET_FRIEND:  // friends meeting again
        hold(1, [&](Frame &f) { f.pet[0].bang = f.pet[1].bang = true; });
        hold(3, [&](Frame &f) { f.pet[0].bubble = SAY_HI_AGAIN, f.pet[1].mood = HAPPY; });
        hold(3, [&](Frame &f) { f.pet[1].bubble = SAY_HI_AGAIN, f.pet[0].mood = HAPPY; });
        happyTogether(2);
        break;
      case FRIENDS:  // they just became friends: both walk up to the gap, "Friends!", hearts, home again
        walk2(SCREEN_W - PET_W, SCREEN_W, 4);
        hold(3, [&](Frame &f) {
          f.pet[0].bubble = f.pet[1].bubble = SAY_FRIENDS;
          f.pet[0].mood = f.pet[1].mood = HAPPY;
        });
        happyTogether(3);
        walk2(HOME[0], HOME[1], 4);
        break;
      case VISIT: {
        // The host steps aside to the far side of its screen; the visitor walks over and stands beside it.
        const int16_t hostX = other == 1 ? ROOM_W - PET_W : 0;
        const int16_t visitorX = other == 1 ? SCREEN_W + 4 : SCREEN_W - PET_W - 4;
        hold(1, [&](Frame &f) { f.pet[other].bang = true; });
        walk(other, hostX, 3);
        walk(actor, visitorX, 6);
        still(1);
        talk(actor, 3);
        happyTogether(2);
        walk(actor, HOME[actor], 6);
        walk(other, HOME[other], 3);
        break;
      }
      case SWAP:
        // Both walk over at once and pass in the gap; they stay there until the next swap.
        walk2(x[1], x[0], 8);
        still(1);
        happyTogether(2);
        break;
      case TRIP:
        // Both into the right screen and a chat, both into the left screen and a chat, then home.
        walk2(SCREEN_W + 4, ROOM_W - PET_W, 6);
        talk(0, 2);
        walk2(0, PET_W + 4, 8);
        talk(1, 2);
        happyTogether(2);
        walk2(HOME[0], HOME[1], 6);
        break;
      case HUG:  // friends since a while: up to the gap, a hug, hearts, home
        hold(1, [&](Frame &f) { f.pet[0].bang = f.pet[1].bang = true; });
        walk2(SCREEN_W - PET_W + 6, SCREEN_W - 6, 4);
        hold(3, [&](Frame &f) {
          f.pet[0].bubble = f.pet[1].bubble = SAY_HUG;
          f.pet[0].mood = f.pet[1].mood = HAPPY;
        });
        happyTogether(2);
        walk2(HOME[0], HOME[1], 4);
        break;
      case HANDSHAKE:  // best friends: up to the gap, bump, shake, a jump together, home
        hold(1, [&](Frame &f) { f.pet[0].bang = f.pet[1].bang = true; });
        walk2(SCREEN_W - PET_W, SCREEN_W, 4);
        for (int i = 0; i < 2; i++) {  // bump: one bobs, then the other
          hold(1, [&](Frame &f) { f.pet[0].dy = -8, f.pet[0].mood = HAPPY; });
          hold(1, [&](Frame &f) { f.pet[1].dy = -8, f.pet[1].mood = HAPPY; });
        }
        for (int i = 0; i < 2; i++) {  // shake: together and apart
          hold(1, [&](Frame &f) { f.pet[0].x += 6, f.pet[1].x -= 6; });
          still(1);
        }
        hold(2, [&](Frame &f) {  // the jump
          f.pet[0].dy = f.pet[1].dy = -12;
          f.pet[0].bubble = f.pet[1].bubble = SAY_SECRET;
          f.pet[0].mood = f.pet[1].mood = HAPPY;
        });
        happyTogether(2);
        walk2(HOME[0], HOME[1], 4);
        break;
      case SLEEPOVER:  // friends at night: sleepily up to the gap, "Night night!", and to sleep side by side
        walk2(SCREEN_W - PET_W + 6, SCREEN_W - 6, 6);
        hold(2, [&](Frame &f) { f.pet[0].bubble = f.pet[1].bubble = SAY_NIGHT; });
        hold(2, [&](Frame &f) { f.pet[0].mood = f.pet[1].mood = SLEEPY, f.pet[0].bubble = SAY_ZZZ; });
        hold(2, [&](Frame &f) { f.pet[0].mood = f.pet[1].mood = SLEEPY, f.pet[1].bubble = SAY_ZZZ; });
        hold(1, [&](Frame &f) { f.pet[0].mood = f.pet[1].mood = SLEEPY; });
        return;  // they stay asleep there (no last normal frame)
      case HIGH_FIVE:  // up to the gap, "High five?", a countdown, and a few beats to press B in
        walk2(SCREEN_W - PET_W + 6, SCREEN_W - 6, 4);
        hold(2, [&](Frame &f) { f.pet[0].bubble = SAY_HIGH_FIVE, f.pet[1].mood = HAPPY; });
        for (uint8_t n = SAY_3; n <= SAY_1; n++) hold(1, [&](Frame &f) { f.pet[0].bubble = f.pet[1].bubble = n; });
        nowBeat = count;
        hold(3, [&](Frame &f) { f.pet[0].bubble = f.pet[1].bubble = SAY_NOW, f.pet[0].dy = f.pet[1].dy = -4; });
        return;  // SLAP or MISS follows (and walks them home)
      case CATCH:  // the games: played live (play_logic.h), not built here
      case TUG:
        break;
      case SLAP:  // both pressed together
        hold(2, [&](Frame &f) {
          f.pet[0].x += 4, f.pet[1].x -= 4;
          f.pet[0].bubble = f.pet[1].bubble = SAY_SLAP;
          f.pet[0].bang = f.pet[1].bang = true;
          f.pet[0].mood = f.pet[1].mood = HAPPY;
        });
        happyTogether(3);
        walk2(HOME[0], HOME[1], 4);
        break;
      case MISS:  // not together: both look at each other, "Oops!", laugh, home
        hold(2, [&](Frame &f) { f.pet[0].bubble = f.pet[1].bubble = SAY_OOPS; });
        hold(2, [&](Frame &f) { f.pet[0].mood = f.pet[1].mood = HAPPY; });
        walk2(HOME[0], HOME[1], 4);
        break;
      case SAY:
      default:
        sayLine(actor, true);
        sayLine(other, false);
        break;
    }
    still(1);  // a last frame with everyone back to normal
  }

 private:
  Dice dice;
  int lastPhrase = 255;

  // A new frame with both Pets where they stand, normal.
  Frame &add() {
    Frame &f = frames[count < MAX_FRAMES ? count++ : MAX_FRAMES - 1];
    f = {};
    f.pet[0].x = x[0], f.pet[1].x = x[1];
    return f;
  }

  template <typename Fn> void hold(int beats, Fn fn) {
    for (int i = 0; i < beats; i++) fn(add());
  }
  void still(int beats) {
    for (int i = 0; i < beats; i++) add();
  }

  // One Pet walks to `to` in `steps` steps, bobbing; the other stands still.
  void walk(int who, int16_t to, int steps) {
    const int16_t from = x[who];
    for (int k = 1; k <= steps; k++) {
      Frame &f = add();
      f.pet[who].x = from + (to - from) * k / steps;
      f.pet[who].dy = k % 2 ? -4 : 0;
      f.pet[who].mood = HAPPY;
    }
    x[who] = to;
  }

  // Both walk at once.
  void walk2(int16_t to0, int16_t to1, int steps) {
    const int16_t f0 = x[0], f1 = x[1];
    for (int k = 1; k <= steps; k++) {
      Frame &f = add();
      f.pet[0].x = f0 + (to0 - f0) * k / steps;
      f.pet[1].x = f1 + (to1 - f1) * k / steps;
      f.pet[0].dy = f.pet[1].dy = k % 2 ? -4 : 0;
      f.pet[0].mood = f.pet[1].mood = HAPPY;
    }
    x[0] = to0, x[1] = to1;
  }

  uint8_t phrase() {
    int p = dice.below(PHRASE_COUNT);
    if (p == lastPhrase) p = (p + 1) % PHRASE_COUNT;
    lastPhrase = p;
    return FIRST_PHRASE + p;
  }

  // One line, held 3 beats; the listener looks happy. With a heart for a first line.
  void sayLine(int who, bool heart) {
    const uint8_t b = phrase();
    hold(3, [&](Frame &f) {
      f.pet[who].bubble = b;
      f.pet[who].heart = heart ? 2 : 0;
      f.pet[1 - who].mood = HAPPY;
    });
  }

  // They take turns, `lines` lines, starting with `first`.
  void talk(int first, int lines) {
    for (int i = 0; i < lines; i++) sayLine((first + i) % 2, false);
  }

  void happyTogether(int beats) {
    hold(beats, [&](Frame &f) {
      f.pet[0].mood = f.pet[1].mood = HAPPY;
      f.pet[0].heart = f.pet[1].heart = 2;
    });
  }
};

// Friends if either side says so (its own list, or F_FRIEND in the other's FEATURES): a reset or a removal on one
// side doesn't make them strangers again.
inline bool friendsNow(bool mine, uint32_t theirBits) { return mine || (theirBits & F_FRIEND); }

// Friends meeting again, the left device chooses how (with its own count of meetings): a hug from the 5th, the secret
// handshake from the 10th, at night a sleepover; only acts the other has announced (FEATURES).
static const int HUG_FROM = 5, HANDSHAKE_FROM = 10;
inline uint8_t friendsGreeting(int met, bool night, uint32_t theirFeatures) {
  if (night && (theirFeatures & F_SLEEPOVER)) return SLEEPOVER;
  if (theirFeatures & F_GREETINGS) {
    if (met >= HANDSHAKE_FROM) return HANDSHAKE;
    if (met >= HUG_FROM) return HUG;
  }
  return GREET_FRIEND;
}
inline bool isNight(int minuteOfDay) { return minuteOfDay >= 22 * 60 || minuteOfDay < 7 * 60; }  // as the Pet sleeps

// A high five lands when both pressed within HIGH_FIVE_MS of each other, between a beat before "Now!" and the end.
static const int32_t HIGH_FIVE_MS = 400, PRESS_FROM_MS = -600;
static const int32_t NO_PRESS = 0x7FFFFFFF;
inline uint8_t highFive(int32_t mine, int32_t theirs) {
  if (mine == NO_PRESS || theirs == NO_PRESS) return MISS;
  const int32_t d = mine > theirs ? mine - theirs : theirs - mine;
  return d <= HIGH_FIVE_MS ? SLAP : MISS;
}

}  // namespace meet

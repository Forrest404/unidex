// Checks the Pets' radio packets, the reach rule and the acts on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/pet tools/meettest/meet_test.cpp -o /tmp/meet_test && /tmp/meet_test
#include <cstdio>
#include <cstdlib>
#include "meet_logic.h"

using namespace meet;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static bool same(const Actor &a, const Actor &b) {
  return a.x == b.x && a.dy == b.dy && a.mood == b.mood && a.bubble == b.bubble && a.bang == b.bang &&
         a.heart == b.heart;
}

static Hello hello(uint32_t id, const char *name, uint32_t look = 0x10000) {
  Hello h = {};
  h.id = id, h.look = look;
  strncpy(h.name, name, MAX_NAME);
  return h;
}

int main() {
  // --- packets ---
  uint8_t p[64];
  Hello in = hello(0xA1B2C3D4, "Mochi", 0x31201), out;
  size_t n = encodeHello(in, p);
  check(n == HELLO_LEN && n <= 250, "a HELLO fits in one packet");
  check(decodeHello(p, n, out) && out.id == in.id && out.look == in.look && strcmp(out.name, "Mochi") == 0,
        "a HELLO survives encoding and decoding");
  n = encodeHello(hello(5, "ABCDEFGHIJKLMNOP"), p);
  check(decodeHello(p, n, out) && strlen(out.name) == MAX_NAME, "a long name is cut to 12 characters");
  n = encodeHello(in, p);
  uint8_t bad[64];
  memcpy(bad, p, n);
  bad[3] = 1;
  check(!decodeHello(bad, n, out) && typeOf(bad, n) == 0, "an older protocol version is ignored");
  memcpy(bad, p, n);
  bad[14] = '\n';
  check(!decodeHello(bad, n, out), "a name with a control character is ignored");
  check(!decodeHello(p, n - 1, out), "a short packet is ignored");

  Message m = {ACT, 11, 22, 0xDEADBEEF, VISIT, 1}, back;
  n = encodeMessage(m, p);
  check(decodeMessage(p, n, back) && back.type == ACT && back.from == 11 && back.to == 22 && back.act == VISIT &&
            back.actor == 1 && back.seed == 0xDEADBEEF,
        "an ACT survives encoding and decoding");
  m = {ASK, 22, 11, 0, SWAP, 1};
  n = encodeMessage(m, p);
  check(decodeMessage(p, n, back) && back.type == ASK && back.act == SWAP && back.seed == 0, "an ASK too");
  m = {ANSWER, 22, 11, 0, 1, 0};
  n = encodeMessage(m, p);
  check(decodeMessage(p, n, back) && back.type == ANSWER && back.act == 1 && n == 15, "an ANSWER too");
  m = {CONNECT, 22, 11};
  n = encodeMessage(m, p);
  check(decodeMessage(p, n, back) && back.type == CONNECT && n == 13, "a CONNECT too");
  check(!decodeMessage(p, n - 1, back), "a CONNECT one byte short is ignored");
  n = encodeHello(in, p);
  check(!decodeMessage(p, n, back), "a HELLO isn't taken for a message");

  // --- reach, from readings like the ones measured on two boards ---
  const uint8_t mac[6] = {2, 1, 1, 1, 1, 1};
  const int touching[] = {-27, -26, -28, -27, -26, -27, -28, -26};
  const int thirtyCm[] = {-36, -42, -34, -35, -38, -41, -33, -36};
  const int oneMetre[] = {-49, -55, -43, -60, -47, -52, -44, -49};
  const int spiky[] = {-49, -55, -12, -60, -47, -10, -44, -49};  // two freak strong readings at 1 m
  auto reach = [&](const int *r, int count) {
    NearbyList l;
    for (int i = 0; i < count; i++) l.heard(hello(7, "x"), mac, i * 250, r[i]);
    return l;
  };
  check(reach(touching, 8).inReach(0), "touching: in reach");
  check(!reach(thirtyCm, 8).inReach(0), "30 cm (typical readings): out of reach");
  check(!reach(oneMetre, 8).inReach(0) && reach(oneMetre, 8).parted(0), "1 m: out of reach, and parted");
  check(!reach(spiky, 8).inReach(0), "two freak strong readings at 1 m don't put it in reach (median)");
  check(!reach(touching, 3).inReach(0), "not in reach on fewer than 4 readings");
  NearbyList none;
  none.heard(hello(8, "y"), mac, 0, 0);
  check(none.inReach(0), "no strength known at all: counts as in reach");
  // Both ways: the other device's median of this one comes back in its HELLO.
  check(heardOf(packHeard(0xABCDEF12, -27), 0x99CDEF12) == -27 && heardOf(packHeard(0xABCDEF12, -27), 7) == 0,
        "a HELLO's 'heard' names this device by the low 24 bits of its id");
  NearbyList both;
  Hello hb = hello(7, "x");
  hb.heard = packHeard(42, -45);  // it hears this device weakly (turned away)
  for (int i = 0; i < 8; i++) both.heard(hb, mac, i * 250, -24, 42);
  check(both.strength(0) == (-24 - 45) / 2 && !both.inReach(0), "strong one way, weak the other: the average decides");
  hb.heard = packHeard(42, -26);
  both.heard(hb, mac, 2000, -24, 42);
  check(both.inReach(0), "strong both ways: in reach");
  hb.heard = packHeard(43, -26);  // it's reporting another Pet, not this one
  both.heard(hb, mac, 2250, -24, 42);
  check(both.strength(0) == both.median(0), "a report about another Pet is ignored");

  NearbyList moving = reach(touching, 8);
  for (int i = 0; i < 8; i++) moving.heard(hello(7, "x"), mac, 2000 + i * 250, oneMetre[i]);
  check(!moving.inReach(0) && moving.parted(0), "moved away: out of reach once the readings follow");
  NearbyList many;
  for (int i = 0; i < 300; i++) many.heard(hello(9, "z"), mac, i * 250, -27);
  check(many.inReach(0) && many.median(0) == -27, "hundreds of readings later the median still works");
  NearbyList quiet = reach(touching, 8);
  check(!quiet.forget(1750 + NearbyList::FORGET_MS - 1) && quiet.forget(1750 + NearbyList::FORGET_MS),
        "a Pet gone quiet for 4 s drops off");

  // --- acts ---
  bool sameOnBoth = true, smooth = true, fits = true, onStage = true;
  for (int act = GREET; act < ACT_COUNT; act++)
    for (int actor = 0; actor < 2; actor++)
      for (uint32_t seed = 1; seed < 40; seed++) {
        Script a, b;
        a.build(act, actor, seed, HOME[0], HOME[1]);
        b.build(act, actor, seed, HOME[0], HOME[1]);
        sameOnBoth &= a.count == b.count;
        for (int f = 0; f < a.count && f < b.count; f++)
          for (int i = 0; i < 2; i++) sameOnBoth &= same(a.frames[f].pet[i], b.frames[f].pet[i]);
        fits &= a.count > 0 && a.count < MAX_FRAMES;
        int16_t last[2] = {HOME[0], HOME[1]};
        for (int f = 0; f < a.count; f++)
          for (int i = 0; i < 2; i++) {
            const Actor &x = a.frames[f].pet[i];
            smooth &= abs(x.x - last[i]) <= 52;  // a step is at most about half a Pet wide
            onStage &= x.x >= 0 && x.x <= ROOM_W - PET_W;
            last[i] = x.x;
          }
      }
  check(sameOnBoth, "both devices build exactly the same frames from the same act, actor and seed");
  check(fits, "every act fits in the frame list");
  check(smooth, "no Pet jumps: every step is at most 52 px");
  check(onStage, "no Pet ever leaves the room");

  Script s;
  s.build(VISIT, 0, 5, HOME[0], HOME[1]);
  check(s.x[0] == HOME[0] && s.x[1] == HOME[1], "after a visit both are home again");
  bool crossed = false, hostAside = false;
  for (int f = 0; f < s.count; f++) {
    crossed |= s.frames[f].pet[0].x >= SCREEN_W;
    hostAside |= s.frames[f].pet[1].x == ROOM_W - PET_W;
  }
  check(crossed && hostAside, "a visit: the visitor walks into the other screen and the host steps aside");
  s.build(SWAP, 0, 5, HOME[0], HOME[1]);
  check(s.x[0] == HOME[1] && s.x[1] == HOME[0], "a swap leaves them on each other's screens");
  s.build(SWAP, 1, 9, s.x[0], s.x[1]);
  check(s.x[0] == HOME[0] && s.x[1] == HOME[1], "a second swap brings them back");
  s.build(TRIP, 0, 3, HOME[0], HOME[1]);
  check(s.x[0] == HOME[0] && s.x[1] == HOME[1], "after a trip both are home");
  s.build(GREET, 0, 3, HOME[0], HOME[1]);
  bool hiBoth[2] = {};
  for (int f = 0; f < s.count; f++)
    for (int i = 0; i < 2; i++) hiBoth[i] |= s.frames[f].pet[i].bubble == SAY_HI_NAME;
  check(hiBoth[0] && hiBoth[1], "a greeting: each one says hi by name");
  s.build(GREET_FRIEND, 0, 3, HOME[0], HOME[1]);
  bool again[2] = {};
  for (int f = 0; f < s.count; f++)
    for (int i = 0; i < 2; i++) again[i] |= s.frames[f].pet[i].bubble == SAY_HI_AGAIN;
  check(again[0] && again[1] && s.x[0] == HOME[0], "friends meeting again: each says hi again");
  s.build(FRIENDS, 0, 3, HOME[0], HOME[1]);
  bool atGap = false, saidIt = false;
  for (int f = 0; f < s.count; f++) {
    atGap |= s.frames[f].pet[0].x + PET_W == SCREEN_W && s.frames[f].pet[1].x == SCREEN_W;
    saidIt |= s.frames[f].pet[0].bubble == SAY_FRIENDS && s.frames[f].pet[1].bubble == SAY_FRIENDS;
  }
  check(atGap && saidIt && s.x[0] == HOME[0] && s.x[1] == HOME[1],
        "becoming friends: they meet at the gap, say \"Friends!\" together, and go home");
  s.build(SAY, 1, 3, HOME[0], HOME[1]);
  check(s.frames[0].pet[1].bubble >= FIRST_PHRASE && s.frames[3].pet[0].bubble >= FIRST_PHRASE &&
            s.frames[0].pet[1].bubble != s.frames[3].pet[0].bubble,
        "saying something: one speaks, the other answers with a different line");

  printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n", failures);
  return failures ? 1 : 0;
}

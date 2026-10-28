// Checks the Meet games (catch, tug of war) on a computer: two copies of a game, one per device, each on its own
// clock, talking over a pretend radio that delays and loses packets, must agree on what happened.
//   c++ -std=c++17 -O2 -I src/apps/pet tools/meettest/play_test.cpp -o /tmp/play_test && /tmp/play_test
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>
#include "play_logic.h"

using namespace duo;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static const int32_t NO = 0x7FFFFFFF;

// Two devices playing one game. Device 1's game starts `lag` ms after device 0's (the ACT took that long to arrive).
// Packets take `delay` ms and each copy is lost with chance `loss` (in %); a fresh one goes 3 times, as tell() sends
// it, and the latest is sent again every 200 ms.
struct Match {
  Game g[2];
  int32_t lag = 30, delay = 5, loss = 0;
  // When device i presses B, in its game time: called each ms with its game; returns true to press now.
  std::function<bool(int, const Game &, int32_t)> presses;
  struct Pkt {
    int to;
    int32_t at;
    Out o;
  };
  std::vector<Pkt> air;
  int32_t doneAt[2] = {NO, NO};
  std::vector<int32_t> doneEvents;
  uint32_t rnd = 12345;
  bool quitBy[2] = {false, false};
  int32_t quitAt = NO;  // device 0 holds A at this game time

  bool lost() {
    rnd = rnd * 1103515245u + 12345u;
    return (int32_t)((rnd >> 16) % 100) < loss;
  }
  void put(int from, int32_t world, const Out &o, int copies) {
    for (int c = 0; c < copies; c++)
      if (!lost()) air.push_back({1 - from, world + delay + c * 15, o});
  }

  void run(uint8_t act, uint32_t seed, int32_t untilWorld = 60000) {
    g[0].begin(act, seed, 0), g[1].begin(act, seed, 1);
    int32_t resendAt[2] = {200, 200};
    for (int32_t w = 0; w < untilWorld; w++) {
      for (int i = 0; i < 2; i++) {
        const int32_t t = w - (i ? lag : 0);
        if (doneAt[i] != NO) continue;
        for (size_t p = 0; p < air.size();)
          if (air[p].to == i && air[p].at <= w) {
            g[i].heard(air[p].o.ev, air[p].o.actor, air[p].o.value, t);
            air.erase(air.begin() + p);
          } else {
            p++;
          }
        if (i == 0 && t == quitAt) g[0].leave(t);
        if (presses && t >= 0 && presses(i, g[i], t)) g[i].press(t);
        g[i].tick(t);
        Out o;
        if (g[i].takeOut(o)) put(i, w, o, 3), resendAt[i] = w + 200;
        else if (w >= resendAt[i] && g[i].latest(o)) put(i, w, o, 1), resendAt[i] = w + 200;
        if (g[i].done(t)) doneAt[i] = t;
      }
      if (doneAt[0] != NO && doneAt[1] != NO) break;
    }
  }
};

// The catcher presses `early` ms before the ball arrives (negative: after), on throws for which `catches` says so.
static std::function<bool(int, const Game &, int32_t)> catcher(std::function<bool(int k, int who)> catches,
                                                               int32_t early = 40) {
  return [=](int i, const Game &g, int32_t t) {
    return !g.over && g.cur.to() == i && catches(g.k, i) && t == g.cur.arrives() - early;
  };
}

// Device i presses every `every[i]` ms from "Pull!" (0: never).
static std::function<bool(int, const Game &, int32_t)> puller(int every0, int every1) {
  return [=](int i, const Game &, int32_t t) {
    const int every = i ? every1 : every0;
    return every && t >= TUG_GO && (t - TUG_GO) % every == 0;
  };
}

static bool sameScene(const Game &a, const Game &b, int32_t t) {
  Scene x, y;
  a.scene(t, x), b.scene(t, y);
  for (int i = 0; i < 2; i++)
    if (x.pet[i].x != y.pet[i].x || x.pet[i].bubble != y.pet[i].bubble || x.pet[i].mood != y.pet[i].mood) return false;
  return x.ball == y.ball && (!x.ball || (x.ballX == y.ballX && x.ballY == y.ballY)) && x.rope == y.rope &&
         x.knotX == y.knotX && x.rally == y.rally;
}

int main() {
  // --- the packet ---
  uint8_t p[32];
  meet::Message m = {meet::GAME, 11, 22, 41, EV_CATCH, CAUGHT}, back;
  size_t n = meet::encodeMessage(m, p);
  check(n == 19 && meet::decodeMessage(p, n, back) && back.type == meet::GAME && back.act == EV_CATCH &&
            back.actor == CAUGHT && back.seed == 41,
        "a GAME packet survives encoding and decoding");
  check(meet::saying(meet::SAY_GOOD_GAME) && meet::saying(meet::SAY_CATCH), "the games' words are there");
  check(flightMs(0) == 1500 && flightMs(5) < flightMs(4) && flightMs(40) == 660, "throws get faster, up to a point");

  // --- catch ---
  {
    Match mt;
    mt.presses = catcher([](int k, int) { return k < 20; });  // 20 catches, then nobody catches
    mt.run(meet::CATCH, 7);
    check(mt.g[0].best == 20 && mt.g[1].best == 20, "20 catches: a rally of 20 on both");
    check(mt.g[0].over && mt.g[1].over && mt.g[0].k == mt.g[1].k && mt.g[0].k == 20 + MAX_DROPS,
          "then three drops in a row: over on both, after the same throw");
    check(mt.g[0].endAt == mt.g[1].endAt, "and at the same moment of the game");
    check(mt.g[0].cur.at == mt.g[1].cur.at, "every throw at the same game time on both");
    check(mt.doneAt[0] != NO && mt.doneAt[1] != NO, "both finish");
  }
  {
    Match mt;  // a drop in the middle: the rally starts again
    mt.presses = catcher([](int k, int) { return k != 4 && k < 9; });
    mt.run(meet::CATCH, 3);
    check(mt.g[0].best == 4 && mt.g[1].best == 4, "a drop at the 5th throw: the best rally is 4 on both");
    // Throw 4 dropped, 5-8 caught: a rally of 4 again, then three drops.
    check(mt.g[0].k == 9 + MAX_DROPS && mt.g[1].k == mt.g[0].k, "and play goes on after it");
  }
  {
    Match mt;  // pressing far too soon is a drop, not a catch
    mt.presses = catcher([](int, int) { return true; }, 700);
    mt.run(meet::CATCH, 3);
    check(mt.g[0].best == 0 && mt.g[1].best == 0 && mt.g[0].k == MAX_DROPS, "pressing too soon drops it");
  }
  {
    Match mt;  // just inside the window, both ways
    mt.presses = [&](int i, const Game &g, int32_t t) {
      if (g.over || g.cur.to() != i) return false;
      const int32_t when = g.k % 2 ? g.cur.arrives() + WINDOW_MS - 5 : g.cur.arrives() - WINDOW_MS + 5;
      return g.k < 6 && t == when;
    };
    mt.run(meet::CATCH, 9);
    check(mt.g[0].best == 6 && mt.g[1].best == 6, "a press up to 0.25 s early or late still catches");
  }
  {
    Match mt;  // a bad radio: 40% of copies lost, 60 ms late
    mt.loss = 40, mt.delay = 60;
    mt.presses = catcher([](int k, int) { return k < 15 && k != 7; });
    mt.run(meet::CATCH, 5);
    check(mt.g[0].best == 7 && mt.g[1].best == 7 && mt.g[0].k == mt.g[1].k && mt.g[0].endAt == mt.g[1].endAt,
          "with 40% of packets lost and 60 ms late, both still agree on every throw");
  }
  {
    Match mt;  // the other device goes silent: over after LOST_MS, not stuck
    mt.loss = 100;
    mt.presses = catcher([](int, int) { return true; });
    mt.run(meet::CATCH, 5);
    check(mt.doneAt[0] != NO && mt.doneAt[1] != NO, "no packets at all: both still end");
  }
  {
    Match mt;  // holding A ends it for both
    mt.quitAt = 5000;
    mt.presses = catcher([](int, int) { return true; });
    mt.run(meet::CATCH, 5);
    check(mt.g[0].quit && mt.g[1].quit && mt.doneAt[0] == 5000 && mt.doneAt[1] < 5100, "holding A ends it on both");
  }
  {  // both screens show the same at the same game time, once both know how each throw went
    Match mt;
    mt.lag = 0;
    mt.presses = catcher([](int k, int) { return k < 5; });
    Game a;
    mt.run(meet::CATCH, 11);
    bool same = true;
    for (int32_t t = 0; t < mt.g[0].endAt + END_MS; t += 10) same &= sameScene(mt.g[0], mt.g[1], t);
    check(same, "the same scene on both screens (the last throws, the chase, the end)");
    Scene s;
    a.begin(meet::CATCH, 11, 0);
    a.scene(INTRO_MS + 700, s);
    check(s.ball && s.ballX > CATCH_X[0] + meet::PET_W && s.ballX < CATCH_X[1] && s.ballY < PAW_Y,
          "the first throw: the ball is in the air between them, above their paws");
    a.scene(1500, s);
    check(s.pet[0].bubble == meet::SAY_CATCH && s.pet[0].x == CATCH_X[0] && s.pet[1].x == CATCH_X[1],
          "they walk to their places and the left one says Catch!");
  }

  {  // a late packet from an earlier game doesn't count in this one
    Game old, now;
    old.begin(meet::TUG, 0x10000000, 1), now.begin(meet::TUG, 0x20000000, 0);
    old.press(TUG_GO + 1);
    Out o;
    old.takeOut(o);
    now.heard(o.ev, o.actor, o.value, TUG_GO + 5);
    check(now.pulls[1] == 0, "a pull from an earlier game is ignored");
    now.begin(meet::TUG, 0x10000000, 0);
    now.heard(o.ev, o.actor, o.value, TUG_GO + 5);
    check(now.pulls[1] == 1, "and one from this game counts");
  }

  // --- tug of war ---
  {
    Match mt;
    mt.presses = puller(100, 150);  // the left pulls faster
    mt.run(meet::TUG, 1);
    check(mt.g[0].winner == 0 && mt.g[1].winner == 0, "the faster puller (left) wins on both");
    check(mt.g[0].finalShift == -WIN_PX && mt.g[1].finalShift == -WIN_PX, "with the knot pulled all the way over");
    check(mt.g[0].endAt < TUG_GO + TUG_MS, "before the time's up");
  }
  {
    Match mt;
    mt.presses = puller(170, 110);
    mt.loss = 30, mt.delay = 40;
    mt.run(meet::TUG, 1);
    check(mt.g[0].winner == 1 && mt.g[1].winner == 1, "the right one wins on both, over a bad radio too");
  }
  {
    Match mt;
    mt.presses = puller(120, 120);
    mt.lag = 0;
    mt.run(meet::TUG, 1);
    check(mt.g[0].winner == 2 && mt.g[1].winner == 2 && mt.g[0].endAt == TUG_GO + TUG_MS,
          "pulling just as fast: a tie when the time's up");
  }
  {
    Match mt;
    mt.presses = puller(200, 0);  // only the left pulls, slowly: still ahead when time's up?
    mt.run(meet::TUG, 1);
    check(mt.g[0].winner == 0 && mt.g[1].winner == 0, "one side not pulling: the other wins");
  }
  {
    Match mt;  // the verdict never arrives: the right decides by itself, the same way
    mt.presses = puller(400, 600);
    mt.loss = 0;
    mt.run(meet::TUG, 1, 25000);
    check(mt.g[0].winner == mt.g[1].winner, "the same winner on both when the time's up");
    Game solo;
    solo.begin(meet::TUG, 1, 1);
    for (int32_t t = 0; t <= TUG_GO + TUG_MS + LOST_MS; t++) solo.tick(t);
    check(solo.over && solo.winner == 2, "a right device that hears nothing ends by itself");
  }
  {
    Game a;
    a.begin(meet::TUG, 1, 0);
    Scene s;
    a.scene(2000, s);
    check(s.rope && s.knotX == meet::SCREEN_W && s.pet[0].bubble == meet::SAY_2, "the countdown: 3, 2, 1");
    a.press(2500);
    check(a.pulls[0] == 0, "pulls before Pull! don't count");
    for (int i = 0; i < 5; i++) a.press(TUG_GO + i);
    a.scene(TUG_GO + 10, s);
    check(s.knotX == meet::SCREEN_W - 5 * PULL_PX && s.pet[0].x == TUG_X[0] - 5 * PULL_PX && s.secondsLeft == 15,
          "each pull moves the knot, and both Pets, toward the puller");
  }

  // --- the best rallies ---
  {
    Rallies r = {};
    check(r.record(5, 3) && r.of(5) == 3 && !r.record(5, 2) && r.of(5) == 3 && r.record(5, 9) && r.of(5) == 9,
          "a best rally is kept, and only beaten by a better one");
    for (uint32_t id = 100; id < 100 + Rallies::MAX; id++) r.record(id, 1);
    check(r.count == Rallies::MAX && r.of(5) == 0 && r.of(100 + Rallies::MAX - 1) == 1, "full: the oldest goes");
  }

  printf(failures ? "\n%d FAILED\n" : "\nall ok\n", failures);
  return failures ? 1 : 0;
}

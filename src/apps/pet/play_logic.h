#pragma once
// The games two Pets play in Meet, across both screens: catch and tug of war. Each device runs its own copy of the
// game on its own clock, from the moment the left device started it (as acts are), and they stay the same by
// sharing only what a press changes (GAME packets, meet_logic.h). No Arduino code, so it can be checked on a
// computer (tools/meettest/play_test.cpp), two copies talking with lost and late packets.
//
// Catch: the ball flies from one Pet to the other in an arc; the one it's flying to presses B within WINDOW_MS of it
// arriving to catch it, and throws it back, a little faster each time. Each throw is judged on the catcher's own
// device, against its own clock (so the radio's delay never decides it), and it tells the other (EV_CATCH). Too
// soon, or no press: dropped, the Pet runs after it, and the rally starts again. Three drops in a row: game over.
// Tug of war: "3, 2, 1, Pull!", then each B pulls the rope your way (both send their count, EV_PULLS). The left
// device decides: the first to pull the knot WIN_PX over wins; after TUG_MS, whoever's ahead (EV_WIN).
// Holding A ends either game for both (EV_QUIT).
#include "meet_logic.h"

namespace duo {

using meet::Actor;

// GAME packet events: the event, its byte (actor: the low 2 bits, plus this game's tag above them, so a late copy
// from an earlier game is never taken for this one's), its value.
enum Ev : uint8_t {
  EV_CATCH = 1,  // throw number (value) was CAUGHT or DROPPED (byte): from the device that judged it
  EV_PULLS = 2,  // this device's pulls so far (value)
  EV_WIN = 3,    // the left device's verdict: the winner (byte: 0, 1, or 2 for a tie), the knot's place (value)
  EV_QUIT = 4,   // held A: the game's over
};
enum Result : uint8_t { UNKNOWN = 0, CAUGHT = 1, DROPPED = 2 };

static const int32_t INTRO_MS = 2000;  // both walk to their places and get ready
static const int32_t END_MS = 2000;    // the ending, before going back to the room
static const int32_t LOST_MS = 3000;   // no word from the other side this long after it was due: over

// Catch
static const int32_t WINDOW_MS = 250;  // B this close to the ball arriving catches it
static const int32_t HOLD_MS = 450;    // a caught ball is thrown back this long after it arrived
static const int32_t CHASE_MS = 2400;  // a dropped one: running after it and bringing it back
static const int MAX_DROPS = 3;        // in a row: game over
inline int32_t flightMs(int rally) { return rally < 12 ? 1500 - 70 * rally : 660; }  // faster each catch
static const int16_t CATCH_X[2] = {10, meet::ROOM_W - 10 - meet::PET_W};  // where they stand: 10 and 294
static const int16_t PAW_Y = 52;  // the paws, from the Pets' top (the ball and the rope)

// Tug of war
static const int32_t TUG_GO = 3000, TUG_MS = 15000;  // "Pull!" and how long they pull
static const int PULL_PX = 4, WIN_PX = 60;           // each pull moves the knot; this far over wins
static const int16_t TUG_X[2] = {meet::SCREEN_W - meet::PET_W - 24, meet::SCREEN_W + 24};  // 80 and 224

// What to draw at a moment: the Pets (as acts draw them), the ball, the rope and its knot (room x; y from the Pets'
// top), and the numbers for the header.
struct Scene {
  Actor pet[2];
  bool ball, rope;
  int16_t ballX, ballY, knotX;
  int rally, best, secondsLeft;  // secondsLeft: tug of war, -1 before "Pull!"
};

// A packet to send.
struct Out {
  uint8_t ev, actor;
  uint32_t value;
};

inline int32_t lerp(int32_t a, int32_t b, int32_t num, int32_t den) { return a + (b - a) * num / den; }

class Game {
 public:
  uint8_t act = 0;  // meet::CATCH or meet::TUG
  int me = 0;       // this device's Pet: 0 on the left device, 1 on the right
  bool over = false, quit = false;
  int32_t endAt = 0;  // when it ended (game time, ms from the start); done END_MS later (at once after a quit)

  // Catch: the throw in the air (or about to leave), and the one before it (still flying, or being chased).
  struct Throw {
    int32_t at = 0;    // when it leaves
    int8_t from = 0;   // the thrower's Pet
    int8_t rally = 0;  // the rally when it was thrown (its speed)
    int16_t height = 50;
    Result result = UNKNOWN;
    int32_t flight() const { return flightMs(rally); }
    int32_t arrives() const { return at + flight(); }
    int to() const { return 1 - from; }
  };
  Throw cur, prev;
  int k = 0;  // cur's number
  int rally = 0, best = 0, drops = 0;

  // Tug of war
  int pulls[2] = {0, 0};
  int winner = -1;  // 0, 1, 2 (a tie), -1 not yet
  int16_t finalShift = 0;

  // Starts a game, with the Pets standing at x0, x1 (they walk to their places first).
  void begin(uint8_t whichAct, uint32_t gameSeed, int mine, int16_t x0 = meet::HOME[0], int16_t x1 = meet::HOME[1]) {
    *this = Game();
    act = whichAct, me = mine & 1, seed = gameSeed;
    startX[0] = x0, startX[1] = x1;
    cur.at = INTRO_MS;
    cur.height = heightOf(0);
  }

  // B went down on this device at game time t.
  void press(int32_t t) {
    if (over || t < 0) return;
    if (act == meet::CATCH) {
      if (cur.to() != me || t < cur.at || t > cur.arrives() + WINDOW_MS) return;  // not ours to catch
      settle(t >= cur.arrives() - WINDOW_MS ? CAUGHT : DROPPED);  // too soon is a drop
    } else if (act == meet::TUG && t >= TUG_GO && t < TUG_GO + TUG_MS) {
      pulls[me]++;
      send(EV_PULLS, 0, pulls[me]);
    }
  }

  // A GAME packet from the other device, heard at game time t.
  void heard(uint8_t ev, uint8_t actor, uint32_t value, int32_t t) {
    if (actor >> 2 != tag()) return;  // another game's
    actor &= 3;
    if (ev == EV_QUIT) {
      if (!over) over = quit = true, endAt = t;
      return;
    }
    if (over) return;
    if (act == meet::CATCH && ev == EV_CATCH && (int)value == k && cur.to() != me && actor && actor <= DROPPED)
      settle((Result)actor);
    else if (act == meet::TUG && ev == EV_PULLS && (int)value > pulls[1 - me])
      pulls[1 - me] = value;
    else if (act == meet::TUG && ev == EV_WIN && me == 1)
      finish(actor > 2 ? 2 : actor, (int16_t)(int32_t)value, t);
  }

  // Time passes: a ball nobody caught is dropped, the tug's verdict, or the other device has gone quiet.
  void tick(int32_t t) {
    if (over) return;
    if (act == meet::CATCH) {
      if (cur.to() == me && t > cur.arrives() + WINDOW_MS) settle(DROPPED);
      else if (cur.to() != me && t > cur.arrives() + WINDOW_MS + LOST_MS) over = true, endAt = t;
    } else if (act == meet::TUG) {
      const int s = rawShift();
      const bool timeUp = t >= TUG_GO + TUG_MS;
      if (me == 0 && (s <= -WIN_PX || s >= WIN_PX || timeUp)) {
        finish(s < 0 ? 0 : s > 0 ? 1 : 2, shift(), t);
        send(EV_WIN, winner, (uint32_t)(int32_t)finalShift);
      } else if (me == 1 && t >= TUG_GO + TUG_MS + LOST_MS) {  // the verdict never came: this side's own
        finish(s < 0 ? 0 : s > 0 ? 1 : 2, shift(), t);
      }
    }
  }

  // Holding A here: over for both.
  void leave(int32_t t) {
    if (!over) over = quit = true, endAt = t;
    send(EV_QUIT, 0, 0);
  }

  bool done(int32_t t) const { return over && t >= endAt + (quit ? 0 : END_MS); }

  // A packet to send now (once), and the latest one, which is sent again every so often in case it got lost.
  bool takeOut(Out &o) {
    if (!fresh) return false;
    fresh = false;
    o = out;
    return true;
  }
  bool latest(Out &o) const {
    o = out;
    return hasOut;
  }

  // Where the knot is: room x from the middle, negative toward the left Pet.
  int16_t shift() const {
    if (winner >= 0) return finalShift;
    const int s = rawShift();
    return s < -WIN_PX ? -WIN_PX : s > WIN_PX ? WIN_PX : s;
  }

  void scene(int32_t t, Scene &s) const {
    s = Scene();
    s.rally = rally, s.best = best, s.secondsLeft = -1;
    for (int i = 0; i < 2; i++) s.pet[i].x = startX[i];
    if (act == meet::CATCH) catchScene(t, s);
    else tugScene(t, s);
  }

 private:
  uint32_t seed = 0;
  int16_t startX[2] = {meet::HOME[0], meet::HOME[1]};
  Out out = {};
  bool fresh = false, hasOut = false;

  uint8_t tag() const { return seed >> 26; }  // 6 bits of the seed both devices started with

  void send(uint8_t ev, uint8_t actor, uint32_t value) {
    out = {ev, (uint8_t)(tag() << 2 | (actor & 3)), value};
    fresh = hasOut = true;
  }

  int16_t heightOf(int n) const {  // each throw's arc, the same on both devices
    meet::Dice d = {seed ^ (uint32_t)n * 2654435761u};
    d.next();
    return 36 + d.below(28);
  }

  // How the current throw went (judged here, or heard): the next throw, or game over.
  void settle(Result r) {
    const bool mine = cur.to() == me;
    cur.result = r;
    prev = cur;
    Throw next;
    next.from = prev.to();  // the catcher throws next (after running for it if it dropped it)
    if (r == CAUGHT) {
      rally++;
      if (rally > best) best = rally;
      drops = 0;
      next.at = prev.arrives() + HOLD_MS;
    } else {
      rally = 0;
      drops++;
      next.at = prev.arrives() + WINDOW_MS + CHASE_MS;
      if (drops >= MAX_DROPS) over = true, endAt = prev.arrives() + WINDOW_MS;
    }
    next.rally = rally;
    next.height = heightOf(k + 1);
    if (mine) send(EV_CATCH, r, k);
    cur = next;
    k++;
  }

  void finish(int who, int16_t at, int32_t t) {
    winner = who, finalShift = at;
    over = true, endAt = t;
  }

  int rawShift() const { return (pulls[1] - pulls[0]) * PULL_PX; }

  // The two walk from where they stood to `to` over the first ms of the game.
  void walkIn(int32_t t, int32_t ms, const int16_t *to, Scene &s) const {
    for (int i = 0; i < 2; i++) {
      if (t >= ms) {
        s.pet[i].x = to[i];
        continue;
      }
      s.pet[i].x = lerp(startX[i], to[i], t < 0 ? 0 : t, ms);
      s.pet[i].dy = (t / 150) % 2 ? -4 : 0;
      s.pet[i].mood = meet::HAPPY;
    }
  }

  static int16_t pawX(int who, int16_t petX) { return who == 0 ? petX + meet::PET_W - 4 : petX + 4; }

  void catchScene(int32_t t, Scene &s) const {
    walkIn(t, 1200, CATCH_X, s);
    if (t >= 1200 && t < INTRO_MS) s.pet[0].bubble = meet::SAY_CATCH;
    if (over && t >= endAt) {  // the end: both pleased with themselves
      for (int i = 0; i < 2; i++) s.pet[i].mood = meet::HAPPY, s.pet[i].heart = 2;
      s.pet[0].bubble = meet::SAY_GOOD_GAME;
      return;
    }
    const Throw &f = k > 0 && t < cur.at ? prev : cur;  // the throw to draw: the one before, until this one leaves
    const int16_t from = pawX(f.from, CATCH_X[f.from]), to = pawX(f.to(), CATCH_X[f.to()]);
    s.ball = true;
    if (t < f.at) {  // in the thrower's paw, about to throw
      s.ballX = from, s.ballY = PAW_Y;
    } else if (t < f.arrives()) {  // in the air: an arc
      const int32_t u = t - f.at, n = f.flight();
      s.ballX = lerp(from, to, u, n);
      s.ballY = PAW_Y - (int32_t)4 * f.height * u / n * (n - u) / n;
      if (u < 200) s.pet[f.from].dy = -4;  // the throw
    } else if (f.result == DROPPED && t >= f.arrives() + WINDOW_MS) {
      chase(f.to(), t - (f.arrives() + WINDOW_MS), s);
    } else {  // arrived: in the catcher's paw (caught, or not judged yet)
      s.ballX = to, s.ballY = PAW_Y;
      if (f.result == CAUGHT) s.pet[f.to()].mood = meet::HAPPY, s.pet[f.to()].dy = -4;
    }
  }

  // A dropped ball rolls off past the Pet's outer edge; it runs after it and brings it back.
  static void chase(int who, int32_t u, Scene &s) {
    const int16_t home = CATCH_X[who], away = who == 0 ? -meet::PET_W - 10 : meet::ROOM_W + 10;
    const int16_t paw = pawX(who, home), edge = who == 0 ? -12 : meet::ROOM_W + 12;
    const int32_t n = CHASE_MS;
    Actor &a = s.pet[who];
    if (u < n / 4) a.bubble = meet::SAY_OOPS;
    if (u < n * 3 / 10) {  // rolling away, bouncing
      s.ballX = lerp(paw, edge, u, n * 3 / 10);
      s.ballY = PAW_Y + 30 - ((u / 120) % 2 ? 10 : 0);
    } else {
      s.ball = false;  // off the screen
    }
    if (u >= n / 10 && u < n / 2) a.x = lerp(home, away, u - n / 10, n * 4 / 10);
    else if (u >= n / 2 && u < n * 9 / 10) a.x = lerp(away, home, u - n / 2, n * 4 / 10);
    else a.x = home;
    if (u >= n / 10 && u < n * 9 / 10) a.dy = (u / 150) % 2 ? -4 : 0, a.mood = meet::HAPPY;
    if (u >= n / 2) {  // back with it
      s.ball = true;
      s.ballX = pawX(who, a.x), s.ballY = PAW_Y;
    }
  }

  void tugScene(int32_t t, Scene &s) const {
    walkIn(t, 1000, TUG_X, s);
    if (t < 1000) return;
    const int16_t sh = shift();
    for (int i = 0; i < 2; i++) s.pet[i].x = TUG_X[i] + sh;
    s.rope = true;
    s.knotX = meet::SCREEN_W + sh;
    if (t < TUG_GO) {  // 3, 2, 1
      const uint8_t n = t < 1200 ? 0 : t < 1800 ? meet::SAY_3 : t < 2400 ? meet::SAY_2 : meet::SAY_1;
      s.pet[0].bubble = s.pet[1].bubble = n;
      return;
    }
    if (!over || t < endAt) {
      const int32_t left = TUG_GO + TUG_MS - t;
      s.secondsLeft = left > 0 ? (left + 999) / 1000 : 0;
      if (t < TUG_GO + 600) s.pet[0].bubble = s.pet[1].bubble = meet::SAY_PULL;
      for (int i = 0; i < 2; i++) s.pet[i].dy = pulls[i] % 2 ? -3 : 0;  // a bob with each pull
      return;
    }
    if (winner == 2) {  // a tie
      for (int i = 0; i < 2; i++) s.pet[i].bubble = meet::SAY_TIE, s.pet[i].mood = meet::HAPPY;
      return;
    }
    Actor &won = s.pet[winner], &lost = s.pet[1 - winner];
    won.mood = meet::HAPPY, won.heart = 2, won.bubble = meet::SAY_I_WIN;
    if (t - endAt < 800) lost.dy = 3, lost.mood = meet::SLEEPY, lost.bubble = meet::SAY_OOPS;  // fell over
    else lost.mood = meet::HAPPY, lost.bubble = meet::SAY_HEHE;    // and gets up laughing
  }
};

// The best catch rally with each Pet played with (by its id), kept in NVS ("pet_rallies") apart from the friends
// list, so that list's saved form doesn't change. The oldest entry goes when it's full.
struct Rallies {
  static const int MAX = 16;
  uint32_t id[MAX];
  uint16_t best[MAX];
  uint8_t count;

  int of(uint32_t who) const {
    for (int i = 0; i < count; i++)
      if (id[i] == who) return best[i];
    return 0;
  }
  // True if it's a new best (and kept).
  bool record(uint32_t who, int rally) {
    if (rally <= of(who)) return false;
    int i = 0;
    while (i < count && id[i] != who) i++;
    if (i == count) {
      if (count == MAX) {  // forget the oldest
        memmove(id, id + 1, sizeof id - sizeof *id);
        memmove(best, best + 1, sizeof best - sizeof *best);
        count--;
      }
      i = count++;
    }
    id[i] = who, best[i] = rally > 65535 ? 65535 : rally;
    return true;
  }
};

}  // namespace duo

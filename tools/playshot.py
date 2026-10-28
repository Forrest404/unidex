#!/usr/bin/env python3
"""Drives two unidex boards on the test build (pio run -e dev) through the Meet games: catch and tug of war.

  ~/.platformio/penv/bin/python tools/playshot.py [--out shots/play] [--ports PORT_A PORT_B]

Both open Pet > Meet and connect, as tools/meetshot.py does ("Be friends?" answered "not now" if asked). Then, each
game started from board A's hold-B menu:
  catch: each board catches the throws to it (X TAP B just before the ball arrives, from its "XG throw" line):
         throws 0-5, not 6, then 7-9, then none, so three drops end it. Both must end with a best rally of 6.
  tug of war: board A pulls about 7 times a second, B 4 times: A's Pet wins on both.
  tug of war, nobody pulls: a tie when the time's up, on both.
  catch, then B holds A (X HOLD A): it ends on both at once.
The game's own lines ("XG", one a frame) are compared across both. Screenshots from both (a-*.png, b-*.png): a throw
in the air, the rope mid-tug, and the room after. Keep the boards side by side.

A best rally is saved on both boards (NVS "pet_rallies", against the other's Pet). To forget it afterwards:
X KEY pet_rallies none on each, then restart them.
"""
import argparse, os, re, sys, threading, time, zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from devshot import Device, write_png  # noqa: E402
from meetshot import boards, open_pet, wait_for  # noqa: E402

CATCH_ROW, TUG_ROW = 3, 4  # in hold B's menu: Swap screens, Send a badge, High five, Catch, Tug of war


def field(s, n):
    f = s["detail"].split(":")
    return f[n] if s["detail"].startswith("meet:") and len(f) > n else ""


def parse(line):
    """'XG t=1 k=2 pulls=3,4 ...' -> {'t': 1, 'k': 2, 'pulls': (3, 4), ...}"""
    out = {}
    for k, v in re.findall(r"(\w+)=(-?[\d,]+)", line):
        out[k] = tuple(map(int, v.split(","))) if "," in v else int(v)
    return out


def shot(d, handle):
    """A screenshot mid-game: the game's own lines (and replies to taps) that come in meanwhile go to handle()."""
    d.s.write(b"X SHOT\n")
    rows, end = [], time.time() + 10
    while time.time() < end:
        l = d.s.readline().decode(errors="replace").strip()
        if l.startswith("XD "):
            rows.append(l[3:])
        elif l.startswith("OK X SHOT"):
            data = bytes.fromhex("".join(rows))
            if len(data) != 5000 or zlib.crc32(data) != int(l.split()[-1]):
                raise RuntimeError("screenshot damaged in transfer")
            return data
        elif l and not l.startswith("XS "):
            handle(l)
    raise TimeoutError("no screenshot")


def open_menu(d, row):
    """Hold B's menu on board d, at `row`. The room's own acts come every 8-12 s, and a press during one is refused
    ("busy"), so each step is checked and done again if it didn't happen."""
    for _ in range(30):
        menu = d.state()["detail"].rsplit(":", 1)[-1]
        if menu == f"menu{row}":
            return
        d.press("a" if menu.startswith("menu") or menu == "ask" else "B")  # ("Be friends?": not now)
    raise RuntimeError("couldn't open the menu")


class Game:
    """Starts a game from board A's menu and plays it on both: catches throws (catch(k) says which), pulls every
    pull[i] seconds, holds A on board i at quit_at[i] seconds into the game, takes screenshots at `shots` (game ms,
    or "end+ms": that long after it ended)."""

    def __init__(self, devs, out, name):
        self.devs, self.out, self.name = devs, out, name
        self.frames, self.ends, self.caught = [[], []], [None, None], [[], []]

    def play(self, row, catch=lambda k: False, pull=(0, 0), quit_at=(None, None), shots=(), seconds=60):
        A = self.devs[0]
        for d in self.devs:
            d.s.reset_input_buffer()  # (late replies to taps from the last game)
        open_menu(A, row)
        stop = time.time() + seconds

        def run(j):
            d = self.devs[j]
            started, ended, next_pull, shot_at = None, None, 0, list(shots)

            def handle(l):
                nonlocal started, ended
                if l.startswith("XG throw "):
                    k, ms = map(int, l.split()[2::2])
                    if catch(k):
                        self.caught[j].append(k)
                        threading.Timer(max(0, ms - 120) / 1000, lambda: d.s.write(b"X TAP B\n")).start()
                elif l.startswith("XG end"):
                    self.ends[j] = parse(l)
                elif l.startswith("XG t="):
                    g = parse(l)
                    self.frames[j].append((time.time(), g))
                    if started is None:
                        started = time.time() - g["t"] / 1000
                    if g["over"] and ended is None:
                        ended = g["t"]

            while time.time() < stop and self.ends[j] is None:
                l = d.s.readline().decode(errors="replace").strip()
                if l:
                    handle(l)
                if started is None:
                    continue
                t = time.time() - started
                if pull[j] and t > 3.05 and time.time() >= next_pull:
                    d.s.write(b"X TAP B\n")
                    next_pull = time.time() + pull[j]
                if quit_at[j] is not None and t >= quit_at[j]:
                    d.s.write(b"X HOLD A\n")
                    return self.finish(j, d, handle)
                due = None
                if shot_at and isinstance(shot_at[0], int):
                    due = shot_at[0]
                elif shot_at and ended is not None:
                    due = ended + int(shot_at[0][4:])
                if due is not None and t * 1000 >= due:
                    tag = shot_at.pop(0)
                    write_png(os.path.join(self.out, f"{'ab'[j]}-{self.name}-{tag}.png"), shot(d, handle))

        ths = [threading.Thread(target=run, args=(j,)) for j in range(2)]
        for t in ths:
            t.start()
        A.s.write(b"X BTN b\n")  # choose: the game starts (the reply only comes after it, so it isn't waited for)
        for t in ths:
            t.join()
        time.sleep(1.5)
        for d in self.devs:
            d.s.reset_input_buffer()
        return self.ends

    def finish(self, j, d, handle):
        end = time.time() + 10
        while time.time() < end and self.ends[j] is None:
            l = d.s.readline().decode(errors="replace").strip()
            if l:
                handle(l)

    def last(self, j):
        return self.frames[j][-1][1] if self.frames[j] else {}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default="shots/play")
    ap.add_argument("--ports", nargs=2)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    ports = args.ports or boards()[:2]
    if len(ports) < 2:
        sys.exit("needs two boards")
    devs = [Device(p) for p in ports]
    problems = []

    def check(what, ok, info=""):
        print(f"{'ok  ' if ok else 'FAIL'} {what}" + (f": {info}" if info else ""))
        if not ok:
            problems.append(what)

    for d in devs:
        open_pet(d)
        d.press("B")  # Meet
    states, ok = wait_for(devs, lambda s: field(s, 3) == "searching" and field(s, 8) == "1", 20)
    if not ok:
        sys.exit("the boards don't see each other in reach")
    devs[0].press("b")  # connect
    states, ok = wait_for(devs, lambda s: field(s, 3) == "connected" and field(s, 11) not in ("", "t0"), 10)
    check("connected, and each knows the other can play games", ok and all(int(field(s, 11)[1:]) & 8 for s in states),
          " ".join(field(s, 11) for s in states))
    time.sleep(4)  # the greeting
    for d in devs:
        if d.state()["detail"].endswith(":ask"):
            d.press("a")  # not now
    time.sleep(1)
    me = [0 if field(d.state(), 4) == "left" else 1 for d in devs]  # each board's Pet in the room

    # catch: 0-5 caught, 6 dropped, 7-9 caught, then nobody catches
    g = Game(devs, args.out, "catch")
    ends = g.play(CATCH_ROW, catch=lambda k: k < 6 or 7 <= k < 10, shots=(3000, 4500, "end+200"))
    check("catch ends on both", all(ends), str(ends))
    if all(ends):
        check("catch: a best rally of 6 on both", ends[0]["best"] == ends[1]["best"] == 6,
              f"{ends[0]['best']}/{ends[1]['best']}")
        check("catch: both counted the same throws", g.last(0).get("k") == g.last(1).get("k") == 13,
              f"{g.last(0).get('k')}/{g.last(1).get('k')} (caught on A: {g.caught[0]}, B: {g.caught[1]})")
    data, _ = devs[0].shot()
    write_png(os.path.join(args.out, "a-catch-after.png"), data)
    toast = devs[0].state().get("toast", "")
    print("  toast on A:", bytes.fromhex(toast).decode() if re.fullmatch(r"[0-9a-f]*", toast) else toast)

    # tug of war: A pulls faster
    g = Game(devs, args.out, "tug")
    ends = g.play(TUG_ROW, pull=(0.14, 0.25), shots=(6000, "end+200", "end+1200"))
    check("tug of war ends on both", all(ends), str(ends))
    if all(ends):
        check("tug of war: A's Pet wins on both", ends[0]["winner"] == ends[1]["winner"] == me[0],
              f"{ends[0]['winner']}/{ends[1]['winner']}, A is Pet {me[0]}, pulls {g.last(0).get('pulls')}")
    time.sleep(2)

    # tug of war: nobody pulls
    g = Game(devs, args.out, "tie")
    ends = g.play(TUG_ROW, shots=("end+200",))
    check("tug of war, nobody pulling: a tie on both", all(ends) and ends[0]["winner"] == ends[1]["winner"] == 2,
          str(ends))
    time.sleep(2)

    # catch, and B holds A
    g = Game(devs, args.out, "quit")
    ends = g.play(CATCH_ROW, catch=lambda k: True, quit_at=(None, 5.0), seconds=20)
    check("holding A on B ends the game on both", all(ends) and ends[0]["quit"] == ends[1]["quit"] == 1, str(ends))
    time.sleep(2)
    states = [d.state() for d in devs]
    check("both back in the room, still connected", all(field(s, 3) == "connected" for s in states))

    devs[1].press("A")  # B leaves Meet: parted
    wait_for(devs[:1], lambda s: field(s, 3) == "searching", 20)
    for d in devs:
        if d.state()["screen"] == "Pet" and not d.state()["detail"].startswith("main:"):
            d.press("A")
    for p in problems:
        print("PROBLEM:", p)
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()

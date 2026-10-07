#!/usr/bin/env python3
"""Drives two unidex boards on the test build at once (pio run -e dev), for the Pet's Meet screen over the radio.

  ~/.platformio/penv/bin/python tools/meetshot.py [--out shots/meet] [--ports PORT_A PORT_B]

Both open Pet > Meet and search; once each has the other in reach, B on board A connects them (the two screens
become one room); the greeting plays (then "Be friends?", answered "not now" on both: tools/friendshot.py tests
friends); A's Pet visits B's screen; B swaps them (hold B's menu: Swap screens); then B leaves Meet and A should
notice they've been parted. Times every act frame on both boards ("XW" lines) to check they stay in step, and
saves screenshots from both (a-*.png, b-*.png). Keep the boards side by side. Pause the Mac agent first.
"""
import argparse, os, sys, threading, time
from serial.tools import list_ports

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from devshot import Device, write_png  # noqa: E402


def boards():
    return sorted(p.device for p in list_ports.comports() if p.vid == 0x303A)


def open_pet(dev):
    """Opens the Pet on its main screen (not Dress up, Name or Meet, where presses would change things)."""
    for _ in range(12):  # home: step to Pet
        st = dev.state()
        if st["screen"] == "Pet" and st["detail"].startswith("main:"):
            break
        if st["screen"] == "Pet":  # in Dress up, Name or Meet: back out (hold A) to the main screen
            dev.press("A")
            continue
        if st["screen"] != "Home":
            dev.press("A")
        elif st["sel"] == "Pet":
            dev.press("b")
        else:
            dev.press("a")
    assert dev.state()["screen"] == "Pet", "couldn't open Pet"


def wait_for(devs, check, seconds):
    end = time.time() + seconds
    while time.time() < end:
        states = [d.state() for d in devs]
        if all(check(s) for s in states):
            return states, True
        time.sleep(0.5)
    return [d.state() for d in devs], False


class Frames:
    """Collects the XW lines (one per act frame) both boards print, with the time each arrived."""

    def __init__(self, devs):
        self.devs, self.times, self.lock = devs, [[], []], threading.Lock()

    def press_and_watch(self, i, key, seconds):
        """Presses on board i (without waiting for its reply) and records XW lines from both for `seconds`."""
        self.times, self.reply = [[], []], None
        stop = time.time() + seconds
        def watch(j):
            s = self.devs[j].s
            while time.time() < stop:
                l = s.readline().decode(errors="replace").strip()
                if l.startswith("XW "):
                    self.times[j].append(time.time())
                elif l.startswith(("OK X BTN", "ERR")) and j == i:
                    self.reply = l
        ths = [threading.Thread(target=watch, args=(j,)) for j in range(2)]
        for t in ths:
            t.start()
        self.devs[i].s.write(f"X BTN {key}\n".encode())
        for t in ths:
            t.join()
        for d in self.devs:
            d.s.reset_input_buffer()
        a, b = self.times
        n = min(len(a), len(b))
        worst = max((abs(x - y) for x, y in zip(a, b)), default=0) * 1000
        return len(a), len(b), worst if n else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="shots/meet")
    ap.add_argument("--ports", nargs=2)
    a = ap.parse_args()
    ports = a.ports or boards()
    if len(ports) < 2:
        sys.exit(f"need two boards, found {len(ports)}")
    os.makedirs(a.out, exist_ok=True)
    devs = [Device(p) for p in ports[:2]]
    problems = []

    def shot(tag):
        for d, n in zip(devs, "ab"):
            data, _ = d.shot()
            write_png(os.path.join(a.out, f"{n}-{tag}.png"), data)

    for d in devs:
        open_pet(d)
        d.press("B")  # hold B: Meet
    def in_reach(s):
        f = s["detail"].split(":")
        return len(f) > 8 and f[3] == "searching" and f[8] == "1"  # what makes the screen say "press B"
    states, ok = wait_for(devs, in_reach, 15)
    print("both searching, in reach:", ok, [s["detail"] for s in states])
    if not ok:
        problems.append("not in reach of each other (keep the boards side by side)")
    shot("1-searching")
    frames = Frames(devs)
    for step, (board, key, secs) in enumerate([(0, "b", 14), (0, "b", 24), (1, "b", 12)], 2):
        if step == 4:  # swap: hold B opens the menu; "Swap screens" is its first row
            devs[board].press("B")
        na, nb, worst = frames.press_and_watch(board, key, secs)
        what = ["connect + greeting", "visit", "swap"][step - 2]
        print(f"{what}: frames A {na}, B {nb}, worst difference {worst if worst is None else round(worst)} ms"
              f"  (press: {frames.reply})")
        if na == 0 or na != nb or worst is None or worst > 80:
            problems.append(f"{what}: not in step (A {na} frames, B {nb}, worst {worst})")
        states, ok = wait_for(devs, lambda s: ":connected:" in s["detail"], 10)
        if not ok:
            problems.append(f"after {what}: {[s['detail'] for s in states]}")
        if step == 2:  # strangers: "Be friends?" comes after the greeting; answer "not now" on both for this test
            wait_for(devs, lambda s: s["detail"].endswith(":ask"), 10)
            for d in devs:  # (one "not now" closes the question on the other board too)
                if d.state()["detail"].endswith(":ask"):
                    d.press("a")
            time.sleep(1)
        shot(f"{step}-after-{what.split()[0]}")
    devs[1].press("A")  # B leaves Meet: A should notice they've been parted
    states, ok = wait_for(devs[:1], lambda s: ":searching:" in s["detail"], 20)
    print("A noticed they were parted:", ok, states[0]["detail"])
    if not ok:
        problems.append("A didn't notice B leaving")
    shot("5-parted")
    devs[0].press("A")
    for d in devs:
        d.press("A")
    for p in problems:
        print("PROBLEM:", p)
    print(f"screenshots in {a.out}")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()

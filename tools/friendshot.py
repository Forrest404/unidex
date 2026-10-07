#!/usr/bin/env python3
"""Two unidex boards on the test build (pio run -e dev): the Pet's friends, end to end.

  ~/.platformio/penv/bin/python tools/friendshot.py [--out shots/friends] [--ports PORT_A PORT_B] [--orders]

Meet -> "Be friends?" on both -> both say yes -> the celebration -> parted -> meet again (the friends' greeting) ->
the friends list -> remove the friend on both (so both end as they started). Screenshots from both. Keep the
boards side by side, neither with the other as a friend already. Pause the Mac agent first.

--orders instead tries every way two people can answer "Be friends?" (left first, right first, a moment apart, a
few seconds apart, at the same time) and checks both devices end up agreeing; each try starts from no friends (the
saved friends are cleared with X KEY and both boards restart).
"""
import argparse, os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from devshot import write_png, Device  # noqa: E402
from meetshot import boards, open_pet, wait_for  # noqa: E402


def field(s, n):
    return s["detail"].split(":")[n] if s["detail"].startswith("meet:") else ""


def reset(ports, clear=(0, 1)):
    """A clean start: the saved friends cleared on the boards in `clear`, then every board restarted (fresh memory).
    Retries while a board is busy with an animation (it answers "ERR busy" and drops the command)."""
    for i, p in enumerate(ports):
        d = Device(p)
        while i in clear and not d.cmd("X KEY pet_friends none")[-1].startswith("OK"):
            time.sleep(0.5)
        while d.cmd("X STATE")[-1] == "ERR busy":  # (between acts, the restart below is taken)
            time.sleep(0.5)
        d.s.write(b"X BTN R\n")
        d.s.close()
    time.sleep(12)


def orders(ports):
    """Every answer order and timing; returns 0 if both devices always agree."""
    bad = 0
    for first, gap in [(0, 0.0), (1, 0.0), (0, 0.15), (1, 0.15), (0, 0.6), (1, 0.6), (0, 3), (1, 3)]:
        reset(ports)
        A, B = Device(ports[0]), Device(ports[1])
        for d in (A, B):
            open_pet(d)
            d.press("B")
        wait_for((A, B), lambda s: field(s, 8) == "1", 20)
        A.press("b")
        _, ok = wait_for((A, B), lambda s: s["detail"].endswith(":ask"), 25)
        one, two = (A, B) if first == 0 else (B, A)
        one.s.write(b"X BTN b\n")  # without waiting for the reply, so the gap can be short
        time.sleep(gap)
        two.s.write(b"X BTN b\n")
        time.sleep(14)  # the celebration
        for d in (A, B):
            d.s.reset_input_buffer()
        fa, fb = field(A.state(), 9), field(B.state(), 9)
        agree = ok and fa == fb == "f1"
        bad += not agree
        print(f"{'left' if first == 0 else 'right'} says yes first, other {gap:.2f} s later: A {fa}  B {fb}"
              f"  {'ok' if agree else 'DISAGREE'}")
    reset(ports)  # leave both with no friends
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="shots/friends")
    ap.add_argument("--ports", nargs=2)
    ap.add_argument("--orders", action="store_true")
    a = ap.parse_args()
    if a.orders:
        sys.exit(orders(a.ports or boards()))
    ports = a.ports or boards()
    os.makedirs(a.out, exist_ok=True)
    A, B = [Device(p) for p in ports[:2]]
    problems = []

    def shot(tag, devs=(A, B)):
        for d in devs:
            data, _ = d.shot()
            write_png(os.path.join(a.out, f"{'ab'[d is B]}-{tag}.png"), data)

    def expect(devs, check, secs, what):
        states, ok = wait_for(devs, check, secs)
        print(f"{what}: {ok}  {[s['detail'] for s in states]}")
        if not ok:
            problems.append(what)
        return ok

    def enter_meet(d):
        open_pet(d)
        d.press("B")

    for d in (A, B):
        enter_meet(d)
    expect((A, B), lambda s: field(s, 8) == "1" and field(s, 9) == "f0", 15, "both searching, in reach, no friends")
    A.press("b")  # connect; the greeting plays, then the question
    expect((A, B), lambda s: s["detail"].endswith(":ask"), 20, "'Be friends?' on both")
    shot("1-ask")
    A.press("b")  # yes
    time.sleep(1)
    shot("2-a-said-yes", (A,))
    B.press("b")  # yes: friends, and the celebration
    expect((A, B), lambda s: field(s, 9) == "f1" and not s["detail"].endswith(":ask"), 30, "friends on both")
    time.sleep(2)
    shot("3-friends")
    B.press("A")  # B leaves Meet: parted
    expect((A,), lambda s: field(s, 3) == "searching", 15, "A back to searching")
    B.press("B")  # B back into Meet
    expect((A, B), lambda s: field(s, 8) == "1", 15, "in reach again")
    A.press("b")  # meet again: the friends' greeting, no question
    expect((A, B), lambda s: field(s, 3) == "connected" and not s["detail"].endswith(":ask"), 20,
           "met again, no question")
    time.sleep(2)
    shot("4-met-again")
    B.press("A")
    expect((A,), lambda s: field(s, 3) == "searching", 15, "parted again")
    for d in (A, B):
        if field(d.state(), 3) != "searching":
            enter_meet(d)
        d.press("a")  # the friends list
    expect((A, B), lambda s: s["detail"].endswith(":list"), 5, "friends list open")
    shot("5-list")
    for d in (A, B):
        d.press("B")  # hold B: remove?
    shot("6-remove", (A,))
    for d in (A, B):
        d.press("b")  # remove
    expect((A, B), lambda s: field(s, 9) == "f0", 5, "friend removed on both")
    for d in (A, B):
        d.press("A")
        d.press("A")
    for p in problems:
        print("PROBLEM:", p)
    print(f"screenshots in {a.out}")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Two unidex boards on the test build (pio run -e dev): sending a badge to a friend, end to end.

  ~/.platformio/penv/bin/python tools/badgeshot.py [--out shots/badge-send] [--ports SENDER RECEIVER]

The two become friends; the sender (it needs badges on its card) opens hold B's menu, "Send a badge", sends the
first one; the receiver sees the preview: first "keep" (with no SD card it says it can't), then a second send and
"no thanks". The friendship is removed on both at the end. Screenshots from both. Nothing is saved on the sender.
"""
import argparse, os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from devshot import write_png, Device  # noqa: E402
from meetshot import boards, open_pet, wait_for  # noqa: E402


def detail(d):
    return d.state()["detail"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="shots/badge-send")
    ap.add_argument("--ports", nargs=2)
    a = ap.parse_args()
    ports = a.ports or boards()
    os.makedirs(a.out, exist_ok=True)
    S, R = [Device(p) for p in ports[:2]]
    problems = []

    def shot(tag, devs=(S, R)):
        for d in devs:
            data, _ = d.shot()
            write_png(os.path.join(a.out, f"{'sr'[d is R]}-{tag}.png"), data)

    def expect(devs, check, secs, what):
        states, ok = wait_for(devs, lambda s: check(s["detail"]), secs)
        print(f"{what}: {ok}  {[s['detail'][-40:] for s in states]}")
        if not ok:
            problems.append(what)
        return ok

    for d in (S, R):
        open_pet(d)
        d.press("B")
    expect((S, R), lambda x: x.split(":")[8] == "1" and ":f0:" in x, 15, "searching, in reach, no friends")
    S.press("b")
    expect((S, R), lambda x: x.endswith(":ask"), 20, "'Be friends?'")
    S.press("b")
    R.press("b")
    expect((S, R), lambda x: ":f1:" in x and ":connected:" in x and not x.endswith("ask"), 30, "friends")
    time.sleep(6)  # let the celebration end

    def send_one(tag):
        S.press("B")  # hold B: the menu
        S.press("a")  # "Send a badge"
        shot(f"{tag}-0-menu", (S,))
        S.press("b")
        expect((S,), lambda x: x.endswith("pick"), 10, f"{tag}: picking a badge")
        shot(f"{tag}-1-pick", (S,))
        t0 = time.time()
        S.s.write(b"X BTN b\n")  # send (the reply comes after the transfer)
        ok = expect((R,), lambda x: x.endswith("preview"), 20, f"{tag}: preview on the receiver")
        print(f"   {tag}: from B to the preview: {time.time() - t0:.1f} s")
        S.reply("", "OK X BTN", timeout=15)  # the sender's reply to the press, once the transfer is done
        if not ok:
            print("   sender log:", [l for l in S.log if l.startswith("XM")][-6:])
            print("   receiver log:", [l for l in R.log if l.startswith("XM")][-6:])
        shot(f"{tag}-2-preview")
        return ok

    if send_one("keep"):
        R.press("b")  # keep: the prototype has no card, so "can't keep it"
        expect((S,), lambda x: not x.endswith("wait"), 10, "keep: the sender heard back")
        time.sleep(0.5)
        shot("keep-3-after")
    time.sleep(2)
    if send_one("nothanks"):
        R.press("a")  # no thanks
        expect((S,), lambda x: not x.endswith("wait"), 10, "no thanks: the sender heard back")
        time.sleep(0.5)
        shot("nothanks-3-after")
    # end as they started: not friends
    R.press("A")
    expect((S,), lambda x: ":searching:" in x, 15, "parted")
    R.press("B")
    for d in (S, R):
        d.press("a")
        d.press("B")
        d.press("b")
    expect((S, R), lambda x: ":f0:" in x, 5, "friend removed on both")
    for d in (S, R):
        d.press("A")
        d.press("A")
    for p in problems:
        print("PROBLEM:", p)
    print(f"screenshots in {a.out}")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()

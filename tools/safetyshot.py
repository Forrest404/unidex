#!/usr/bin/env python3
# Tests the safety features on one board running the test build (pio run -e dev), and puts everything back:
#   low battery     X BATTMV 3400: Notes and Dex refuse with "Battery too low"
#   flat battery    X BATTMV 3300: "Charge me" within ~31 s (the test build shows it but stays on), then a restart
#   watchdog        X HANG: the main loop stops; the board must restart by itself within ~40 s
#   safe saving     a calendar sent with a wrong checksum is refused and the saved one is untouched; with no SD card
#                   a correct one is refused too (it used to say OK)
# Nothing here writes to your calendar, Dex or notes, and the battery reading is put back to the real one.
#   python3 tools/safetyshot.py [port] [--skip flat,hang]
# Screens go to shots/safety/.
import os
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from devshot import Device, find_port, write_png  # noqa: E402

OUT = "shots/safety"
LOW = "Battery too low"


def toast(st):
    return bytes.fromhex(st.get("toast", "")).decode()


class Run:
    def __init__(self, port):
        self.port = port
        self.dev = Device(port)
        self.results = []
        os.makedirs(OUT, exist_ok=True)

    def check(self, ok, what):
        self.results.append((bool(ok), what))
        print(("ok    " if ok else "FAIL  ") + what)

    def shot(self, name):
        data, meta = self.dev.shot()
        write_png(os.path.join(OUT, name + ".png"), data)

    def home(self):
        for _ in range(6):
            if self.dev.state()["screen"] == "Home":
                return
            self.dev.press("A")

    def select(self, name):
        self.home()
        for _ in range(10):
            if self.dev.state()["sel"] == name:
                return True
            self.dev.press("a")
        return False

    def reconnect(self, seconds):
        """Waits for the board to come back after a restart; returns its state, or None."""
        try:
            self.dev.s.close()
        except Exception:
            pass
        end = time.time() + seconds
        while time.time() < end:
            time.sleep(1)
            try:
                dev = Device(self.port if os.path.exists(self.port) else find_port())
                dev.s.write(b"X STATE\n")  # a short wait: a frozen board keeps its USB port but never answers
                t = time.time() + 2
                while time.time() < t:
                    if dev.s.readline().decode("ascii", "replace").startswith("OK X STATE"):
                        self.dev = dev
                        return dev.state()
                dev.s.close()
            except Exception:
                continue
        return None

    def reply_lines(self, line, last, timeout=10):
        """Sends a non-X line ("": nothing, the reply to something already sent); returns the reply lines up to
        one starting with any of `last`."""
        if line:
            self.dev.s.write((line + "\n").encode())
        out, end = [], time.time() + timeout
        while time.time() < end:
            l = self.dev.s.readline().decode("ascii", "replace").strip()
            if l:
                out.append(l)
                if l.startswith(last):
                    return out
        raise TimeoutError(f"no reply to {line!r}")

    def card(self):
        """(has a card, size of /events.csv or None)."""
        lines = self.reply_lines("S", ("OK S end", "OK S none"))
        if lines[-1].startswith("OK S none"):
            return False, None
        for l in lines:
            p = l.split()
            if len(p) == 3 and p[0] == "F" and p[1] == "/events.csv":
                return True, int(p[2])
        return True, None

    # --- the tests ---

    def low_battery(self):
        self.dev.cmd("X FAKE 1")  # if a gate were missing, a note would get the fake cloud, never a real one
        self.dev.cmd("X BATTMV 3400")
        if self.select("Notes"):
            self.dev.press("b")  # open
            self.dev.hold_b(400)  # too short for a note even if the gate were missing
            st = self.dev.state()
            self.shot("low-notes")
            self.check(toast(st) == LOW, f"Notes refuses to record when low (toast: {toast(st)!r})")
        if self.select("Dex"):
            self.dev.press("b")
            if self.dev.state()["screen"] == "Dex":
                self.dev.press("b")  # scan
                st = self.dev.state()
                self.shot("low-dex")
                self.check(toast(st) == LOW, f"Dex refuses to scan when low (toast: {toast(st)!r})")
            else:
                print("      (Dex needs a card: skipped)")
        self.dev.cmd("X BATTMV 0")
        self.dev.cmd("X FAKE 0")
        self.home()

    def flat_battery(self):
        self.dev.cmd("X BATTMV 3300")
        end, st = time.time() + 40, {}
        while time.time() < end:
            st = self.dev.state()
            if st.get("flat") == "1":
                break
            time.sleep(1)
        self.dev.cmd("X BATTMV 0")
        if st.get("flat") == "1":
            self.shot("flat-charge-me")
        self.check(st.get("flat") == "1", "a flat battery shows Charge me within 40 s")
        self.dev.s.write(b"X BTN R\n")  # restart (the test build stayed on); no reply comes back
        time.sleep(2)
        st = self.reconnect(30)
        self.check(st is not None and st["screen"] == "Home", "back home after the restart")

    def watchdog(self):
        r = self.dev.cmd("X HANG")[-1]
        self.check(r == "OK X HANG", "X HANG accepted")
        t0 = time.time()
        time.sleep(20)  # still frozen: the watchdog waits 30 s
        st = self.reconnect(40)
        took = time.time() - t0
        # esp_reset_reason: 4 = panic, 6 = task watchdog
        self.check(st is not None and st.get("reset") in ("4", "6"),
                   f"restarted by the watchdog after {took:.0f} s (reset reason {st and st.get('reset')})")

    def safe_saving(self):
        has_card, size = self.card()
        crc_key = self.dev.cmd("X KEY events_crc")[-1].split()[-1]
        line = "2026-01-01,09:00,10:00,safety test,nowhere"
        self.dev.s.write(f"E 1 12345\n{line}\n".encode())  # a wrong checksum
        r = self.reply_lines("", ("OK E", "ERR"))[-1]
        _, size2 = self.card()
        self.check(r == "ERR" and size2 == size and self.dev.cmd("X KEY events_crc")[-1].split()[-1] == crc_key,
                   "a calendar with a wrong checksum is refused and the saved one is untouched")
        if not has_card:
            crc = zlib.crc32((line + "\n").encode())
            self.dev.s.write(f"E 1 {crc}\n{line}\n".encode())
            r = self.reply_lines("", ("OK E", "ERR"))[-1]
            self.check(r == "ERR", f"with no SD card a calendar is refused, not reported saved ({r})")
            r = self.reply_lines("B zz-test.bmp 10 1", ("OK B", "ERR"))[-1]
            self.check(r == "ERR", "with no SD card a badge upload is refused")
        else:
            print("      (card present: the no-card checks run on the prototype board)")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    skip = set()
    for a in sys.argv[1:]:
        if a.startswith("--skip="):
            skip |= set(a.split("=", 1)[1].split(","))
    run = Run(args[0] if args else find_port())
    try:
        run.home()
        run.low_battery()
        run.safe_saving()
        if "flat" not in skip:
            run.flat_battery()
        if "hang" not in skip:
            run.watchdog()
    finally:
        try:
            run.dev.cmd("X BATTMV 0")
            run.dev.cmd("X FAKE 0")
        except Exception:
            print("couldn't put the battery reading back: restart the board (hold A + B)")
    failed = [w for ok, w in run.results if not ok]
    print(f"\n{len(run.results) - len(failed)} passed, {len(failed)} failed; screens in {OUT}/")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

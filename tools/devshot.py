#!/usr/bin/env python3
"""Drive a unidex running the test build (pio run -e dev) and save screenshots.

  ~/.platformio/penv/bin/python tools/devshot.py run tools/walkthroughs/home.txt [--out shots/x] [--compare shots/y]
  ~/.platformio/penv/bin/python tools/devshot.py shot NAME [--out DIR]
  ~/.platformio/penv/bin/python tools/devshot.py press a b B ...   (a/b = short, A/B = long)

Walkthrough lines: home | select <app> | pick <game> | waitjob <s> | job | seed <n> | manual <0|1> | frames <n> | press <keys...> | hold B <ms> | shot <name> | expect screen=<name> | sleep <ms>
                   | dry|fake|netfail|nocard <0|1> | clock unset | # comment
Pause the Mac agent first (it shares the port). Each run ends with the switches off, and puts the clock back
if the run unset it.
"""
import argparse
import html
import os
import struct
import sys
import time
import zlib

import serial
from serial.tools import list_ports

SCALE = 3


def find_port():
    for p in list_ports.comports():
        if p.vid == 0x303A:
            return p.device
    sys.exit("No unidex found on USB.")


class Device:
    def __init__(self, port):
        self.s = serial.Serial()
        self.s.port, self.s.baudrate, self.s.timeout = port, 115200, 0.2
        self.s.rts = False  # leave DTR alone: pyserial sets DTR before RTS, and DTR low with RTS high resets the chip
        self.s.open()
        time.sleep(0.3)
        self.s.reset_input_buffer()
        self.log = []  # anything that isn't a reply (boot messages, crash reports)

    def cmd(self, line, timeout=60):
        """Sends a line; returns all reply lines up to and including the OK/ERR line."""
        self.s.write((line + "\n").encode())
        out, end = [], time.time() + timeout
        while time.time() < end:
            l = self.s.readline().decode("ascii", "replace").strip()
            if not l:
                continue
            out.append(l)
            if l.startswith("OK X") or l == "ERR":
                return out
            if not l.startswith(("XS ", "XD ")):
                self.log.append(l)
        raise TimeoutError(f"no reply to {line!r}")

    def state(self):
        r = self.cmd("X STATE")[-1]
        if not r.startswith("OK X STATE"):
            raise RuntimeError(f"not a test build? {r!r}")
        kv = dict(p.split("=", 1) for p in r.split()[3:])
        kv["screen"] = bytes.fromhex(kv["screen"]).decode()
        kv["sel"] = bytes.fromhex(kv["sel"]).decode()
        kv["detail"] = bytes.fromhex(kv.get("detail", "")).decode()
        return kv

    def press(self, key):
        r = self.cmd(f"X BTN {key}")[-1].split()
        return r[3], int(r[4])  # refresh kind, ms

    def hold_b(self, ms):
        r = self.cmd(f"X HOLD B {ms}", timeout=ms / 1000 + 90)[-1].split()
        return r[3], int(r[4])

    def shot(self):
        lines = self.cmd("X SHOT")
        head = next(l for l in lines if l.startswith("XS "))
        rows = [l[3:] for l in lines if l.startswith("XD ")]
        data = bytes.fromhex("".join(rows))
        crc = int(lines[-1].split()[-1])
        if len(data) != 5000 or zlib.crc32(data) != crc:
            raise RuntimeError("screenshot damaged in transfer")
        _, _, _, kind, ms, name = head.split()
        return data, {"kind": kind, "ms": int(ms), "screen": bytes.fromhex(name).decode()}


def pixel(data, x, y):
    return data[(y * 200 + x) >> 3] >> (7 - (x & 7)) & 1


def write_png(path, data):
    raw = bytearray()
    for y in range(200 * SCALE):
        raw.append(0)
        for x in range(200 * SCALE):
            raw.append(0 if pixel(data, x // SCALE, y // SCALE) else 255)
    def chunk(t, b):
        return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 200 * SCALE, 200 * SCALE, 8, 0, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def margin_ink(data):
    return sum(pixel(data, x, y) for y in range(200) for x in range(200) if x < 2 or y < 2 or x > 197 or y > 197)


class Run:
    def __init__(self, dev, out, compare):
        self.dev, self.out, self.compare = dev, out, compare
        self.tiles, self.problems, self.clock_touched, self.last = [], [], False, ("-", 0)
        os.makedirs(out, exist_ok=True)

    def shot(self, name):
        data, meta = self.dev.shot()
        write_png(os.path.join(self.out, name + ".png"), data)
        open(os.path.join(self.out, name + ".bin"), "wb").write(data)
        notes = []
        ink = margin_ink(data)
        if ink and meta["screen"] != "Badge":
            notes.append(f"{ink} px in the 2 px margin")
        if self.compare:
            old = os.path.join(self.compare, name + ".bin")
            if os.path.exists(old):
                prev = open(old, "rb").read()
                diff = sum(bin(a ^ b).count("1") for a, b in zip(prev, data))
                notes.append(f"{diff} px differ from before" if diff else "same as before")
        kind, ms = self.last
        self.tiles.append((name, meta["screen"], kind, ms, notes))
        print(f"  shot {name:28} {meta['screen']:10} last press: {kind} {ms} ms  {'; '.join(notes)}")

    def home(self):
        for _ in range(6):
            if self.dev.state()["screen"] == "Home":
                return
            self.last = self.dev.press("A")
        self.problems.append("couldn't get back to Home")

    def job(self):
        """(busy, step, gen, result, note id) from N JOB."""
        self.dev.s.write(b"N JOB\n")
        end = time.time() + 5
        while time.time() < end:
            l = self.dev.s.readline().decode("ascii", "replace").strip()
            if l.startswith("OK N JOB"):
                p = l.split(" ")
                return p[3], p[4], p[5], bytes.fromhex(p[6]).decode(), bytes.fromhex(p[7]).decode() if len(p) > 7 else ""
        raise TimeoutError("no reply to N JOB")

    def select(self, name):
        """Home, then A until `name` is highlighted."""
        self.home()
        for _ in range(10):
            if self.dev.state()["sel"] == name:
                return
            self.last = self.dev.press("a")
        self.problems.append(f"couldn't find {name} on the home screen")

    def pick(self, name):
        """In the Games list: A until `name` is highlighted."""
        for _ in range(8):
            if self.dev.state()["detail"] == "list:" + name:
                return
            self.last = self.dev.press("a")
        self.problems.append(f"couldn't highlight {name} in the Games list")

    def step(self, line):
        w = line.split()
        if not w or w[0].startswith("#"):
            return
        if w[0] == "home":
            self.home()
        elif w[0] == "select":
            self.select(" ".join(w[1:]))
        elif w[0] == "pick":
            self.pick(" ".join(w[1:]))
        elif w[0] == "press":
            for k in w[1:]:
                self.last = self.dev.press(k)
        elif w[0] == "hold" and w[1] == "B":
            self.last = self.dev.hold_b(int(w[2]))
        elif w[0] == "shot":
            self.shot(w[1])
        elif w[0] == "expect":
            k, v = " ".join(w[1:]).split("=", 1)
            got = self.dev.state().get(k)
            if got != v:
                self.problems.append(f"expected {k}={v}, got {got} (before shot {len(self.tiles)})")
        elif w[0] == "waitjob":  # until the Notes background job is idle (or the seconds run out)
            end = time.time() + int(w[1])
            while time.time() < end:
                r = self.job()
                if r[0] == "0":
                    break
                time.sleep(1)
            else:
                self.problems.append("the Notes job didn't finish in time")
        elif w[0] == "job":  # print the job state (step, result)
            print("  job:", self.job())
        elif w[0] == "seed":  # games: a fixed random seed, so a round plays the same every time
            self.dev.cmd(f"X SEED {w[1]}")
        elif w[0] == "manual":  # games: move on only with "frames" (1), or with time again (0)
            self.dev.cmd(f"X MANUAL {w[1]}")
        elif w[0] == "frames":  # games: run n frames at once
            self.dev.cmd(f"X FRAMES {w[1]}", timeout=120)
        elif w[0] == "sleep":
            time.sleep(int(w[1]) / 1000)
        elif w[0] in ("dry", "fake", "netfail", "nocard", "nopush"):
            self.dev.cmd(f"X {w[0].upper()} {w[1]}")
        elif w[0] == "clock" and w[1] == "unset":
            self.dev.cmd("X CLOCK UNSET")
            self.clock_touched = True
        else:
            raise ValueError(f"unknown step: {line}")

    def finish(self):
        self.dev.cmd("X SEED 0")
        self.dev.cmd("X MANUAL 0")
        for sw in ("DRY", "FAKE", "NETFAIL", "NOCARD", "NOPUSH"):
            self.dev.cmd(f"X {sw} 0")
        if self.clock_touched:
            self.dev.s.write(f"T {int(time.time())}\n".encode())
            time.sleep(0.3)
        rows = "".join(
            f'<figure><img src="{html.escape(n)}.png" width="300"><figcaption><b>{html.escape(n)}</b><br>'
            f'{html.escape(s)} · {k} {ms} ms<br>{html.escape("; ".join(notes))}</figcaption></figure>'
            for n, s, k, ms, notes in self.tiles)
        page = ("<!doctype html><meta charset=utf-8><title>unidex shots</title><style>body{font:13px system-ui;"
                "margin:16px;background:#eee}figure{display:inline-block;margin:8px;vertical-align:top}"
                "img{border:1px solid #999;image-rendering:pixelated}</style>" + rows)
        open(os.path.join(self.out, "index.html"), "w").write(page)
        if self.dev.log:
            open(os.path.join(self.out, "device.log"), "w").write("\n".join(self.dev.log) + "\n")
            self.problems.append(f"{len(self.dev.log)} unexpected lines from the device (device.log)")
        for p in self.problems:
            print("PROBLEM:", p)
        print(f"{len(self.tiles)} shots in {self.out}/index.html")
        return not self.problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("what", choices=["run", "shot", "press"])
    ap.add_argument("args", nargs="*")
    ap.add_argument("--out", default="shots/latest")
    ap.add_argument("--compare")
    ap.add_argument("--port")
    a = ap.parse_args()
    dev = Device(a.port or find_port())
    run = Run(dev, a.out, a.compare)
    try:
        if a.what == "run":
            for line in open(a.args[0]):
                run.step(line.strip())
        elif a.what == "shot":
            run.shot(a.args[0] if a.args else "shot")
        else:
            for k in a.args:
                print(k, dev.press(k))
    except Exception:
        if dev.log:
            print("device said:\n  " + "\n  ".join(dev.log[-30:]))
        raise
    finally:
        ok = run.finish() if a.what != "press" else True
    sys.exit(0 if ok else 1)


main()

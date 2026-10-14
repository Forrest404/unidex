#!/usr/bin/env python3
"""Install, list and remove unidex apps over USB, the same way the website's store page does (docs/APPS.md).

  python3 tools/appinstall.py install apps/examples/hello [--port /dev/cu.usbmodem101]
  python3 tools/appinstall.py list
  python3 tools/appinstall.py rm hello [--keep]      (--keep: offload, its data stays)
  python3 tools/appinstall.py data hello
"""
import argparse
import os
import sys
import time
import zlib

import serial
from serial.tools import list_ports


def find_port():
    for p in list_ports.comports():
        if p.vid == 0x303A:
            return p.device
    sys.exit("no unidex found (plug it in and press a button: it may be asleep)")


class Device:
    def __init__(self, port):
        self.s = serial.Serial()
        self.s.port, self.s.baudrate, self.s.timeout = port, 115200, 0.2
        self.s.rts = False  # DTR low with RTS high resets the chip
        self.s.open()
        time.sleep(0.3)
        self.s.reset_input_buffer()

    def send(self, line):
        self.s.write((line + "\n").encode())

    def expect(self, prefixes, timeout=10, collect=None):
        end = time.time() + timeout
        while time.time() < end:
            l = self.s.readline().decode("ascii", "replace").strip()
            if not l:
                continue
            if l.startswith(prefixes) or l.startswith("ERR"):
                return l
            if collect is not None:
                collect.append(l)
        raise TimeoutError(f"no reply (waiting for {prefixes})")


def files_of(folder):
    out = []
    for root, dirs, names in os.walk(folder):
        dirs[:] = [d for d in dirs if not d.startswith(".") and d != "data"]
        for n in names:
            if n.startswith(".") or (root == folder and n in ("README.md", "LICENSE")):
                continue  # (README and LICENSE are for people reading the store, not for the device)
            full = os.path.join(root, n)
            out.append((os.path.relpath(full, folder).replace(os.sep, "/"), full))
    out.sort(key=lambda f: f[0] == "app.json")  # the manifest last (the device moves it in last anyway)
    return out


def install(d, folder):
    app_id = os.path.basename(os.path.normpath(folder))
    d.send(f"A BEGIN {app_id}")
    r = d.expect(("OK A BEGIN",))
    if r.startswith("ERR"):
        sys.exit(f"begin: {r}")
    for rel, full in files_of(folder):
        data = open(full, "rb").read()
        d.send(f"A PUT {app_id} {rel} {len(data)} {zlib.crc32(data)}")
        r = d.expect(("OK A PUT",))
        if r.startswith("ERR"):
            sys.exit(f"{rel}: refused ({r})")
        for i in range(0, len(data), 64):
            d.send("D " + data[i:i + 64].hex())
            r = d.expect(("K", "OK A FILE"))
            if r.startswith("ERR"):
                sys.exit(f"{rel}: failed while sending")
        if r != "OK A FILE":
            r = d.expect(("OK A FILE",))
        if r.startswith("ERR"):
            sys.exit(f"{rel}: the device's copy didn't match")
        print(f"  sent {rel} ({len(data)} bytes)")
    d.send(f"A DONE {app_id}")
    r = d.expect(("OK A DONE",), timeout=20)
    if r.startswith("ERR"):
        why = r[4:]
        try:
            why = bytes.fromhex(why).decode()
        except ValueError:
            pass
        sys.exit(f"install refused: {why}")
    print(f"installed {app_id}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["install", "list", "rm", "data"])
    ap.add_argument("what", nargs="?")
    ap.add_argument("--port")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    d = Device(a.port or find_port())
    if a.cmd == "install":
        install(d, a.what)
    elif a.cmd == "list":
        lines = []
        d.send("A LIST")
        d.expect(("OK A end",), collect=lines)
        for l in lines:
            if l.startswith("A "):
                p = l.split()
                state = "ok" if p[5] == "ok" else bytes.fromhex(p[5]).decode()
                print(f"{p[1]:<24} {p[2]:<10} code {p[3]:>7}  data {p[4]:>7}  {state}")
    elif a.cmd == "rm":
        d.send(f"A RM {a.what}" + (" keep" if a.keep else ""))
        print(d.expect(("OK A RM",)))
    elif a.cmd == "data":
        d.send(f"A DATA {a.what}")
        print(d.expect(("OK A DATA",)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check that a unidex holds no credentials before it is sold or given away.

Asks the board for its credential status ("N ?") and prints only whether each slot is set.
Values and key hints are never printed. PASS only if every slot reads "unset".

  ~/.platformio/penv/bin/python tools/check_unit.py [port]
"""
import sys
import time

import serial
from serial.tools import list_ports

SLOTS = ["wifi_ssid", "wifi_pass", "wifi_user", "openai_key", "anthropic_key", "cleanup", "cleanup_model",
         "gh_on", "gh_repo", "gh_branch", "gh_dir", "gh_token"]


def find_port():
    for p in list_ports.comports():
        if p.vid == 0x303A:
            return p.device
    sys.exit("No unidex found on USB. Plug it in, or pass the port.")


def ask(port):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.2
    s.rts = False  # leave DTR alone: pyserial sets DTR before RTS, and DTR low with RTS high resets the chip
    s.open()
    lines, start, resent = [], time.time(), False
    s.write(b"N ?\n")
    while time.time() < start + 8:
        line = s.readline().decode("ascii", "replace").strip()
        if line.startswith(("NS ", "NC ")):
            lines.append(line)
        elif line == "OK N ?":
            break
        elif not lines and not resent and time.time() > start + 4:  # a cold boot shows the splash for ~3.5 s
            s.write(b"N ?\n")
            resent = True
    s.close()
    return lines


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    try:
        lines = ask(port)
    except serial.SerialException as e:
        sys.exit(f"Can't open {port} ({e}). Close the website tab or pause the Mac agent.")

    state = {}
    notes = None
    for line in lines:
        parts = line.split(" ")
        if parts[0] == "NS" and len(parts) >= 3:
            state[parts[1]] = parts[2]  # parts[3] (values, key hints) is dropped, never shown
        elif parts[0] == "NC" and len(parts) >= 4:
            notes = int(parts[2])

    ok = True
    for name in SLOTS:
        s = state.get(name, "missing")
        ok &= s == "unset"
        print(f"  {name:14} {s}")
    if notes:
        print(f"  warning: the SD card holds {notes} note(s); use a fresh card for a unit you sell")
    print("PASS" if ok else "FAIL: erase the unit fully, then check again")
    sys.exit(0 if ok else 1)


main()

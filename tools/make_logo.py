#!/usr/bin/env python3
"""Turns the unidex logo (the square "Secondary" PNG: the mascot above the word) into the boot screen's bitmaps.

  python3 tools/make_logo.py "path/to/unidex Logo_Secondary.png"     (writes src/core/logo.h and the unidex badge)

The logo is pixel art on two grids: the mascot's squares are 160 px in the 8334 px original and the word's are
about 113 px. Each square is sampled once at its centre and drawn as a whole number of screen pixels (4 for the
mascot, 3 for the word), so every edge stays sharp on the 200x200 screen.
"""
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
HEAD_PX, TEXT_PX = 4, 3  # screen pixels per square


def cells(black, x0, x1, y0, y1, unit):
    """The art squares of the box, as rows of 0/1 (1 = ink), sampled at each square's centre."""
    cols, rows = round((x1 - x0 + 1) / unit), round((y1 - y0 + 1) / unit)
    ux, uy = (x1 - x0 + 1) / cols, (y1 - y0 + 1) / rows
    return [[int(black[int(y0 + (r + .5) * uy), int(x0 + (c + .5) * ux)]) for c in range(cols)] for r in range(rows)]


def pack(grid, scale):
    """Rows of squares -> 1 bit a pixel, MSB first, each row padded to whole bytes (Adafruit GFX drawBitmap)."""
    h, w = len(grid) * scale, len(grid[0]) * scale
    bytes_per_row = (w + 7) // 8
    out = bytearray(bytes_per_row * h)
    for y in range(h):
        for x in range(w):
            if grid[y // scale][x // scale]:
                out[y * bytes_per_row + x // 8] |= 0x80 >> (x % 8)
    return w, h, out


def c_array(name, data):
    lines = []
    for i in range(0, len(data), 16):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    return f"static const uint8_t {name}[] PROGMEM = {{\n" + "\n".join(lines) + "\n};\n"


def write_badge(hw, hh, hb, tw, th, tb, path="starter/badges/02-unidex.bmp"):
    """The unidex badge for new SD cards: the boot screen as a 200x200 1-bit BMP (laid out as drawSplash() does)."""
    img = Image.new("1", (200, 200), 1)
    gap = 8
    top = (200 - (hh + gap + th)) // 2
    for w, h, data, y0 in ((hw, hh, hb, top), (tw, th, tb, top + hh + gap)):
        x0, bytes_per_row = (200 - w) // 2, (w + 7) // 8
        for y in range(h):
            for x in range(w):
                if data[y * bytes_per_row + x // 8] & (0x80 >> (x % 8)):
                    img.putpixel((x0 + x, y0 + y), 0)
    img.save(path)


def main():
    im = Image.open(sys.argv[1]).convert("L")
    black = np.array(im) < 128
    ys, xs = np.where(black)
    row_ink = black.sum(axis=1)
    inked = np.where(row_ink > 0)[0]
    gap = next(i for i in range(len(inked) - 1) if inked[i + 1] - inked[i] > 20)  # between the mascot and the word
    head_y1, text_y0 = inked[gap], inked[gap + 1]
    hx = np.where(black[: head_y1 + 1].any(axis=0))[0]
    tx = np.where(black[text_y0:].any(axis=0))[0]
    head = cells(black, hx.min(), hx.max(), inked[0], head_y1, 160)
    text = cells(black, tx.min(), tx.max(), text_y0, inked[-1], 113.5)
    hw, hh, hb = pack(head, HEAD_PX)
    tw, th, tb = pack(text, TEXT_PX)
    with open("src/core/logo.h", "w") as f:
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("// The unidex logo for the boot screen, made by tools/make_logo.py from the logo artwork (do not edit by hand).\n")
        f.write("// 1 bit a pixel, MSB first, rows padded to whole bytes: Adafruit GFX drawBitmap().\n")
        f.write(f"static const int16_t LOGO_HEAD_W = {hw}, LOGO_HEAD_H = {hh}, LOGO_TEXT_W = {tw}, LOGO_TEXT_H = {th};\n")
        f.write(c_array("LOGO_HEAD", hb) + c_array("LOGO_TEXT", tb))
    write_badge(hw, hh, hb, tw, th, tb)
    print(f"head {hw}x{hh} ({len(head[0])}x{len(head)} squares), word {tw}x{th} ({len(text[0])}x{len(text)} squares)")


if __name__ == "__main__":
    main()

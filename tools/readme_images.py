#!/usr/bin/env python3
"""The README's strip of screens (docs/screens.png) and the website's link preview (site/og.png), from the screenshots
tools/walkthroughs/readme.txt takes (devshot.py, 600x600 at 3x):

  ~/.platformio/penv/bin/python tools/devshot.py run tools/walkthroughs/readme.txt --out shots/readme
  python3 tools/readme_images.py shots/readme          (needs Pillow; add --og to remake the link preview too)

Plain: the e-paper's tone for the screens with a thin frame, Liberation Sans (tools/fonts) for the preview's text.
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

shots = sys.argv[1]
repo = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PAPER, INK, BG, EDGE = (240, 239, 233), (26, 26, 26), (255, 255, 255), (60, 60, 60)
STRIP = ["r-home-timetable", "r-timetable", "r-today", "r-notes-list", "r-note", "r-pet", "r-dino"]


def screen(name, size):
    im = Image.open(os.path.join(shots, name + ".png")).convert("L").resize((size, size), Image.NEAREST)
    out = Image.new("RGB", (size, size), PAPER)
    out.paste(Image.new("RGB", (size, size), INK), mask=im.point(lambda v: 255 if v < 128 else 0))
    framed = Image.new("RGB", (size + 4, size + 4), EDGE)  # a 2 px border, like the panel's edge
    framed.paste(out, (2, 2))
    return framed


tiles = [screen(n, 400) for n in STRIP]  # 2x
gap, pad = 24, 24
width = pad * 2 + sum(t.width for t in tiles) + gap * (len(tiles) - 1)
strip = Image.new("RGB", (width, pad * 2 + tiles[0].height), BG)
x = pad
for t in tiles:
    strip.paste(t, (x, pad))
    x += t.width + gap
strip.save(os.path.join(repo, "docs", "screens.png"), optimize=True)
print("docs/screens.png", strip.size)

if "--og" in sys.argv:  # the link preview: the name and one line on the left, two screens on the right
    fonts = os.path.join(repo, "tools", "fonts")
    og = Image.new("RGB", (1200, 630), BG)
    d = ImageDraw.Draw(og)
    d.text((72, 170), "unidex", font=ImageFont.truetype(os.path.join(fonts, "LiberationSans-Bold.ttf"), 96), fill=INK)
    body = ImageFont.truetype(os.path.join(fonts, "LiberationSans-Regular.ttf"), 34)
    for i, line in enumerate(["A pocket e-ink OS for the", "Waveshare ESP32-S3 board:", "timetable, voice notes,",
                              "a Pet, badges and games."]):
        d.text((76, 300 + i * 46), line, font=body, fill=(70, 70, 70))
    og.paste(screen("r-timetable", 280), (560, 175))
    og.paste(screen("r-pet", 280), (870, 175))
    og.save(os.path.join(repo, "site", "og.png"), optimize=True)
    print("site/og.png", og.size)

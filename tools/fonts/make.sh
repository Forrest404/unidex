#!/bin/sh
# The bitmap fonts in src/core/fonts, from Liberation Sans 2 (SIL Open Font License 1.1, see LICENSE here).
# Sized so lines come out as wide as with the FreeSans fonts unidex used before (within ~1%, never much wider):
# 12 and 18 point are drawn a pixel smaller than the name says.
cd "$(dirname "$0")/../.." || exit 1
f=tools/fonts
python3 tools/gfxfont.py $f/LiberationSans-Regular.ttf 7 Sans7pt7b > src/core/fonts/Sans7pt7b.h
python3 tools/gfxfont.py $f/LiberationSans-Regular.ttf 9 Sans9pt7b > src/core/fonts/Sans9pt7b.h
python3 tools/gfxfont.py $f/LiberationSans-Regular.ttf 11.75 Sans12pt7b > src/core/fonts/Sans12pt7b.h
python3 tools/gfxfont.py $f/LiberationSans-Regular.ttf 17.4 Sans18pt7b > src/core/fonts/Sans18pt7b.h
python3 tools/gfxfont.py $f/LiberationSans-Bold.ttf 9 SansBold9pt7b > src/core/fonts/SansBold9pt7b.h

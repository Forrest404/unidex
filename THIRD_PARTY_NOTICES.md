# Third-party notices

unidex is licensed under the GNU General Public License v3.0 or later (see [LICENSE](LICENSE)).
The firmware built from this repository includes, or is built with, the third-party software below.
Full licence texts not reproduced here are in [licenses/](licenses/).

This list was compiled from each project's own licence files and package metadata. It is not legal advice.

## Compiled into the firmware

| Component | Version | Licence | Copyright | Source |
|---|---|---|---|---|
| GxEPD2 (e-paper driver) | 1.6.9 | GPL-3.0 | Jean-Marc Zingg | https://github.com/ZinggJM/GxEPD2 |
| Adafruit GFX Library | 1.12.6 | BSD (see below) | Adafruit Industries | https://github.com/adafruit/Adafruit-GFX-Library |
| FreeSans fonts (in Adafruit GFX, converted from GNU FreeFont; src/core/FreeSans7pt7b.h converted from FreeFont 20120503 by tools/gfxfont.py) | – | GPL-3.0-or-later with font exception | Free Software Foundation and contributors | https://www.gnu.org/software/freefont/license.html |
| Adafruit BusIO | 1.17.4 | MIT (see below) | Adafruit Industries | https://github.com/adafruit/Adafruit_BusIO |
| ArduinoJson | 7.4.3 | MIT (see below) | Benoit Blanchon | https://github.com/bblanchon/ArduinoJson |
| Arduino core for ESP32 (arduino-esp32) | 2.0.17 | LGPL-2.1-or-later (files from the Arduino project) and Apache-2.0 (Espressif files) | Arduino team; Espressif Systems | https://github.com/espressif/arduino-esp32/tree/2.0.17 |
| ESP-IDF libraries (precompiled in arduino-esp32 2.0.17) | 4.4 | Apache-2.0, plus third-party components under BSD, MIT, ISC and similar licences | Espressif Systems and others | https://github.com/espressif/esp-idf/blob/release/v4.4/docs/en/COPYRIGHT.rst |
| ESP32 WiFi/PHY libraries | bundled | Apache-2.0 | Espressif Systems | https://github.com/espressif/esp32-wifi-lib |
| Mozilla root certificate list (certs/x509_crt_bundle.bin, 2024-09-24) | – | MPL-2.0 | Mozilla contributors | https://curl.se/docs/caextract.html |
| ES8311 register values and start-up sequences in src/core/audio.cpp | esp_codec_dev 1.3.5 | Apache-2.0 | 2023 Espressif Systems (Shanghai) CO LTD | https://github.com/espressif/esp-adf/tree/master/components/esp_codec_dev |
| Note clean-up prompt in src/apps/notes/cloud.cpp | – | MIT (see below) | 2026 Forrest | https://github.com/Forrest404/forrest-notes |

**Changes to Apache-2.0 material:** the ES8311 register values and sequences from Espressif's `es8311.c` were
used in a new, minimal standalone driver written for the Wire bus and the legacy ESP-IDF I2S API
(src/core/audio.cpp). No file was copied.

## Used by the website only (not stored on the device)

| Component | Version | Licence | Copyright | Source |
|---|---|---|---|---|
| ESP Web Tools | 10.4.0 | Apache-2.0 | ESPHome contributors | https://github.com/esphome/esp-web-tools |
| esptool-js | 0.6.0 | Apache-2.0 | 2023 Espressif Systems | https://github.com/espressif/esptool-js |
| Flasher stub sent to the chip's RAM by esptool-js while flashing (not saved on the device) | – | GPL-2.0 | Espressif Systems and contributors | https://github.com/espressif/esptool-legacy-flasher-stub |
| ical.js | 2.2.1 | MPL-2.0 | ical.js contributors | https://github.com/kewisch/ical.js |

## Build tools (not distributed)

PlatformIO (Apache-2.0), esptool.py (GPL-2.0), Pillow (used by tools/badges.py) and cryptography (used by
tools/make_cert_bundle.py) are used to build the firmware or files and are not part of it.

## Licence texts

### Adafruit GFX Library (BSD)

```
Software License Agreement (BSD License)

Copyright (c) 2012 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

### Adafruit BusIO (MIT)

```
The MIT License (MIT)

Copyright (c) 2017 Adafruit Industries

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### ArduinoJson (MIT)

```
The MIT License (MIT)
---------------------

Copyright © 2014-2026, Benoit BLANCHON

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

### forrest-notes (MIT)

```
MIT License

Copyright (c) 2026 Forrest

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

GPL-3.0: [LICENSE](LICENSE). GPL-2.0, LGPL-2.1, Apache-2.0, MPL-2.0: [licenses/](licenses/).

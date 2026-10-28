#pragma once

struct ClimateReading {
  float temperatureC;
  float humidityRH;
};

// Reads the board's SHTC3 (about 20 ms). false if it doesn't answer or a CRC is wrong. The sensor sits next to the
// chip, so it reads the board's own warmth too: a few degrees over the room, more while charging or on WiFi.
bool climateRead(ClimateReading &out);

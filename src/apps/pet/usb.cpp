#include "pet.h"
#include "pet_logic.h"
#include "../../core/storage.h"

static const size_t MAX_NAME = 12;

static int hexValue(char c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

// A name from hex: printable ASCII only (the screen's font has nothing else), trimmed, at most MAX_NAME.
static bool nameFromHex(const char *hex, String &out) {
  out = "";
  const size_t n = strlen(hex);
  if (n % 2 || n / 2 > MAX_NAME) return false;
  for (size_t i = 0; i < n; i += 2) {
    const int hi = hexValue(hex[i]), lo = hexValue(hex[i + 1]);
    if (hi < 0 || lo < 0) return false;
    const char c = (char)(hi << 4 | lo);
    if (c < 0x20 || c > 0x7E) return false;
    out += c;
  }
  out.trim();
  return true;
}

bool petSave(uint32_t look, const String &raw) {
  String name = raw;
  name.trim();
  if (name.length() > MAX_NAME) return false;
  for (const char c : name)
    if (c < 0x20 || c > 0x7E) return false;
  storagePutInt("pet_look", pet::pack(pet::unpack(look)));  // parts that don't exist fall back to the default
  if (name.length()) storagePutString("pet_name", name.c_str());
  else storageRemoveKey("pet_name");
  petChanged();
  return true;
}

String petPartsJson() {
  String out;
  out.reserve(36 * 1024);
  out = "{\"layers\":[";
  for (int layer = 0; layer < pet::LAYERS; layer++) {
    const pet::LayerParts lp = pet::partsOf(layer);
    out += String(layer ? "," : "") + "{\"name\":\"" + pet::LAYER_NAMES[layer] + "\",\"parts\":[";
    for (size_t i = 0; i < lp.count; i++) {
      out += String(i ? "," : "") + "{\"n\":\"" + lp.parts[i].name + "\",\"r\":\"";
      for (const char *row : lp.parts[i].rows) out += row;  // only '#', 'o' and '.': nothing to escape
      out += "\"}";
    }
    out += "]}";
  }
  out += "],\"look\":" + String(petLookBits()) + ",\"name\":\"";
  for (const char c : petName()) out += c == '"' || c == '\\' ? String("\\") + c : String(c);
  return out + "\"}";
}

bool petUsb(const char *line) {
  if (strncmp(line, "P ", 2) != 0) return false;
  const char *cmd = line + 2;
  if (strcmp(cmd, "GET") == 0) {
    Serial.printf("OK P %05lx ", (unsigned long)petLookBits());
    for (const char c : petName()) Serial.printf("%02x", (uint8_t)c);
    Serial.print('\n');
  } else if (strncmp(cmd, "SET ", 4) == 0) {
    char lookHex[12] = "", nameHex[2 * MAX_NAME + 2] = "";
    String name;
    char *end;
    const int n = sscanf(cmd + 4, "%11s %25s", lookHex, nameHex);
    const unsigned long look = strtoul(lookHex, &end, 16);
    if (n < 1 || *end || !nameFromHex(n > 1 ? nameHex : "", name) || !petSave(look, name)) {
      Serial.println("ERR");
      return true;
    }
    Serial.println("OK P SET");
  } else {
    Serial.println("ERR");
  }
  return true;
}

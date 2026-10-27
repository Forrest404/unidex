#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "phone_text.h"

// The setup side of the phone portal: the form a phone sends, the rules a value must pass before it's saved
// (the same as the website's Setup page), and the saved settings as JSON with keys masked. No Arduino in here,
// so tools/notetest checks it on a computer.
namespace phonesetup {

inline int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// application/x-www-form-urlencoded: "+" is a space, %XX a byte.
inline std::string urlDecode(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    const int hi = s[i] == '%' && i + 2 < s.size() ? hexValue(s[i + 1]) : -1, lo = hi < 0 ? -1 : hexValue(s[i + 2]);
    if (hi >= 0 && lo >= 0) out += (char)(hi * 16 + lo), i += 2;
    else out += s[i] == '+' ? ' ' : s[i];
  }
  return out;
}

// "a=1&b=two" as (name, value) pairs, in order.
inline std::vector<std::pair<std::string, std::string>> formPairs(const std::string &body) {
  std::vector<std::pair<std::string, std::string>> out;
  size_t at = 0;
  while (at <= body.size()) {
    size_t end = body.find('&', at);
    if (end == std::string::npos) end = body.size();
    const std::string part = body.substr(at, end - at);
    if (!part.empty()) {
      const size_t eq = part.find('=');
      out.push_back({urlDecode(part.substr(0, eq)), eq == std::string::npos ? "" : urlDecode(part.substr(eq + 1))});
    }
    at = end + 1;
  }
  return out;
}

inline bool endsWith(const std::string &s, const std::string &tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

inline bool repoLike(const std::string &s) {  // owner/name
  const size_t slash = s.find('/');
  if (slash == std::string::npos || slash == 0 || slash + 1 == s.size() || s.find('/', slash + 1) != std::string::npos)
    return false;
  for (char c : s)
    if (c != '/' && !isalnum((unsigned char)c) && c != '_' && c != '.' && c != '-') return false;
  return true;
}

inline bool linkLike(const std::string &s) {  // https://, http://, webcal:// or webcals://, no spaces
  std::string lower;
  for (char c : s) lower += (char)tolower((unsigned char)c);
  const char *schemes[] = {"https://", "http://", "webcal://", "webcals://"};
  for (const char *p : schemes)
    if (phonetext::startsWith(lower, p) && s.size() > strlen(p)) {
      for (char c : s)
        if (isspace((unsigned char)c)) return false;
      return true;
    }
  return false;
}

inline bool base64Like(const std::string &s) {  // a certificate's body: letters, digits, + and /, then = padding
  size_t i = 0;
  while (i < s.size() && (isalnum((unsigned char)s[i]) || s[i] == '+' || s[i] == '/')) i++;
  if (!i) return false;
  while (i < s.size() && s[i] == '=') i++;
  return i == s.size();
}

// nullptr if the value may be saved under this name, else why not (shown on the phone). Empty clears a setting,
// except the two choices, which always need one of their values. Lengths and unknown names are credSet's job.
inline const char *checkValue(const std::string &name, const std::string &value) {
  if (name == "transcribe")
    return value == "openai" || value == "elevenlabs" ? nullptr : "Choose OpenAI or ElevenLabs.";
  if (name == "cleanup")
    return value == "openai" || value == "anthropic" || value == "off" ? nullptr : "Choose OpenAI, Claude or off.";
  if (value.empty()) return nullptr;
  if (name == "gh_on") return value == "0" || value == "1" ? nullptr : "On or off.";
  if (name == "openai_key") return phonetext::startsWith(value, "sk-") ? nullptr : "OpenAI keys start with sk-.";
  if (name == "cleanup_model")
    return phonetext::startsWith(value, "sk-") ? "That looks like a key. This box takes a model name." : nullptr;
  if (name == "gh_repo") return repoLike(value) ? nullptr : "The repo should look like owner/name.";
  if (name == "cal_url") return linkLike(value) ? nullptr : "The link should start with https:// or webcal://.";
  if (endsWith(name, "_ca")) return base64Like(value) ? nullptr : "That isn't a certificate.";
  return nullptr;
}

// credStatus's lines ("<name> <set|unset> <hex>", the hex being the value, a key's last 4 characters, or nothing
// for a WiFi password) as one JSON object: {"name":{"set":1,"v":"..."},...}. Nothing more than they say leaves.
inline std::string statusJson(const std::string &lines) {
  std::string out = "{";
  size_t at = 0;
  while (at < lines.size()) {
    size_t end = lines.find('\n', at);
    if (end == std::string::npos) end = lines.size();
    std::string line = lines.substr(at, end - at);
    at = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t s1 = line.find(' '), s2 = s1 == std::string::npos ? s1 : line.find(' ', s1 + 1);
    if (s1 == std::string::npos || s1 == 0) continue;
    const std::string name = line.substr(0, s1), state = line.substr(s1 + 1, s2 == std::string::npos ? s2 : s2 - s1 - 1);
    const std::string hex = s2 == std::string::npos ? "" : line.substr(s2 + 1);
    std::string value;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
      const int hi = hexValue(hex[i]), lo = hexValue(hex[i + 1]);
      if (hi < 0 || lo < 0) break;
      value += (char)(hi * 16 + lo);
    }
    if (out.size() > 1) out += ",";
    out += "\"" + phonetext::json(name) + "\":{\"set\":" + (state == "set" ? "1" : "0") + ",\"v\":\"" +
           phonetext::json(value) + "\"}";
  }
  return out + "}";
}

// The device's own name, which its hotspot is called (empty: "unidex-" and 4 characters). Up to 20 printable
// characters, so the QR code that joins it still fits on the screen; not the characters a WiFi QR code would have
// to escape (\ ; , : "), so the code stays plain.
static const size_t DEVICE_NAME_LEN = 20;  // (NAME_MAX is a libc macro)
inline const char *checkName(const std::string &n) {
  if (n.empty()) return nullptr;
  if (n.size() > DEVICE_NAME_LEN) return "Up to 20 characters.";
  if (n.front() == ' ' || n.back() == ' ') return "No spaces at the start or end.";
  for (char c : n) {
    if (c < 0x20 || c > 0x7e) return "Letters, numbers, spaces and simple symbols only.";
    if (strchr("\\;,:\"", c)) return "Not \\ ; , : or \" (the QR code can't hold them).";
  }
  return nullptr;
}

// --- the Tools tab ---

// Base64, standard or URL-safe ("-" and "_", which a form sends without escaping), padding optional.
inline bool base64Decode(const std::string &in, std::vector<uint8_t> &out) {
  out.clear();
  uint32_t acc = 0;
  int bits = 0;
  for (char c : in) {
    int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52
            : c == '+' || c == '-' ? 62 : c == '/' || c == '_' ? 63 : c == '=' ? -2 : -1;
    if (v == -2) break;
    if (v < 0) return false;
    acc = acc << 6 | v, bits += 6;
    if (bits >= 8) bits -= 8, out.push_back((uint8_t)(acc >> bits));
  }
  return !out.empty();
}

static const size_t BADGE_BYTES = 62 + 28 * 200;  // a 200x200 1-bit BMP (rows padded to 4 bytes), as the badge maker makes it

// A badge as the Badge app reads it: "BM", 200x200, 1 bit per pixel, uncompressed, and the size that makes.
inline bool badgeBmpOk(const std::vector<uint8_t> &b) {
  auto u16 = [&](size_t i) { return (uint32_t)b[i] | (uint32_t)b[i + 1] << 8; };
  auto u32 = [&](size_t i) { return u16(i) | u16(i + 2) << 16; };
  return b.size() == BADGE_BYTES && b[0] == 'B' && b[1] == 'M' && u32(18) == 200 && u32(22) == 200 &&
         u16(26) == 1 && u16(28) == 1 && u32(30) == 0 && u32(10) == 62;
}

// The new badge's file: the next number after the highest one on the card, then the name made safe for a file
// ("My cat!" -> "05-my-cat.bmp"). The badges are listed in that order, so a new one comes last.
inline std::string badgeFileName(const std::vector<std::string> &existing, const std::string &name) {
  int top = 0;
  for (const std::string &f : existing) top = std::max(top, atoi(f.c_str()));
  std::string slug;
  for (char c : name) {
    const char l = (char)tolower((unsigned char)c);
    if (isalnum((unsigned char)l)) slug += l;
    else if (!slug.empty() && slug.back() != '-') slug += '-';
    if (slug.size() >= 24) break;
  }
  while (!slug.empty() && slug.back() == '-') slug.pop_back();
  char num[8];
  snprintf(num, sizeof num, "%02d", std::min(top + 1, 99));
  return std::string(num) + "-" + (slug.empty() ? "badge" : slug) + ".bmp";
}

}  // namespace phonesetup

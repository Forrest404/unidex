#pragma once
#include <cctype>
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

}  // namespace phonesetup

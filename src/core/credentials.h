#pragma once
#include <Arduino.h>

// WiFi, API keys and the GitHub target, set from the website's Notes page over USB (never compiled in).
// They live in their own NVS namespace: Settings > Reset > Everything leaves them; credClearAll() wipes them.
//
// Names (also the NVS keys and the protocol names):
//   wifi_ssid, wifi_pass                 WiFi for NTP and Notes
//   wifi_user                            username for WPA2-Enterprise (eduroam); empty = a home network
//   openai_key                           Whisper transcription (and cleanup when provider = openai)
//   anthropic_key                        cleanup when provider = anthropic
//   cleanup                              "openai", "anthropic" or "off"
//   cleanup_model                        model name; empty = the provider's default
//   gh_on                                "1" = push notes to GitHub
//   gh_repo, gh_branch, gh_dir, gh_token owner/name, branch, folder in the repo, fine-grained token
bool credKnown(const char *name);
bool credSecret(const char *name);              // passwords and keys: never sent back over USB
String credGet(const char *name);               // "" if not set (gh_branch/gh_dir/cleanup have defaults)
bool credSet(const char *name, const String &value);  // false for an unknown name or a value too long
void credClear(const char *name);
void credClearAll();
bool credHas(const char *name);

// One status line per name: "<name> <set|unset> <hex>", where <hex> is the value for plain settings, the last
// 4 characters of API keys and tokens (so the page can show "…a1b2" without the key leaving the device), and
// nothing for the WiFi password.
void credStatus(Print &out);

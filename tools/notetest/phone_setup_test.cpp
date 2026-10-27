// Checks the setup side of the phone portal on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/notes tools/notetest/phone_setup_test.cpp -o /tmp/phone_setup_test && /tmp/phone_setup_test
#include <cstdio>
#include <fstream>
#include <iterator>
#include "phone_setup.h"

using namespace phonesetup;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

int main() {
  check(urlDecode("a+b%20c%2Fd%3d") == "a b c/d=", "form decoding: + and %XX");
  check(urlDecode("100%") == "100%" && urlDecode("%zz") == "%zz", "a stray % is kept as it is");
  const auto p = formPairs("wifi_ssid=Home+Net&wifi_pass=p%26w%3Dx&empty=&flag");
  check(p.size() == 4 && p[0].first == "wifi_ssid" && p[0].second == "Home Net" && p[1].second == "p&w=x" &&
            p[2].first == "empty" && p[2].second.empty() && p[3].first == "flag" && p[3].second.empty(),
        "form pairs, with & and = inside a value");
  check(formPairs("").empty() && formPairs("&&").empty(), "an empty form has no pairs");

  check(!checkValue("transcribe", "openai") && !checkValue("transcribe", "elevenlabs") &&
            checkValue("transcribe", "") && checkValue("transcribe", "whisper"),
        "transcribe: openai or elevenlabs only");
  check(!checkValue("cleanup", "off") && !checkValue("cleanup", "anthropic") && checkValue("cleanup", "claude"),
        "cleanup: openai, anthropic or off");
  check(!checkValue("openai_key", "sk-abc") && checkValue("openai_key", "abc") && !checkValue("openai_key", ""),
        "an OpenAI key starts with sk- (empty clears it)");
  check(!checkValue("note_lang", "") && !checkValue("note_lang", "fr") && !checkValue("note_lang", "zh") &&
          checkValue("note_lang", "xx") && checkValue("note_lang", "FR") && checkValue("note_lang", "french"),
        "a note language: Auto (empty) or a code from the list");
  check(checkValue("cleanup_model", "sk-abc") && !checkValue("cleanup_model", "gpt-4o-mini"),
        "a key in the model box is refused");
  check(!checkValue("gh_repo", "Forrest404/Notes") && !checkValue("gh_repo", "a.b/c_d-e") &&
            checkValue("gh_repo", "Notes") && checkValue("gh_repo", "a/b/c") && checkValue("gh_repo", "/b") &&
            checkValue("gh_repo", "a b/c"),
        "the repo looks like owner/name");
  check(!checkValue("gh_on", "1") && !checkValue("gh_on", "0") && checkValue("gh_on", "yes"), "gh_on is 0 or 1");
  check(!checkValue("cal_url", "webcal://p01.icloud.com/x") && !checkValue("cal_url", "HTTPS://a.b/c.ics") &&
            checkValue("cal_url", "ftp://a") && checkValue("cal_url", "https://") &&
            checkValue("cal_url", "https://a b"),
        "the calendar link: https, http, webcal or webcals, no spaces");
  check(!checkValue("wifi3_ca", "MIIBab+/cd==") && checkValue("wifi_ca", "-----BEGIN") &&
            checkValue("wifi_ca", "ab=cd") && !checkValue("wifi_ca", ""),
        "a certificate body is base64");
  check(!checkValue("wifi2_ssid", "Home Net") && !checkValue("gh_token", "github_pat_x"), "other values pass");

  // As credStatus prints them: a key's last 4 characters, a WiFi password as nothing, plain values whole.
  const std::string js = statusJson(
      "wifi_ssid set 486f6d65\nwifi_pass set \nopenai_key set 61316232\ngh_repo unset \r\ncleanup set 6f6666\n"
      "cleanup_model set 223c2f\n");
  check(js == "{\"wifi_ssid\":{\"set\":1,\"v\":\"Home\"},\"wifi_pass\":{\"set\":1,\"v\":\"\"},"
              "\"openai_key\":{\"set\":1,\"v\":\"a1b2\"},\"gh_repo\":{\"set\":0,\"v\":\"\"},"
              "\"cleanup\":{\"set\":1,\"v\":\"off\"},\"cleanup_model\":{\"set\":1,\"v\":\"\\\"\\u003c/\"}}",
        "status as JSON: only what the status lines hold, escaped");
  check(statusJson("") == "{}" && statusJson("junk\n") == "{}", "nothing to report gives {}");

  check(!checkName("") && !checkName("Forrest's unidex") && !checkName("Room 4 (B)") && !checkName(std::string(20, 'a')),
        "a name: empty (the default) or up to 20 printable characters");
  check(checkName(std::string(21, 'a')) && checkName(" lead") && checkName("trail ") && checkName("caf\xc3\xa9") &&
            checkName("a;b") && checkName("a:b") && checkName("a,b") && checkName("a\\b") && checkName("a\"b") &&
            checkName("tab\there"),
        "a name: too long, spaces at the ends, non-ASCII, or what a WiFi QR code would escape");

  std::vector<uint8_t> b;
  check(base64Decode("aGk=", b) && b == std::vector<uint8_t>{'h', 'i'} && base64Decode("-_8", b) && b.size() == 2 &&
            b[0] == 0xfb && b[1] == 0xff && !base64Decode("a*b", b),
        "base64, standard and URL-safe; anything else is refused");
  std::vector<uint8_t> bmp(BADGE_BYTES, 0);
  auto put = [&](size_t at, uint32_t v, int n) { for (int i = 0; i < n; i++) bmp[at + i] = v >> (8 * i); };
  bmp[0] = 'B', bmp[1] = 'M', put(10, 62, 4), put(18, 200, 4), put(22, 200, 4), put(26, 1, 2), put(28, 1, 2);
  check(badgeBmpOk(bmp), "a 200x200 1-bit BMP is a badge");
  std::ifstream starter("starter/badges/01-hello.bmp", std::ios::binary);  // (run from the repo folder)
  const std::vector<uint8_t> real((std::istreambuf_iterator<char>(starter)), std::istreambuf_iterator<char>());
  check(badgeBmpOk(real), "a starter badge passes (5662 bytes: rows padded to 28 bytes)");
  auto wrong = bmp;
  wrong[28] = 8;  // 8 bits per pixel
  check(!badgeBmpOk(std::vector<uint8_t>(bmp.begin(), bmp.end() - 1)) && !badgeBmpOk(wrong), "anything else isn't");
  check(badgeFileName({"01-ada.bmp", "04-cat.bmp", "notes.txt"}, "My cat!") == "05-my-cat.bmp" &&
            badgeFileName({}, "  ") == "01-badge.bmp" && badgeFileName({"99-x.bmp"}, "a") == "99-a.bmp" &&
            badgeFileName({}, "Ünï code/..\\x") == "01-n-code-x.bmp",
        "badge file names: the next number, a safe name");

  printf(failures ? "\n%d failed\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}

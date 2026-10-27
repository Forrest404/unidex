// Checks the setup side of the phone portal on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/notes tools/notetest/phone_setup_test.cpp -o /tmp/phone_setup_test && /tmp/phone_setup_test
#include <cstdio>
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

  printf(failures ? "\n%d failed\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}

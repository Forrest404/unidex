// Checks the text side of Notes "Open on phone" on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/notes tools/notetest/phone_text_test.cpp -o /tmp/phone_text_test
//   /tmp/phone_text_test [note.md ...]   (with .md files: shows how each one splits up)
#include <cstdio>
#include <fstream>
#include <sstream>
#include "phone_text.h"

using namespace phonetext;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

int main(int argc, char **argv) {
  check(escape("<b>Tom & \"Jo's\"</b>") == "&lt;b&gt;Tom &amp; &quot;Jo&#39;s&quot;&lt;/b&gt;", "HTML is escaped");
  check(json("say \"hi\"\\\n</script>\t\x01") == "say \\\"hi\\\"\\\\\\n\\u003c/script>\\t\\u0001",
        "JSON escaping, including < (the list sits inside the page's script)");
  check(firstLine("one\ntwo  three", 100) == "one two three" && firstLine("aaaa bbbb cccc", 9) == "aaaa bbbb\u2026" && firstLine("aaaa bbbb cccc", 8) == "aaaa\u2026",
        "first line: one line, cut at a space");
  check(firstLine("\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9", 3) == "\xc3\xa9\u2026", "never cut inside a UTF-8 character");
  check(whenOf("20261005-130440") == "5 Oct 2026, 13:04", "the date comes from the note's id");
  check(whenOf("note-7") == "note-7" && whenOf("20261305-130440") == "20261305-130440", "odd ids stay as they are");

  const std::string full =
      "---\ntitle: \"Plan for \\\"Friday\\\"\"\naliases: [\"x\"]\ndate: 2026-10-01T13:04:40\nuid: 1\n---\n\n"
      "> [!summary] Meet Sam on Friday.\n\n## Plan\n- **Book** a room\n- Bring <notes>\n\n"
      "> [!quote]- Original transcript\n> um so friday\n> with Sam\n\n---\nTopics: [[Sam]] · [[Friday]]\n";
  const Note n = parse(full);
  check(n.title == "Plan for \"Friday\"", "title from the front matter, quotes unescaped");
  check(n.summary == "Meet Sam on Friday.", "summary");
  check(n.body == "## Plan\n- **Book** a room\n- Bring <notes>", "body without summary, transcript or topics");
  check(n.transcript == "um so friday\nwith Sam", "original transcript, without the > marks");
  check(n.topics == "Sam · Friday", "topics without the link brackets");
  check(lineHtml("- **Book** a room") == "&bull; Book a room" && lineHtml("## Plan") == "<b>Plan</b>" &&
            lineHtml("Bring <notes>") == "Bring &lt;notes&gt;", "lines: bullets, headings, escaping");
  check(plainText(n, "1 Oct") == "Plan for \"Friday\"\n1 Oct\n\nMeet Sam on Friday.\n\n## Plan\n- Book a room\n- Bring <notes>",
        "Copy text: title, date, summary, body");

  const Note bare = parse("---\ntitle: \"Hi\"\n---\n\nJust text.\n");
  check(bare.title == "Hi" && bare.summary.empty() && bare.body == "Just text." && bare.transcript.empty(),
        "a note without summary or transcript");
  check(parse("").body.empty() && parse("---\nbroken").title.empty(), "empty or broken files don't crash");

  for (int i = 1; i < argc; i++) {  // real notes: show the split
    std::ifstream f(argv[i]);
    std::stringstream ss;
    ss << f.rdbuf();
    const Note r = parse(ss.str());
    printf("\n== %s\n title: %s\n summary: %s\n body: %s\n transcript: %.60s%s\n topics: %s\n", argv[i],
           r.title.c_str(), r.summary.c_str(), r.body.c_str(), r.transcript.c_str(),
           r.transcript.size() > 60 ? "..." : "", r.topics.c_str());
    if (r.title.empty() || r.body.empty()) printf(" FAIL: missing title or body\n"), failures++;
  }
  printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
  return failures ? 1 : 0;
}

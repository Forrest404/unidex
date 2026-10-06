#pragma once
#include <cstdio>
#include <string>

// The text side of "Open on phone": a note's .md split into what the page shows, made safe for HTML, and the
// plain text Copy puts on the clipboard. No Arduino in here, so tools/notetest checks it on a computer.
namespace phonetext {

struct Note {
  std::string title, summary, body, transcript, topics;
};

inline void replaceAll(std::string &s, const std::string &from, const std::string &to) {
  for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
}

inline std::string trim(const std::string &s) {
  const size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

inline bool startsWith(const std::string &s, const std::string &p) { return s.compare(0, p.size(), p) == 0; }

// Text inside HTML: the five characters that would read as markup.
inline std::string escape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 16);
  for (char c : s) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else if (c == '\'') out += "&#39;";
    else out += c;
  }
  return out;
}

// Text inside a JSON string: quotes, backslashes and control characters escaped (UTF-8 passes through), and
// '<' too, so the list can sit inside the page's <script> without a note ever closing it.
inline std::string json(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\t') out += "\\t";
    else if (c == '<') out += "\\u003c";
    else if (c < 0x20) {
      char hex[8];
      snprintf(hex, sizeof hex, "\\u%04x", c);
      out += hex;
    } else
      out += (char)c;
  }
  return out;
}

// The first `max` bytes of a note's text on one line (for the list's second line and search), cut at a
// space and never inside a UTF-8 character.
inline std::string firstLine(const std::string &text, size_t max) {
  std::string s;
  for (char c : text) s += (c == '\n' || c == '\r') ? ' ' : c;
  replaceAll(s, "**", "");
  replaceAll(s, "  ", " ");
  s = trim(s);
  if (s.size() <= max) return s;
  size_t cut = s.rfind(' ', max);
  if (cut == std::string::npos || cut < max / 2) cut = max;
  while (cut > 0 && ((unsigned char)s[cut] & 0xC0) == 0x80) cut--;
  return trim(s.substr(0, cut)) + "\u2026";
}

// When a note was recorded, from its id (YYYYMMDD-HHMMSS): "5 Oct 2026, 13:04". Anything else as it is.
inline std::string whenOf(const std::string &id) {
  static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  if (id.size() < 15 || id[8] != '-') return id;
  for (int i : {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12})
    if (id[i] < '0' || id[i] > '9') return id;
  const int month = std::stoi(id.substr(4, 2)), day = std::stoi(id.substr(6, 2));
  if (month < 1 || month > 12) return id;
  return std::to_string(day) + " " + MONTHS[month - 1] + " " + id.substr(0, 4) + ", " + id.substr(9, 2) + ":" +
         id.substr(11, 2);
}

// The layout noteMarkdown (cloud.cpp) writes: front matter with the title, "> [!summary] ...", the text,
// "> [!quote]- Original transcript" with "> " lines, then "---\nTopics: [[a]] · [[b]]".
inline Note parse(const std::string &md) {
  Note n;
  std::string rest = md;
  replaceAll(rest, "\r", "");
  if (startsWith(rest, "---")) {
    const size_t end = rest.find("\n---", 3);
    const std::string front = end == std::string::npos ? "" : rest.substr(0, end);
    const size_t t = front.find("\ntitle: \"");
    if (t != std::string::npos) {
      const size_t from = t + 9, to = front.find('\n', from);
      n.title = front.substr(from, to == std::string::npos ? std::string::npos : to - from);
      if (!n.title.empty() && n.title.back() == '"') n.title.pop_back();
      replaceAll(n.title, "\\\"", "\"");
      replaceAll(n.title, "\\\\", "\\");
    }
    const size_t next = end == std::string::npos ? std::string::npos : rest.find('\n', end + 1);
    rest = next == std::string::npos ? "" : rest.substr(next + 1);
  }
  const size_t topics = rest.find("\n---\nTopics: ");
  if (topics != std::string::npos) {
    n.topics = rest.substr(topics + 13);
    replaceAll(n.topics, "[[", "");
    replaceAll(n.topics, "]]", "");
    n.topics = trim(n.topics);
    rest = rest.substr(0, topics);
  }
  const size_t quote = rest.find("> [!quote]");
  if (quote != std::string::npos) {
    const size_t line = rest.find('\n', quote);
    std::string q = line == std::string::npos ? "" : "\n" + rest.substr(line + 1);
    replaceAll(q, "\n> ", "\n");
    replaceAll(q, "\n>\n", "\n\n");
    n.transcript = trim(q);
    rest = rest.substr(0, quote);
  }
  rest = trim(rest);
  if (startsWith(rest, "> [!summary] ")) {
    const size_t end = rest.find('\n');
    n.summary = trim(rest.substr(13, end == std::string::npos ? std::string::npos : end - 13));
    rest = end == std::string::npos ? "" : rest.substr(end + 1);
  }
  n.body = trim(rest);
  return n;
}

// One line of a note's text as HTML: Markdown bullets as "•", headings in bold, ** dropped.
inline std::string lineHtml(std::string line) {
  line = trim(line);
  replaceAll(line, "**", "");
  if (startsWith(line, "- ") || startsWith(line, "* ")) return "&bull; " + escape(line.substr(2));
  if (startsWith(line, "#")) {
    line.erase(0, line.find_first_not_of('#'));
    return "<b>" + escape(trim(line)) + "</b>";
  }
  return escape(line);
}

// What Copy puts on the clipboard: the note as plain text, ready to paste into Apple Notes.
inline std::string plainText(const Note &n, const std::string &when) {
  std::string body = n.body;
  replaceAll(body, "**", "");
  std::string out = n.title + "\n" + when + "\n\n";
  if (!n.summary.empty()) out += n.summary + "\n\n";
  return out + body;
}

}  // namespace phonetext

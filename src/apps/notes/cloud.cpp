#include "cloud.h"
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include "../../core/credentials.h"

static const char *OPENAI = "api.openai.com", *ANTHROPIC = "api.anthropic.com", *GITHUB = "api.github.com";
static const char *DEFAULT_OPENAI_MODEL = "gpt-4o-mini", *DEFAULT_ANTHROPIC_MODEL = "claude-haiku-4-5";
static const size_t MAX_TRANSCRIPT = 6000;  // characters sent to the cleanup model

// The cleanup instructions, from forrest-notes (the AI cleanup it added to its voice notes).
static const char *CLEANUP_PROMPT =
  "You clean up and label a raw voice-note transcript. Reply with ONLY a JSON object, "
  "no prose, no code fences. Schema: {"
  "\"title\": string (EXACTLY ONE word: the single main topic of the note, a noun in Title Case, no spaces), "
  "\"summary\": string (1 sentence overview), "
  "\"cleaned\": string (the note rewritten as clear, coherent, succinct markdown in the "
  "speaker's own voice, first person if the original is. Remove filler words, false "
  "starts, stutters and repetition; fix grammar and punctuation; keep ALL substantive "
  "details, names, numbers, dates and intent; do NOT add anything that was not said. Use "
  "short paragraphs, and markdown bullet points only when the note is naturally a list. "
  "No headings.), "
  "\"topics\": array of 0-6 strings (proper-noun topics or people mentioned, Title Case, "
  "no # or brackets), "
  "\"event\": object or null. Set it ONLY if the note describes a specific plan, "
  "appointment or thing to attend with a date/time (e.g. 'going to Soho House tomorrow', "
  "'dentist next Friday at 3'). Shape: {\"title\": short string, "
  "\"start\": local datetime 'YYYY-MM-DDTHH:MM', \"end\": same format or empty, "
  "\"allDay\": boolean}. Resolve relative dates ('today','tomorrow','next Friday') against "
  "the current local date/time given below. If only a day is mentioned with no clock time, "
  "set allDay true and use 00:00. If a start time is given but no end, leave end empty. "
  "If there is no dated plan, set event to null}. If the transcript is empty or "
  "unintelligible return {\"title\":\"\",\"summary\":\"\",\"cleaned\":\"\",\"topics\":[],\"event\":null}.";

// A short reason from an HTTP status, for the screen.
static const char *failure(int status, const char *service, const String &body) {
  static char reason[40];
  if (status < 0) snprintf(reason, sizeof reason, "%s: no connection", service);
  else if (status == 401 || status == 403) snprintf(reason, sizeof reason, "%s: bad key", service);
  else if (status == 429 && body.indexOf("quota") >= 0) snprintf(reason, sizeof reason, "%s: no credit", service);
  else if (status == 429) snprintf(reason, sizeof reason, "%s: busy, try later", service);
  else snprintf(reason, sizeof reason, "%s: error %d", service, status);
#if DEBUG
  Serial.printf("%s %d: %.300s\n", service, status, body.c_str());
#endif
  return reason;
}

const char *cloudTranscribe(const NetPart *audio, int nParts, String &text) {
  const String key = credGet("openai_key");
  if (!key.length()) return "no OpenAI key";
  static const char *BOUNDARY = "unidexnote7MA4YWxkTrZu0gW";
  const String pre = String("--") + BOUNDARY +
                     "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n--" + BOUNDARY +
                     "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n--" + BOUNDARY +
                     "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"note.wav\"\r\n"
                     "Content-Type: audio/wav\r\n\r\n";
  const String post = String("\r\n--") + BOUNDARY + "--\r\n";
  std::vector<NetPart> parts;
  parts.push_back(NetPart::text(pre));
  for (int i = 0; i < nParts; i++) parts.push_back(audio[i]);
  parts.push_back(NetPart::text(post));
  const String headers = "Authorization: Bearer " + key + "\r\nContent-Type: multipart/form-data; boundary=" +
                         BOUNDARY + "\r\n";
  String body;
  const int status = netHttps(OPENAI, "POST", "/v1/audio/transcriptions", headers, parts.data(), parts.size(), body);
  if (status != 200) return failure(status, "Whisper", body);
  JsonDocument doc;
  if (deserializeJson(doc, body)) return "Whisper: bad reply";
  text = doc["text"] | "";
  text.trim();
  return nullptr;
}

static String firstWords(const String &s, int words) {
  String out;
  int n = 0;
  for (size_t i = 0; i < s.length() && n < words; i++) {
    const char c = s[i];
    if (isalnum((unsigned char)c) || c == '\'' || c == '-') out += c;
    else if (out.length() && !out.endsWith(" ") && ++n < words) out += ' ';
  }
  out.trim();
  return out;
}

// Reads the model's JSON (tolerating text around it) into the note.
static bool readCleanup(const String &reply, NoteText &note) {
  const int from = reply.indexOf('{'), to = reply.lastIndexOf('}');
  if (from < 0 || to <= from) return false;
  JsonDocument doc;
  if (deserializeJson(doc, reply.substring(from, to + 1))) return false;
  note.title = doc["title"] | "";
  note.summary = doc["summary"] | "";
  note.body = doc["cleaned"] | "";
  note.topics.clear();
  for (JsonVariant t : doc["topics"].as<JsonArray>()) {
    String s = t.as<String>();
    s.trim();
    if (s.length()) note.topics.push_back(s);
  }
  JsonObject ev = doc["event"].as<JsonObject>();
  if (!ev.isNull()) {
    note.eventTitle = ev["title"] | "";
    note.eventStart = ev["start"] | "";
    note.eventEnd = ev["end"] | "";
    note.eventAllDay = ev["allDay"] | false;
  }
  return true;
}

const char *cloudCleanup(NoteText &note, const String &nowLocal) {
  const String provider = credGet("cleanup");
  note.title = firstWords(note.transcript, 4);  // the fallback, kept if cleanup is off or fails
  if (provider == "off" || !note.transcript.length()) return nullptr;
  const bool claude = provider == "anthropic";
  const String key = credGet(claude ? "anthropic_key" : "openai_key");
  if (!key.length()) return claude ? "no Anthropic key" : "no OpenAI key";
  String model = credGet("cleanup_model");
  if (!model.length()) model = claude ? DEFAULT_ANTHROPIC_MODEL : DEFAULT_OPENAI_MODEL;

  const String user = "Current local date/time: " + nowLocal + "\n\nTranscript:\n" +
                      note.transcript.substring(0, MAX_TRANSCRIPT);
  JsonDocument req;
  req["model"] = model;
  req["temperature"] = 0;
  String headers;
  if (claude) {
    req["max_tokens"] = 4096;
    req["system"] = CLEANUP_PROMPT;
    JsonObject m = req["messages"].add<JsonObject>();
    m["role"] = "user";
    m["content"] = user;
    headers = "x-api-key: " + key + "\r\nanthropic-version: 2023-06-01\r\nContent-Type: application/json\r\n";
  } else {
    req["response_format"]["type"] = "json_object";
    JsonObject s = req["messages"].add<JsonObject>();
    s["role"] = "system";
    s["content"] = CLEANUP_PROMPT;
    JsonObject m = req["messages"].add<JsonObject>();
    m["role"] = "user";
    m["content"] = user;
    headers = "Authorization: Bearer " + key + "\r\nContent-Type: application/json\r\n";
  }
  String payload, body;
  serializeJson(req, payload);
  const NetPart part = NetPart::text(payload);
  const char *service = claude ? "Claude" : "OpenAI";
  const int status = netHttps(claude ? ANTHROPIC : OPENAI, "POST", claude ? "/v1/messages" : "/v1/chat/completions",
                              headers, &part, 1, body);
  if (status != 200) return failure(status, service, body);

  JsonDocument doc;
  if (deserializeJson(doc, body)) return claude ? "Claude: bad reply" : "OpenAI: bad reply";
  const String reply = claude ? (doc["content"][0]["text"] | "") : (doc["choices"][0]["message"]["content"] | "");
  const String fallback = note.title;
  if (!readCleanup(reply, note)) {
    note.title = fallback;
    return claude ? "Claude: bad reply" : "OpenAI: bad reply";
  }
  if (!note.title.length()) note.title = fallback;
  return nullptr;
}

static String yamlText(const String &s) {  // inside double quotes
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"' || c == '\\') out += '\\';
    if (c != '\n' && c != '\r') out += c;
  }
  return out;
}

static String linkSafe(const String &s) {  // characters Obsidian links and tags can't hold
  String out;
  for (size_t i = 0; i < s.length(); i++)
    if (!strchr("[]#^|\\\"\n\r", s[i])) out += s[i];
  out.trim();
  return out;
}

String noteMarkdown(const NoteText &note, const String &id, const String &createdUtc) {
  String md = "---\ntitle: \"" + yamlText(note.title) + "\"\n";
  if (note.title.length()) md += "aliases: [\"" + yamlText(note.title) + "\"]\n";
  if (createdUtc.length()) md += "date: " + createdUtc + "\n";
  md += "uid: " + id + "\nsource: unidex\ntags: [\"Note\"";
  for (const String &t : note.topics) {
    String tag = linkSafe(t);
    tag.replace(" ", "-");
    if (tag.length()) md += ", \"" + yamlText(tag) + "\"";
  }
  md += "]\n";
  if (note.eventTitle.length() && note.eventStart.length()) {
    md += "event_title: \"" + yamlText(note.eventTitle) + "\"\nevent_start: " + note.eventStart + "\n";
    if (note.eventEnd.length()) md += "event_end: " + note.eventEnd + "\n";
    md += String("event_allday: ") + (note.eventAllDay ? "true" : "false") + "\n";
  }
  md += "---\n\n";
  if (note.summary.length()) md += "> [!summary] " + note.summary + "\n\n";

  String body = note.body.length() ? note.body : note.transcript;
  if (body.startsWith("---")) body = "\n" + body;  // would read as front matter
  md += body;
  if (!body.endsWith("\n")) md += "\n";
  if (note.body.length() && note.transcript.length()) {  // keep what was actually said
    String raw = note.transcript;
    raw.replace("\r", "");
    raw.replace("\n", "\n> ");
    md += "\n> [!quote]- Original transcript\n> " + raw + "\n";
  }
  String links;
  for (const String &t : note.topics) {
    const String l = linkSafe(t);
    if (l.length()) links += (links.length() ? " · " : "") + String("[[") + l + "]]";
  }
  if (links.length()) md += "\n---\nTopics: " + links + "\n";
  return md;
}

static String urlPath(const String &s) {  // percent-encodes a repo path, keeping the slashes
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    const uint8_t c = s[i];
    if (isalnum(c) || strchr("/-_.~", c)) out += (char)c;
    else {
      char hex[4];
      snprintf(hex, sizeof hex, "%%%02X", c);
      out += hex;
    }
  }
  return out;
}

static String githubHeaders() {
  return "Authorization: Bearer " + credGet("gh_token") +
         "\r\nAccept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\nUser-Agent: unidex\r\n";
}

const char *cloudPush(const String &name, const String &markdown, String &path) {
  const String repo = credGet("gh_repo"), token = credGet("gh_token");
  if (!repo.length() || !token.length()) return "GitHub not set up";
  String dir = credGet("gh_dir");
  while (dir.endsWith("/")) dir.remove(dir.length() - 1);
  const String base = "/repos/" + repo + "/contents/" + (dir.length() ? urlPath(dir) + "/" : "");

  // A free file name: the title, then "Title 2" ... "Title 9" (never overwrite another note).
  String body, file;
  for (int n = 1; n <= 9 && !file.length(); n++) {
    const String candidate = n == 1 ? name : name + " " + n;
    const int status = netHttps(GITHUB, "GET", (base + urlPath(candidate) + ".md").c_str(), githubHeaders(), nullptr, 0,
                                body, 2048);
    if (status == 404) file = candidate;
    else if (status != 200) return failure(status, "GitHub", body);
  }
  if (!file.length()) return "GitHub: name taken";

  size_t len = 0;
  mbedtls_base64_encode(nullptr, 0, &len, (const uint8_t *)markdown.c_str(), markdown.length());
  std::vector<uint8_t> b64(len + 1);
  mbedtls_base64_encode(b64.data(), b64.size(), &len, (const uint8_t *)markdown.c_str(), markdown.length());
  b64[len] = 0;
  JsonDocument req;
  req["message"] = "Voice note: " + file;
  req["content"] = (const char *)b64.data();
  req["branch"] = credGet("gh_branch");
  String payload;
  serializeJson(req, payload);
  const NetPart part = NetPart::text(payload);
  const int status = netHttps(GITHUB, "PUT", (base + urlPath(file) + ".md").c_str(),
                              githubHeaders() + "Content-Type: application/json\r\n", &part, 1, body, 4096);
  if (status == 404) return "GitHub: repo not found";
  if (status != 200 && status != 201) return failure(status, "GitHub", body);
  path = (dir.length() ? dir + "/" : "") + file + ".md";
  return nullptr;
}

const char *cloudTest(const char *what) {
  if (const char *err = netConnect()) return err;
  String body;
  const char *err = nullptr;
  int status = 0;
  if (strcmp(what, "wifi") == 0) {
    // connected is enough
  } else if (strcmp(what, "openai") == 0) {
    const String key = credGet("openai_key");
    if (!key.length()) err = "no OpenAI key";
    else if ((status = netHttps(OPENAI, "GET", "/v1/models", "Authorization: Bearer " + key + "\r\n", nullptr, 0,
                                body, 1024)) != 200)
      err = failure(status, "OpenAI", body);
  } else if (strcmp(what, "anthropic") == 0) {
    const String key = credGet("anthropic_key");
    if (!key.length()) err = "no Anthropic key";
    else if ((status = netHttps(ANTHROPIC, "GET", "/v1/models",
                                "x-api-key: " + key + "\r\nanthropic-version: 2023-06-01\r\n", nullptr, 0, body,
                                1024)) != 200)
      err = failure(status, "Claude", body);
  } else if (strcmp(what, "github") == 0) {
    const String repo = credGet("gh_repo");
    if (!repo.length() || !credHas("gh_token")) err = "GitHub not set up";
    else if ((status = netHttps(GITHUB, "GET", ("/repos/" + repo).c_str(), githubHeaders(), nullptr, 0, body,
                                1024)) == 404)
      err = "GitHub: repo not found";
    else if (status != 200)
      err = failure(status, "GitHub", body);
  } else {
    err = "unknown test";
  }
  netOff();
  return err;
}

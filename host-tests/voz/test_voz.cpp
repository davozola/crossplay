// VOZ's freestanding half, checked on a laptop: the WAV header, base64 in
// pieces, the JSON read back from both services, and the note on the card.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "VozCore.h"

using namespace voz;

static int failures = 0;
static int checks = 0;
static void check(const bool ok, const char* what, const std::string& detail = "") {
  ++checks;
  if (ok) return;
  ++failures;
  std::printf("FAIL %s %s\n", what, detail.c_str());
}

int main() {
  // WAV
  uint8_t h[kWavHeaderBytes];
  wavHeader(h, 32000);
  check(!std::memcmp(h, "RIFF", 4) && !std::memcmp(h + 8, "WAVEfmt ", 8) && !std::memcmp(h + 36, "data", 4), "tags");
  check(get32(h + 4) == 36 + 32000 && get32(h + 40) == 32000 && get32(h + 24) == 16000 && get32(h + 28) == 32000,
        "sizes and rates");
  check(h[22] == 1 && h[34] == 16 && h[32] == 2, "mono, 16 bits, two-byte frames");
  check(secondsIn(44 + 32000) == 1 && secondsIn(44 + 32000 * 42) == 42 && secondsIn(44) == 0 && secondsIn(10) == 0,
        "seconds");
  check(kMaxSamples == 960000, "a minute at 16 kHz");

  // normalise
  int16_t quiet[4] = {100, -200, 50, 0};
  check(normalise(quiet, 4) == 8 && quiet[1] == -1600, "a quiet take is raised, at most eight times");
  int16_t loud[3] = {20000, -30000, 5};
  check(normalise(loud, 3) == 1 && loud[1] == -30000, "a loud take is left");
  int16_t mid[2] = {4000, -4000};
  check(normalise(mid, 2) == 4 && mid[0] == 16000, "to half of full scale");
  int16_t silent[2] = {0, 0};
  check(normalise(silent, 2) == 1, "silence");

  // levels
  int16_t stuck[4] = {-32768, -32768, -32768, -32768};
  check(levelOf(stuck, 4).peak == 0 && levelOf(stuck, 4).rms == 0, "a stuck line is silence");
  int16_t tone[4] = {1100, 900, 1100, 900};
  check(levelOf(tone, 4).peak == 100 && levelOf(tone, 4).rms == 100, "level about the mean");
  check(levelOf(tone, 0).peak == 0, "no samples, no level");

  check(levelBar(0) == 0 && levelBar(kHeardPeak / 2 - 1) == 0, "an empty bar for silence");
  check(levelBar(kHeardPeak) == 20 && levelBar(kHeardPeak * 4) == 40, "each doubling is a step");
  check(levelBar(32767) == 100, "a full bar at full scale");

  // microphone setups
  for (int i = 0; i < kMicSetups; ++i) check(micIndex(micSetup(i)) == i, "setup round trip");
  check(micSetup(0).enableHigh && !micSetup(0).rightSlot && !micSetup(0).fastClock, "setup 0 is the board's notes");
  char setupName[24];
  micSetupName(micSetup(7), setupName, sizeof(setupName));
  check(!std::strcmp(setupName, "BAJA DER 2 MHZ"), "setup name", setupName);
  Level heard[kMicSetups];
  bool opened[kMicSetups];
  for (int i = 0; i < kMicSetups; ++i) opened[i] = true;
  check(bestSetup(heard, opened, kMicSetups) == -1, "nothing heard, no setup");
  heard[2].peak = 500;
  heard[2].rms = 90;
  heard[5].peak = 300;
  heard[5].rms = 120;
  heard[6].peak = kHeardPeak - 1;
  heard[6].rms = 400;
  check(bestSetup(heard, opened, kMicSetups) == 5, "the loudest setup wins");
  opened[5] = false;
  check(bestSetup(heard, opened, kMicSetups) == 2, "one that did not open cannot win");

  // base64
  const char* samples[][2] = {{"", ""},
                              {"f", "Zg=="},
                              {"fo", "Zm8="},
                              {"foo", "Zm9v"},
                              {"foob", "Zm9vYg=="},
                              {"fooba", "Zm9vYmE="},
                              {"foobar", "Zm9vYmFy"}};
  for (const auto& s : samples) {
    char out[16] = {};
    const size_t n = base64(reinterpret_cast<const uint8_t*>(s[0]), std::strlen(s[0]), out);
    check(n == base64Length(std::strlen(s[0])) && std::string(out, n) == s[1], "base64", s[0]);
  }
  {
    // Pieces of a multiple of three join into one encoding.
    std::vector<uint8_t> data(1000);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i * 37 + 11);
    std::string whole(base64Length(data.size()), '?');
    base64(data.data(), data.size(), &whole[0]);
    std::string joined;
    for (size_t at = 0; at < data.size(); at += 300) {
      const size_t n = data.size() - at < 300 ? data.size() - at : 300;
      std::string piece(base64Length(n), '?');
      base64(data.data() + at, n, &piece[0]);
      joined += piece;
    }
    check(whole == joined, "pieces join");
  }

  // JSON
  std::string esc;
  jsonEscape("a\"b\\c\nd\x01", esc);
  check(esc == "a\\\"b\\\\c\\nd\\u0001", "escape", esc);
  std::string out;
  check(stringField("{\"text\" : \"hola\\n\\\"mundo\\\" \\u00e1\\u00F1 \\ud83d\\ude00\"}", "text", out) &&
            out == "hola\n\"mundo\" \xC3\xA1\xC3\xB1 \xF0\x9F\x98\x80",
        "unescape", out);
  check(!stringField("{\"text\": 3}", "text", out), "not a string");
  check(stringField("{\"context\":\"x\",\"text\":\"y\"}", "text", out) && out == "y", "whole key names only", out);
  check(stringField("{\"role\":\"text\",\"text\":\"z\"}", "text", out) && out == "z", "a value named like the key",
        out);
  check(!stringField("{\"text\":\"cut", "text", out), "a cut string");

  // Gemini's answer, as it comes, with the note inside its text.
  const std::string gemini =
      "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"{\\\"tipo\\\": \\\"tarea\\\", \\\"titulo\\\": "
      "\\\"Llamar al notario\\\", \\\"resumen\\\": \\\"Llamar el lunes.\\\\nSin falta.\\\", "
      "\\\"transcripcion\\\": \\\"Tengo que llamar al notario \\u201cel lunes\\u201d\\u2026\\\"}\"}],\"role\":"
      "\"model\"},\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"promptTokenCount\":812}}";
  std::string text;
  Note note;
  check(geminiText(gemini, text), "gemini text");
  check(noteFrom(text, note), "gemini note", text);
  check(note.kind == Kind::Task && note.title == "Llamar al notario" && note.summary == "Llamar el lunes. Sin falta.",
        "fields", note.summary);
  check(note.transcript == "Tengo que llamar al notario \"el lunes\"...", "punctuation the fonts lack",
        note.transcript);

  // OpenAI's two answers.
  check(stringField("{\"text\":\"Una idea: un reloj de arena.\",\"usage\":{}}", "text", out) &&
            out == "Una idea: un reloj de arena.",
        "transcription");
  const std::string chat =
      "{\"id\":\"c\",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"{\\\"tipo\\\":"
      "\\\"idea\\\",\\\"titulo\\\":\\\"Reloj de arena\\\",\\\"resumen\\\":\\\"Un reloj.\\\",\\\"transcripcion\\\":"
      "\\\"Una idea: un reloj de arena.\\\"}\",\"refusal\":null}}]}";
  check(stringField(chat, "content", text) && noteFrom(text, note) && note.kind == Kind::Idea &&
            note.title == "Reloj de arena",
        "chat note", text);
  check(!noteFrom("{\"tipo\":\"nota\"}", note), "no transcript, no note");
  check(noteFrom("{\"transcripcion\":\"x\"}", note) && note.title == "Nota sin titulo" && note.kind == Kind::Note,
        "a title is never empty");

  // Requests are JSON a parser takes whole.
  Models m;
  const std::string body = geminiHead() + "QUJD" + geminiTail();
  check(body.find("\"inline_data\":{\"mime_type\":\"audio/wav\",\"data\":\"QUJD\"}") != std::string::npos,
        "gemini body", body);
  check(geminiUrl(m) == "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.1-flash-lite:generateContent",
        "gemini url");
  const std::string c = chatBody(m, "dijo \"hola\"\n");
  check(c.find("\"model\":\"gpt-6-luna\"") != std::string::npos && c.find("dijo \\\"hola\\\"\\n") != std::string::npos,
        "chat body", c);
  check(multipartHead(m).find("name=\"model\"\r\n\r\ngpt-transcribe\r\n") != std::string::npos &&
            multipartHead(m).find("filename=\"nota.wav\"") != std::string::npos &&
            multipartTail() == std::string("\r\n--") + kBoundary + "--\r\n",
        "multipart");
  Models over;
  readModels("# comentario\ngemini=gemini-3.5-flash-lite\r\nchat=gpt-6.1-terra\nbad\ntranscribe=\n", over);
  check(over.gemini == "gemini-3.5-flash-lite" && over.chat == "gpt-6.1-terra" && over.transcribe == "gpt-transcribe",
        "models file");

  // The note on the card.
  Note a;
  a.kind = Kind::Idea;
  a.title = "Un título";
  a.summary = "Resumen.";
  a.transcript = "Línea uno.\nLínea dos.";
  Note b;
  check(parse(serialise(a), b) && b.kind == Kind::Idea && b.title == a.title && b.summary == a.summary &&
            b.transcript == a.transcript,
        "round trip");
  check(!parse("VOZ0\nIDEA\n", b) && !parse("VOZ1\nIDEA\nx", b), "a broken note is refused");

  // Keys and names.
  char key[64];
  check(tidyKey("  AIza Sy-abc_123\n", key, sizeof(key)) && !std::strcmp(key, "AIzaSy-abc_123"), "tidy key", key);
  check(!tidyKey("short", key, sizeof(key)), "too short");
  // A key pasted on the phone page: tidied the same way, but never cut short.
  check(tidyPastedKey(" AIzaSy-abc_123\r\n", key, sizeof(key)) && !std::strcmp(key, "AIzaSy-abc_123"), "pasted key",
        key);
  check(!tidyPastedKey("short", key, sizeof(key)), "pasted too short");
  const std::string longKey(sizeof(key) - 1, 'k');
  check(tidyPastedKey(longKey.c_str(), key, sizeof(key)) && longKey == key, "pasted key that just fits");
  const std::string tooLong(sizeof(key), 'k');
  check(!tidyPastedKey(tooLong.c_str(), key, sizeof(key)), "pasted key too long is refused, not cut");
  Service named = Service::OpenAI;
  check(serviceFromName("gemini", named) && named == Service::Gemini, "service gemini");
  check(serviceFromName("openai", named) && named == Service::OpenAI, "service openai");
  check(!serviceFromName("claude", named) && !serviceFromName("", named) && !serviceFromName(nullptr, named),
        "unknown service");
  check(phoneStateJson(true, false) == "{\"gemini\":true,\"openai\":false}", "phone state",
        phoneStateJson(true, false).c_str());
  // What the phone page reads the notes as.
  check(isoWhen("20261121-154203") == "2026-11-21 15:42", "iso when", isoWhen("20261121-154203").c_str());
  check(isoWhen("notas.txt").empty(), "iso when of a non-stamp");
  Note exported;
  exported.kind = Kind::Task;
  exported.title = "Llamar a \"Marta\"";
  exported.summary = "Quedar el jueves.";
  exported.transcript = "Tengo que llamar a Marta\npara quedar el jueves.";
  const std::string row = listRowJson("20261121-154203", true, exported, 42);
  check(row ==
            "{\"id\":\"20261121-154203\",\"when\":\"2026-11-21 15:42\",\"seconds\":42,\"done\":true,"
            "\"kind\":\"TAREA\",\"title\":\"Llamar a \\\"Marta\\\"\"}",
        "list row", row.c_str());
  const std::string bare = listRowJson("20261121-154203", false, Note{}, 7);
  check(bare == "{\"id\":\"20261121-154203\",\"when\":\"2026-11-21 15:42\",\"seconds\":7,\"done\":false}", "bare row",
        bare.c_str());
  const std::string whole = noteJson("20261121-154203", exported);
  check(whole.find("\"transcript\":\"Tengo que llamar a Marta\\npara quedar el jueves.\"") != std::string::npos &&
            whole.find("\"summary\":\"Quedar el jueves.\"") != std::string::npos,
        "note json", whole.c_str());
  const std::string md = noteMarkdown("20261121-154203", exported);
  check(md ==
            "## Llamar a \"Marta\"\n\n2026-11-21 15:42 \xC2\xB7 TAREA\n\n**Resumen:** Quedar el jueves.\n\n"
            "Tengo que llamar a Marta\npara quedar el jueves.\n\n---\n\n",
        "markdown", md.c_str());
  Note untitled;
  untitled.transcript = "Solo palabras.";
  const std::string plain = noteMarkdown("20261121-154203", untitled);
  check(plain.find("## Sin t\xC3\xADtulo\n") == 0 && plain.find("Resumen") == std::string::npos, "untitled markdown",
        plain.c_str());
  char requested[16];
  check(stampFromRequest("20261121-154203", requested, sizeof(requested)) && !std::strcmp(requested, "20261121-154203"),
        "stamp from request");
  check(!stampFromRequest("../../etc/passwd", requested, sizeof(requested)) &&
            !stampFromRequest("20261121-154203.wav", requested, sizeof(requested)) &&
            !stampFromRequest("20261121-15420/", requested, sizeof(requested)) &&
            !stampFromRequest("", requested, sizeof(requested)) &&
            !stampFromRequest(nullptr, requested, sizeof(requested)),
        "a request cannot name anything but a stamp");
  char hint[32];
  keyHint("sk-proj-abcd1234", hint, sizeof(hint));
  check(!std::strcmp(hint, "TERMINA EN 1234"), "hint", hint);
  keyHint("", hint, sizeof(hint));
  check(!std::strcmp(hint, "SIN CLAVE"), "no key");
  char s[24];
  stamp(2026, 11, 21, 15, 42, 3, s, sizeof(s));
  check(!std::strcmp(s, "20261121-154203") && isStamp(s), "stamp", s);
  check(!isStamp("notas.txt") && !isStamp("20261121_154203"), "not stamps");
  char w[32];
  when("20261121-154203.wav", w, sizeof(w));
  check(!std::strcmp(w, "21 NOV \xC2\xB7 15:42"), "when", w);
  check(!std::strcmp(failure(401), "CLAVE NO V\xC3\x81LIDA") && !std::strcmp(failure(0), "SIN CONEXI\xC3\x93N") &&
            !std::strcmp(failure(-2), "FALTA LA CLAVE"),
        "failures");

  std::printf("%d checks, %d failed\n", checks, failures);
  if (failures == 0) std::printf("voz: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

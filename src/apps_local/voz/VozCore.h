#pragma once

// VOZ, the part with no hardware in it: the recording as a WAV file, the
// requests that carry it to Gemini or OpenAI, the answers read back, and the
// note as it is kept on the card. Freestanding, so host-tests/voz checks it.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace voz {

// ---------------------------------------------------------------- the audio
constexpr uint32_t kSampleRate = 16000;
constexpr int kMaxSeconds = 60;
constexpr size_t kMaxSamples = static_cast<size_t>(kSampleRate) * kMaxSeconds;
constexpr size_t kWavHeaderBytes = 44;

inline void put16(uint8_t* p, const uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
inline void put32(uint8_t* p, const uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
inline uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

// The 44 bytes in front of 16-bit mono PCM.
inline void wavHeader(uint8_t out[kWavHeaderBytes], const uint32_t dataBytes, const uint32_t rate = kSampleRate) {
  std::memcpy(out, "RIFF", 4);
  put32(out + 4, 36 + dataBytes);
  std::memcpy(out + 8, "WAVEfmt ", 8);
  put32(out + 16, 16);
  put16(out + 20, 1);  // PCM
  put16(out + 22, 1);  // mono
  put32(out + 24, rate);
  put32(out + 28, rate * 2);
  put16(out + 32, 2);
  put16(out + 34, 16);
  std::memcpy(out + 36, "data", 4);
  put32(out + 40, dataBytes);
}

// Seconds of audio a WAV file of `fileBytes` holds, rounded.
inline int secondsIn(const size_t fileBytes) {
  if (fileBytes <= kWavHeaderBytes) return 0;
  return static_cast<int>((fileBytes - kWavHeaderBytes + kSampleRate) / (2 * kSampleRate));
}

// The microphone is quiet: raise the take so its loudest moment sits near
// half of full scale, never by more than eight times, so a silent room is not
// turned into hiss. Returns the gain applied.
inline int normalise(int16_t* pcm, const size_t count) {
  int peak = 0;
  for (size_t i = 0; i < count; ++i) {
    const int v = pcm[i] < 0 ? -static_cast<int>(pcm[i]) : pcm[i];
    if (v > peak) peak = v;
  }
  if (peak == 0) return 1;
  int gain = 16000 / peak;
  if (gain > 8) gain = 8;
  if (gain <= 1) return 1;
  for (size_t i = 0; i < count; ++i) {
    int v = pcm[i] * gain;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    pcm[i] = static_cast<int16_t>(v);
  }
  return gain;
}

// ---------------------------------------------------------------- base64
constexpr size_t base64Length(const size_t bytes) { return (bytes + 2) / 3 * 4; }

// Writes base64Length(n) characters, no terminator. `n` that is not the last
// piece must be a multiple of three, so pieces join into one encoding.
inline size_t base64(const uint8_t* in, const size_t n, char* out) {
  static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t o = 0;
  size_t i = 0;
  for (; i + 2 < n; i += 3) {
    const uint32_t v = (static_cast<uint32_t>(in[i]) << 16) | (static_cast<uint32_t>(in[i + 1]) << 8) | in[i + 2];
    out[o++] = kAlphabet[(v >> 18) & 63];
    out[o++] = kAlphabet[(v >> 12) & 63];
    out[o++] = kAlphabet[(v >> 6) & 63];
    out[o++] = kAlphabet[v & 63];
  }
  if (i < n) {
    uint32_t v = static_cast<uint32_t>(in[i]) << 16;
    if (i + 1 < n) v |= static_cast<uint32_t>(in[i + 1]) << 8;
    out[o++] = kAlphabet[(v >> 18) & 63];
    out[o++] = kAlphabet[(v >> 12) & 63];
    out[o++] = i + 1 < n ? kAlphabet[(v >> 6) & 63] : '=';
    out[o++] = '=';
  }
  return o;
}

// ---------------------------------------------------------------- JSON
inline void jsonEscape(const char* s, std::string& out) {
  for (; *s; ++s) {
    const unsigned char c = static_cast<unsigned char>(*s);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char u[8];
          std::snprintf(u, sizeof(u), "\\u%04x", c);
          out += u;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
}

inline void appendUtf8(const uint32_t cp, std::string& out) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

inline int hexValue(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Reads the JSON string that starts at `p` (on its opening quote) into `out`.
// Returns where it ended, or nullptr when it is not a whole string.
inline const char* readString(const char* p, const char* end, std::string& out) {
  if (p >= end || *p != '"') return nullptr;
  ++p;
  while (p < end && *p != '"') {
    if (*p != '\\') {
      out += *p++;
      continue;
    }
    if (++p >= end) return nullptr;
    const char e = *p++;
    switch (e) {
      case 'n':
        out += '\n';
        break;
      case 't':
        out += '\t';
        break;
      case 'r':
        break;
      case 'b':
      case 'f':
        out += ' ';
        break;
      case 'u': {
        uint32_t cp = 0;
        for (int i = 0; i < 4; ++i) {
          const int h = p < end ? hexValue(*p++) : -1;
          if (h < 0) return nullptr;
          cp = (cp << 4) | static_cast<uint32_t>(h);
        }
        if (cp >= 0xD800 && cp < 0xDC00 && p + 6 <= end && p[0] == '\\' && p[1] == 'u') {
          uint32_t low = 0;
          bool ok = true;
          for (int i = 2; i < 6; ++i) {
            const int h = hexValue(p[i]);
            if (h < 0) ok = false;
            low = (low << 4) | static_cast<uint32_t>(h < 0 ? 0 : h);
          }
          if (ok && low >= 0xDC00 && low < 0xE000) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            p += 6;
          }
        }
        appendUtf8(cp, out);
        break;
      }
      default:
        out += e;  // \" \\ \/
    }
  }
  return p < end ? p + 1 : nullptr;
}

// The first string value under `key` anywhere in `json`. Enough for answers
// whose shape is fixed: the field wanted is the first of its name.
inline bool stringField(const std::string& json, const char* key, std::string& out) {
  out.clear();
  std::string quoted = "\"";
  quoted += key;
  quoted += '"';
  const char* begin = json.data();
  const char* end = begin + json.size();
  size_t from = 0;
  while ((from = json.find(quoted, from)) != std::string::npos) {
    const char* p = begin + from + quoted.size();
    from += quoted.size();
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    if (p >= end || *p != ':') continue;
    ++p;
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    if (p < end && *p == '"' && readString(p, end, out)) return true;
    out.clear();
  }
  return false;
}

// Punctuation the device's text faces do not carry, as the nearest they do.
inline void plainPunctuation(std::string& s) {
  struct Swap {
    const char* from;
    const char* to;
  };
  static const Swap kSwaps[] = {{"\xE2\x80\x9C", "\""},  {"\xE2\x80\x9D", "\""}, {"\xE2\x80\x98", "'"},
                                {"\xE2\x80\x99", "'"},   {"\xE2\x80\x94", "-"},  {"\xE2\x80\x93", "-"},
                                {"\xE2\x80\xA6", "..."}, {"\xC2\xA0", " "}};
  for (const Swap& w : kSwaps) {
    size_t at = 0;
    const size_t len = std::strlen(w.from);
    while ((at = s.find(w.from, at)) != std::string::npos) {
      s.replace(at, len, w.to);
      at += std::strlen(w.to);
    }
  }
}

// ---------------------------------------------------------------- the note
enum class Kind : uint8_t { Note, Idea, Task };
inline const char* kindTag(const Kind k) { return k == Kind::Idea ? "IDEA" : (k == Kind::Task ? "TAREA" : "NOTA"); }
inline Kind kindFrom(const std::string& s) {
  if (s.size() >= 4 && (s[0] == 'i' || s[0] == 'I')) return Kind::Idea;
  if (s.size() >= 5 && (s[0] == 't' || s[0] == 'T')) return Kind::Task;
  return Kind::Note;
}

struct Note {
  Kind kind = Kind::Note;
  std::string title;
  std::string summary;
  std::string transcript;
};

// What the model is asked for. One instruction for both services, so a note
// reads the same whichever wrote it.
inline const char* instructions() {
  return "Eres un asistente que ordena notas de voz. Recibes una grabacion corta. Devuelve SOLO un objeto JSON "
         "con estas claves: \"tipo\" (\"idea\", \"tarea\" o \"nota\": tarea si pide hacer algo, idea si propone "
         "algo nuevo, nota en otro caso), \"titulo\" (de 3 a 7 palabras, sin punto final), \"resumen\" (una o "
         "dos frases claras) y \"transcripcion\" (lo dicho, literal, con puntuacion correcta, sin muletillas). "
         "Escribe titulo y resumen en el idioma en que se habla.";
}

inline void oneLine(std::string& s) {
  for (char& c : s) {
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
  }
  while (!s.empty() && s.back() == ' ') s.pop_back();
}

// The model's JSON (the text it answered with) as a note.
inline bool noteFrom(const std::string& modelJson, Note& note) {
  std::string kind;
  stringField(modelJson, "tipo", kind);
  if (!stringField(modelJson, "transcripcion", note.transcript) || note.transcript.empty()) return false;
  stringField(modelJson, "titulo", note.title);
  stringField(modelJson, "resumen", note.summary);
  note.kind = kindFrom(kind);
  oneLine(note.title);
  oneLine(note.summary);
  plainPunctuation(note.title);
  plainPunctuation(note.summary);
  plainPunctuation(note.transcript);
  if (note.title.empty()) note.title = "Nota sin titulo";
  return true;
}

// On the card: a first line naming the format, the kind, the title, the
// summary, then the transcript to the end of the file. Readable as plain text.
inline std::string serialise(const Note& n) {
  std::string out = "VOZ1\n";
  out += kindTag(n.kind);
  out += '\n';
  out += n.title;
  out += '\n';
  out += n.summary;
  out += '\n';
  out += n.transcript;
  return out;
}
inline bool parse(const std::string& text, Note& n) {
  if (text.compare(0, 5, "VOZ1\n") != 0) return false;
  size_t at = 5;
  std::string lines[3];
  for (std::string& line : lines) {
    const size_t nl = text.find('\n', at);
    if (nl == std::string::npos) return false;
    line = text.substr(at, nl - at);
    at = nl + 1;
  }
  n.kind = lines[0] == "IDEA" ? Kind::Idea : (lines[0] == "TAREA" ? Kind::Task : Kind::Note);
  n.title = lines[1];
  n.summary = lines[2];
  n.transcript = text.substr(at);
  return true;
}

// ---------------------------------------------------------------- services
enum class Service : uint8_t { Gemini, OpenAI, Count };

struct Models {
  std::string gemini = "gemini-3.1-flash-lite";
  std::string transcribe = "gpt-transcribe";
  std::string chat = "gpt-6-luna";
};

// Lines "gemini=...", "transcribe=...", "chat=..." override the defaults, so a
// renamed model is a text edit on the card rather than a new firmware.
inline void readModels(const std::string& text, Models& m) {
  size_t at = 0;
  while (at < text.size()) {
    size_t nl = text.find('\n', at);
    if (nl == std::string::npos) nl = text.size();
    std::string line = text.substr(at, nl - at);
    at = nl + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    const size_t eq = line.find('=');
    if (eq == std::string::npos || eq + 1 >= line.size()) continue;
    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);
    if (key == "gemini") m.gemini = value;
    if (key == "transcribe") m.transcribe = value;
    if (key == "chat") m.chat = value;
  }
}

// Gemini: one request with the audio inline; the answer is the note.
inline std::string geminiUrl(const Models& m) {
  return "https://generativelanguage.googleapis.com/v1beta/models/" + m.gemini + ":generateContent";
}
inline std::string geminiHead() {
  std::string s = "{\"contents\":[{\"parts\":[{\"text\":\"";
  jsonEscape(instructions(), s);
  s += "\"},{\"inline_data\":{\"mime_type\":\"audio/wav\",\"data\":\"";
  return s;
}
inline const char* geminiTail() {
  return "\"}}]}],\"generationConfig\":{\"responseMimeType\":\"application/json\",\"temperature\":0.2}}";
}
// The model's text inside Gemini's answer.
inline bool geminiText(const std::string& body, std::string& out) { return stringField(body, "text", out); }

// OpenAI: the audio to the transcription endpoint, then the words to a chat
// model for the title, the kind and the summary.
constexpr char kOpenAiTranscribeUrl[] = "https://api.openai.com/v1/audio/transcriptions";
constexpr char kOpenAiChatUrl[] = "https://api.openai.com/v1/chat/completions";
constexpr char kBoundary[] = "----crossplay-voz-7d1f3a";
inline std::string multipartHead(const Models& m) {
  std::string s;
  s += "--";
  s += kBoundary;
  s += "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n";
  s += m.transcribe;
  s += "\r\n--";
  s += kBoundary;
  s += "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n--";
  s += kBoundary;
  s += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"nota.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  return s;
}
inline std::string multipartTail() { return std::string("\r\n--") + kBoundary + "--\r\n"; }
inline std::string multipartType() { return std::string("multipart/form-data; boundary=") + kBoundary; }
inline std::string chatBody(const Models& m, const std::string& transcript) {
  std::string s = "{\"model\":\"";
  jsonEscape(m.chat.c_str(), s);
  s += "\",\"reasoning_effort\":\"none\",\"response_format\":{\"type\":\"json_object\"},\"messages\":[{\"role\":"
       "\"system\",\"content\":\"";
  jsonEscape(instructions(), s);
  s += " La transcripcion ya esta hecha: devuelvela tal cual en \\\"transcripcion\\\".\"},{\"role\":\"user\","
       "\"content\":\"";
  jsonEscape(transcript.c_str(), s);
  s += "\"}]}";
  return s;
}

// A failed request, said briefly: the list shows it in place of a title.
inline const char* failure(const int status) {
  if (status == 0) return "SIN CONEXIÓN";
  if (status == 401 || status == 403) return "CLAVE NO VÁLIDA";
  if (status == 404) return "MODELO NO ENCONTRADO";
  if (status == 413) return "GRABACIÓN DEMASIADO LARGA";
  if (status == 429) return "LÍMITE DE USO ALCANZADO";
  if (status >= 500) return "EL SERVICIO FALLA";
  if (status == -1) return "RESPUESTA ILEGIBLE";
  if (status == -2) return "FALTA LA CLAVE";
  if (status == -3) return "SIN MEMORIA";
  return "PETICIÓN RECHAZADA";
}

// A key as typed, trimmed of the spaces and line ends a paste brings.
inline bool tidyKey(const char* in, char* out, const size_t cap) {
  size_t n = 0;
  for (const char* p = in; *p && n + 1 < cap; ++p) {
    if (*p > ' ' && *p < 0x7F) out[n++] = *p;
  }
  out[n] = 0;
  return n >= 8;
}

// The longest key kept. Gemini's are 39 characters and OpenAI's project keys
// run past 160.
constexpr size_t kMaxKeyChars = 199;

// A key pasted on the phone page. Unlike one typed on the panel it can be cut
// short by the paste, and a key cut to fit would be saved and then refused by
// the service with no hint why, so one too long to keep whole is turned away.
inline bool tidyPastedKey(const char* in, char* out, const size_t cap) {
  size_t printable = 0;
  for (const char* p = in; *p; ++p) {
    if (*p > ' ' && *p < 0x7F) ++printable;
  }
  if (printable + 1 > cap) return false;
  return tidyKey(in, out, cap);
}

// The service a phone page's request names.
inline bool serviceFromName(const char* name, Service& service) {
  if (name == nullptr) return false;
  if (!std::strcmp(name, "gemini")) {
    service = Service::Gemini;
    return true;
  }
  if (!std::strcmp(name, "openai")) {
    service = Service::OpenAI;
    return true;
  }
  return false;
}

// What the phone page is told: which services have a key, and nothing of one.
inline std::string phoneStateJson(const bool gemini, const bool openai) {
  return std::string("{\"gemini\":") + (gemini ? "true" : "false") + ",\"openai\":" + (openai ? "true" : "false") + "}";
}

// "····a1b2": enough of a key to recognise it, not enough to read it off.
inline void keyHint(const char* key, char* out, const size_t cap) {
  const size_t n = std::strlen(key);
  if (n == 0) {
    std::snprintf(out, cap, "SIN CLAVE");
    return;
  }
  std::snprintf(out, cap, "TERMINA EN %s", key + (n > 4 ? n - 4 : 0));
}

// ---------------------------------------------------------------- names
// A note is named by when it was recorded: 20261121-154203.
inline void stamp(const int year, const int month, const int day, const int hour, const int minute, const int second,
                  char* out, const size_t cap) {
  std::snprintf(out, cap, "%04d%02d%02d-%02d%02d%02d", year % 10000, month % 100, day % 100, hour % 100, minute % 100,
                second % 100);
}
inline bool isStamp(const char* s) {
  if (std::strlen(s) < 15 || s[8] != '-') return false;
  for (int i = 0; i < 15; ++i) {
    if (i != 8 && (s[i] < '0' || s[i] > '9')) return false;
  }
  return true;
}
// "21 NOV · 15:42" from a stamp.
inline void when(const char* s, char* out, const size_t cap) {
  static const char* const kMonths[] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN",
                                        "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};
  if (!isStamp(s)) {
    std::snprintf(out, cap, "%s", s);
    return;
  }
  const int month = (s[4] - '0') * 10 + (s[5] - '0');
  const int day = (s[6] - '0') * 10 + (s[7] - '0');
  std::snprintf(out, cap, "%d %s · %c%c:%c%c", day, kMonths[(month >= 1 && month <= 12) ? month - 1 : 0], s[9], s[10],
                s[11], s[12]);
}

// ---------------------------------------------------------------- export
// What the phone page reads the notes as. All of it is built here, off the
// card and off the server, so it can be tested on a host.

constexpr char kNotesDir[] = "/voz";

// "2026-11-21 15:42" from a note's stamp: the form a file kept for years sorts
// and reads by. Empty when the name is not a stamp.
inline std::string isoWhen(const char* s) {
  if (!isStamp(s)) return "";
  char out[20];
  std::snprintf(out, sizeof(out), "%.4s-%.2s-%.2s %.2s:%.2s", s, s + 4, s + 6, s + 9, s + 11);
  return out;
}

// One row of the page's list. A take with no note yet is listed too, so its
// audio can still be fetched.
inline std::string listRowJson(const char* stamp, const bool done, const Note& note, const int seconds) {
  std::string out = "{\"id\":\"";
  out += stamp;
  out += "\",\"when\":\"";
  out += isoWhen(stamp);
  out += "\",\"seconds\":";
  out += std::to_string(seconds);
  out += ",\"done\":";
  out += done ? "true" : "false";
  if (done) {
    out += ",\"kind\":\"";
    out += kindTag(note.kind);
    out += "\",\"title\":\"";
    jsonEscape(note.title.c_str(), out);
    out += "\"";
  }
  out += "}";
  return out;
}

// A whole note, for the page to show and copy.
inline std::string noteJson(const char* stamp, const Note& note) {
  std::string out = "{\"id\":\"";
  out += stamp;
  out += "\",\"when\":\"";
  out += isoWhen(stamp);
  out += "\",\"kind\":\"";
  out += kindTag(note.kind);
  out += "\",\"title\":\"";
  jsonEscape(note.title.c_str(), out);
  out += "\",\"summary\":\"";
  jsonEscape(note.summary.c_str(), out);
  out += "\",\"transcript\":\"";
  jsonEscape(note.transcript.c_str(), out);
  out += "\"}";
  return out;
}

// A note as Markdown: a heading, when and what kind, the summary, the words.
// Plain enough to read as text and to paste into any notes app.
inline std::string noteMarkdown(const char* stamp, const Note& note) {
  std::string out = "## ";
  out += note.title.empty() ? "Sin t\xC3\xADtulo" : note.title;
  out += "\n\n";
  out += isoWhen(stamp);
  out += " \xC2\xB7 ";
  out += kindTag(note.kind);
  out += "\n\n";
  if (!note.summary.empty()) {
    out += "**Resumen:** ";
    out += note.summary;
    out += "\n\n";
  }
  out += note.transcript;
  out += "\n\n---\n\n";
  return out;
}

// The stamp a request names, checked before it is put in a path: a stamp is
// digits and one dash, so nothing else can reach the card through it.
inline bool stampFromRequest(const char* id, char* out, const size_t cap) {
  if (id == nullptr || std::strlen(id) != 15 || !isStamp(id) || cap < 16) return false;
  std::snprintf(out, cap, "%s", id);
  return true;
}

}  // namespace voz

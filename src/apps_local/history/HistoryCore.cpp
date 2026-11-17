#include "HistoryCore.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../discover/DiscoverCore.h"

namespace history {

namespace {

constexpr const char* kKindNames[kKinds] = {"CIENCIA", "HISTORIA", "CULTURA"};
constexpr const char* kMonths[12] = {"ENERO", "FEBRERO", "MARZO",      "ABRIL",   "MAYO",      "JUNIO",
                                     "JULIO", "AGOSTO",  "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"};
constexpr int kMonthDays[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Lower case, ASCII and the Spanish letters, which are two bytes with a C3
// lead: their upper and lower forms differ by 0x20 in the second.
std::string lowered(const std::string_view text) {
  std::string out(text);
  for (size_t i = 0; i < out.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(out[i]);
    if (c >= 'A' && c <= 'Z') {
      out[i] = static_cast<char>(c + 32);
    } else if (c == 0xC3 && i + 1 < out.size()) {
      const unsigned char next = static_cast<unsigned char>(out[i + 1]);
      if (next >= 0x80 && next <= 0x9E && next != 0x97) out[i + 1] = static_cast<char>(next + 0x20);
      ++i;
    }
  }
  return out;
}

bool hasAny(const std::string& text, const char* const* words, const int count) {
  for (int i = 0; i < count; ++i) {
    if (text.find(words[i]) != std::string::npos) return true;
  }
  return false;
}

constexpr const char* kScienceWords[] = {
    "descubr",
    "invent",
    "cient\xC3\xAD"
    "f",
    "f\xC3\xADsic",
    "qu\xC3\xADmic",
    "astr\xC3\xB3nom",
    "astronom",
    "telescopio",
    "sat\xC3\xA9lite",
    "nasa",
    "sonda",
    "espacial",
    "astronaut",
    "cosmonaut",
    "vacuna",
    "teor\xC3\xAD"
    "a",
    "experiment",
    "patent",
    "computador",
    "ordenador",
    "internet",
    "adn",
    "elemento qu",
    "cometa",
    "planeta",
    "eclipse",
    "transbordador",
    "cohete",
    "trasplante",
    "transplante",
    "matem\xC3\xA1tic",
    "ingenier",
    "telegr",
    "tel\xC3\xA9"
    "fono",
    "radio",
    "nuclear",
    "at\xC3\xB3mic",
    "genoma",
    "f\xC3\xB3sil",
    "medicina",
    "microscop",
    "luna",
    "marte",
    "\xC3\xB3rbita",
    "software",
    "primer vuelo",
    "apolo",
    "soyuz",
    "observatorio",
};
constexpr const char* kCultureWords[] = {
    "publica",
    "estren",
    "pel\xC3\xAD"
    "cula",
    "novela",
    "\xC3\xA1lbum",
    "\xC3\xB3pera",
    "sinfon\xC3\xAD"
    "a",
    "museo",
    "pint",
    "escritor",
    "poeta",
    "teatro",
    "literatura",
    "canci\xC3\xB3n",
    "banda",
    "concierto",
    "libro",
    "videojuego",
    "serie de televisi",
    "ol\xC3\xADmpic",
    "f\xC3\xBAtbol",
    "campe",
    "copa",
    "disco",
    "sencillo",
    "ballet",
    "cine",
    "festival",
    "premio",
    "poema",
    "revista",
    "peri\xC3\xB3"
    "dico",
    "emite",
    "televisi\xC3\xB3n",
    "compositor",
    "m\xC3\xBAsic",
    "cantante",
    "actor",
    "actriz",
    "arquitect",
    "catedral",
    "universidad",
    "biblioteca",
    "exposici\xC3\xB3n",
    "juegos",
    "mundial de",
    "torneo",
    "r\xC3\xA9"
    "cord",
    "marat\xC3\xB3n",
};

}  // namespace

const char* kindName(const Kind kind) {
  const int i = static_cast<int>(kind);
  return kKindNames[i >= 0 && i < kKinds ? i : 1];
}

Kind classify(const std::string_view text) {
  const std::string low = lowered(text);
  if (hasAny(low, kScienceWords, sizeof(kScienceWords) / sizeof(kScienceWords[0]))) return Kind::Science;
  if (hasAny(low, kCultureWords, sizeof(kCultureWords) / sizeof(kCultureWords[0]))) return Kind::Culture;
  return Kind::History;
}

void Scanner::endItem() {
  ++seen_;
  if (text_.empty() || !hasYear_ || static_cast<int>(out_.size()) >= max_) return;
  // What the reading face cannot set is dropped; what is left is the event.
  discover::latinOnly(text_.data());
  text_.resize(std::strlen(text_.c_str()));
  if (text_.size() < 12 || year_ < -9999 || year_ > 9999) return;
  Event event;
  event.year = static_cast<int16_t>(year_);
  event.kind = classify(text_);
  event.selected = section_ == "selected";
  event.text = text_;
  out_.push_back(std::move(event));
}

void Scanner::endString() {
  if (depth_ == 1 && !afterColon_) {
    section_ = token_;
  } else if (depth_ == 3 && !afterColon_) {
    key_ = token_;
  } else if (depth_ == 3 && afterColon_ && key_ == "text") {
    text_ = token_;
  }
  // A string value closes the pair; a key waits for its colon.
  if (afterColon_) afterColon_ = false;
  keep_ = false;
  token_.clear();
}

void Scanner::endNumber() {
  inNumber_ = false;
  if (depth_ == 3 && key_ == "year" && !token_.empty()) {
    int value = 0;
    bool negative = false;
    for (const char c : token_) {
      if (c == '-') {
        negative = true;
      } else if (c >= '0' && c <= '9' && value < 100000) {
        value = value * 10 + (c - '0');
      } else {
        break;
      }
    }
    year_ = negative ? -value : value;
    hasYear_ = true;
  }
  afterColon_ = false;
  token_.clear();
}

void Scanner::feed(const char* data, const size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const char c = data[i];
    if (inString_) {
      if (unicodeLeft_ > 0) {
        int h = -1;
        if (c >= '0' && c <= '9') h = c - '0';
        if (c >= 'a' && c <= 'f') h = c - 'a' + 10;
        if (c >= 'A' && c <= 'F') h = c - 'A' + 10;
        unicode_ = (unicode_ << 4) | static_cast<uint32_t>(h < 0 ? 0 : h);
        if (--unicodeLeft_ > 0) continue;
        uint32_t cp = unicode_;
        if (cp >= 0xD800 && cp < 0xDC00) {
          highSurrogate_ = cp;
          continue;
        }
        if (cp >= 0xDC00 && cp < 0xE000 && highSurrogate_ != 0) {
          cp = 0x10000 + ((highSurrogate_ - 0xD800) << 10) + (cp - 0xDC00);
        }
        highSurrogate_ = 0;
        if (keep_ && token_.size() < 600) {
          if (cp < 0x80) {
            token_.push_back(static_cast<char>(cp));
          } else if (cp < 0x800) {
            token_.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            token_.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
          } else if (cp < 0x10000) {
            token_.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            token_.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            token_.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
          } else {
            token_.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            token_.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            token_.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            token_.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
          }
        }
        continue;
      }
      if (escaped_) {
        escaped_ = false;
        if (c == 'u') {
          unicodeLeft_ = 4;
          unicode_ = 0;
        } else if (keep_ && token_.size() < 600) {
          token_.push_back(c == 'n' || c == 't' || c == 'r' ? ' ' : c);
        }
        continue;
      }
      if (c == '\\') {
        escaped_ = true;
        continue;
      }
      if (c == '"') {
        inString_ = false;
        endString();
        continue;
      }
      if (keep_ && token_.size() < 600) token_.push_back(c);
      continue;
    }

    if (inNumber_) {
      if ((c >= '0' && c <= '9') || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+') {
        if (token_.size() < 12) token_.push_back(c);
        continue;
      }
      endNumber();
      // The character that ended it is handled below.
    }

    switch (c) {
      case '"':
        inString_ = true;
        // Only the root's keys and an item's own keys and text are kept.
        keep_ = (depth_ == 1 && !afterColon_) || (depth_ == 3 && (!afterColon_ || key_ == "text"));
        token_.clear();
        break;
      case ':':
        afterColon_ = true;
        break;
      case ',':
        afterColon_ = false;
        break;
      case '{':
      case '[':
        ++depth_;
        afterColon_ = false;
        if (c == '{' && depth_ == 3) {
          text_.clear();
          key_.clear();
          hasYear_ = false;
          year_ = 0;
        }
        break;
      case '}':
      case ']':
        if (c == '}' && depth_ == 3) endItem();
        if (depth_ > 0) --depth_;
        afterColon_ = false;
        break;
      default:
        if (afterColon_ && ((c >= '0' && c <= '9') || c == '-')) {
          inNumber_ = true;
          token_.assign(1, c);
        } else if (afterColon_ && (c == 't' || c == 'f' || c == 'n')) {
          // true, false, null: a value with nothing to keep.
          afterColon_ = false;
        }
        break;
    }
  }
}

void sortEvents(std::vector<Event>& events) {
  std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
    if (a.year != b.year) return a.year > b.year;
    return a.selected && !b.selected;
  });
}

void dropRepeats(std::vector<Event>& events) {
  std::vector<Event> kept;
  kept.reserve(events.size());
  for (Event& e : events) {
    bool repeat = false;
    for (Event& k : kept) {
      if (k.year != e.year) continue;
      const size_t n = std::min<size_t>(40, std::min(k.text.size(), e.text.size()));
      if (n >= 20 && k.text.compare(0, n, e.text, 0, n) == 0) {
        repeat = true;
        k.selected = k.selected || e.selected;
      }
    }
    if (!repeat) kept.push_back(std::move(e));
  }
  events.swap(kept);
}

std::string pack(const std::vector<Event>& events) {
  std::string out;
  out.reserve(events.size() * 160);
  char head[40];
  for (const Event& e : events) {
    std::snprintf(head, sizeof(head), "%d\t%d\t%d\t", static_cast<int>(e.kind), e.selected ? 1 : 0,
                  static_cast<int>(e.year));
    out += head;
    for (const char c : e.text) out.push_back(c == '\t' || c == '\n' ? ' ' : c);
    out.push_back('\n');
  }
  return out;
}

void unpack(const std::string_view text, std::vector<Event>& events) {
  events.clear();
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(at, end - at);
    at = end + 1;
    int fields[3] = {0, 0, 0};
    bool ok = true;
    for (int f = 0; f < 3 && ok; ++f) {
      const size_t tab = line.find('\t');
      if (tab == std::string_view::npos) {
        ok = false;
        break;
      }
      bool negative = false;
      for (size_t i = 0; i < tab; ++i) {
        if (line[i] == '-' && i == 0) {
          negative = true;
        } else if (line[i] >= '0' && line[i] <= '9' && fields[f] < 100000) {
          fields[f] = fields[f] * 10 + (line[i] - '0');
        } else {
          ok = false;
        }
      }
      if (negative) fields[f] = -fields[f];
      line.remove_prefix(tab + 1);
    }
    if (!ok || fields[0] < 0 || fields[0] >= kKinds || line.empty() || fields[2] < -9999 || fields[2] > 9999) continue;
    Event e;
    e.kind = static_cast<Kind>(fields[0]);
    e.selected = fields[1] != 0;
    e.year = static_cast<int16_t>(fields[2]);
    e.text = std::string(line);
    events.push_back(std::move(e));
  }
}

void feedUrl(const char* type, const int month, const int day, char* out, const size_t cap) {
  std::snprintf(out, cap, "https://es.wikipedia.org/api/rest_v1/feed/onthisday/%s/%02d/%02d", type, month % 100,
                day % 100);
}

int daysInMonth(const int month) { return month >= 1 && month <= 12 ? kMonthDays[month - 1] : 30; }

void dayLabel(const int month, const int day, char* out, const size_t cap) {
  std::snprintf(out, cap, "%d DE %s", day % 100, kMonths[month >= 1 && month <= 12 ? month - 1 : 0]);
}

void stepDay(int& month, int& day, const int by) {
  day += by > 0 ? 1 : -1;
  if (day < 1) {
    month = month <= 1 ? 12 : month - 1;
    day = daysInMonth(month);
  } else if (day > daysInMonth(month)) {
    month = month >= 12 ? 1 : month + 1;
    day = 1;
  }
}

void yearLabel(const int year, char* out, const size_t cap) {
  if (year < 0) {
    std::snprintf(out, cap, "%d A. C.", (-year) % 100000);
  } else {
    std::snprintf(out, cap, "%d", year % 100000);
  }
}

int countOf(const std::vector<Event>& events, const Kind kind) {
  int n = 0;
  for (const Event& e : events) n += e.kind == kind ? 1 : 0;
  return n;
}

int featured(const std::vector<Event>& events, const Kind kind, const int month, const int day) {
  int picks = 0;
  int all = 0;
  for (const Event& e : events) {
    if (e.kind != kind) continue;
    ++all;
    if (e.selected) ++picks;
  }
  if (all == 0) return -1;
  const bool fromPicks = picks > 0;
  const int pool = fromPicks ? picks : all;
  int want = (month * 31 + day * 7 + static_cast<int>(kind) * 3) % pool;
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i].kind != kind || (fromPicks && !events[i].selected)) continue;
    if (want-- == 0) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace history

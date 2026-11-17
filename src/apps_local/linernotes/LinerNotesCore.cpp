#include "LinerNotesCore.h"

#include <cstdlib>
#include <cstring>

namespace linernotes {

const char* const kCategoryNames[kCategories] = {"ESSENTIAL",    "INFLUENTIAL",    "CULT CLASSIC",         "HIDDEN GEM",
                                                 "EXPERIMENTAL", "GENRE-DEFINING", "COLLECTOR'S DISCOVERY"};
const char* const kMarkNames[static_cast<int>(Mark::Count)] = {"NOT MARKED", "WANT TO LISTEN", "LISTENING",
                                                               "REDISCOVERED", "FAVOURITE"};

namespace {

constexpr int kIndexFields = 11;

struct Heading {
  char key;
  const char* es;
  const char* en;
};
constexpr Heading kHeadings[] = {
    {'W', "Por qu\xC3\xA9 importa", "Why it matters"},
    {'C', "Contexto", "Context"},
    {'S', "El sonido", "The sound"},
    {'L', "Qu\xC3\xA9 escuchar", "What to listen for"},
    {'T', "Temas esenciales", "Essential tracks"},
    {'U', "Curiosidades", "Curiosities"},
    {'E', "Clasificaci\xC3\xB3n", "Classification"},
    {'R', "Fuentes", "Sources"},
};

void addLine(Document& out, const char* text, const size_t length, const Style style) {
  Line line;
  line.start = static_cast<uint32_t>(out.text.size());
  line.length = static_cast<uint16_t>(length);
  line.style = style;
  out.text.append(text, length);
  out.text.push_back('\0');
  out.lines.push_back(line);
}

// Where a UTF-8 sequence may be cut: never inside a character.
size_t cutPoint(const char* text, size_t at) {
  while (at > 1 && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) --at;
  return at;
}

// Greedy wrap of one paragraph. A word wider than the line (a web address) is
// broken where it stops fitting.
void wrap(Document& out, const char* text, size_t length, const int width, const Measure measure, void* context) {
  while (length > 0 && *text == ' ') ++text, --length;
  while (length > 0) {
    size_t fits = 0;  // bytes of the longest prefix ending at a word boundary that fits
    size_t at = 0;
    while (at < length) {
      size_t end = at;
      while (end < length && text[end] != ' ') ++end;
      if (measure(context, text, end, false) > width) break;
      fits = end;
      at = end;
      while (at < length && text[at] == ' ') ++at;
    }
    if (fits == 0) {
      fits = length;
      while (fits > 1 && measure(context, text, fits, false) > width) fits = cutPoint(text, fits - 1);
    }
    addLine(out, text, fits, Style::Body);
    text += fits;
    length -= fits;
    while (length > 0 && *text == ' ') ++text, --length;
  }
}

}  // namespace

bool Index::parse(std::string&& text) {
  pool = std::move(text);
  albums.clear();
  if (pool.compare(0, 13, "LINERNOTES\t1\t") != 0) return false;
  size_t at = pool.find('\n');
  if (at == std::string::npos) return false;
  const long promised = std::strtol(pool.c_str() + 13, nullptr, 10);
  albums.reserve(promised > 0 && promised < 5000 ? static_cast<size_t>(promised) : 64);
  ++at;
  while (at < pool.size()) {
    size_t end = pool.find('\n', at);
    if (end == std::string::npos) end = pool.size();
    uint32_t field[kIndexFields] = {};
    int count = 0;
    field[count++] = static_cast<uint32_t>(at);
    for (size_t i = at; i < end; ++i) {
      if (pool[i] != '\t') continue;
      pool[i] = '\0';
      if (count < kIndexFields) field[count++] = static_cast<uint32_t>(i + 1);
    }
    if (end < pool.size()) pool[end] = '\0';
    if (count == kIndexFields) {
      Album a;
      a.id = field[0];
      a.artist = field[1];
      a.title = field[2];
      a.year = static_cast<uint16_t>(std::strtol(str(field[3]), nullptr, 10));
      a.label = field[4];
      a.categories = static_cast<uint8_t>(std::strtol(str(field[5]), nullptr, 10));
      a.plexKey = field[6];
      for (int lang = 0; lang < 2; ++lang) {
        a.offset[lang] = static_cast<uint32_t>(std::strtoul(str(field[7 + lang * 2]), nullptr, 10));
        a.length[lang] = static_cast<uint32_t>(std::strtoul(str(field[8 + lang * 2]), nullptr, 10));
      }
      albums.push_back(a);
    }
    at = end + 1;
  }
  return !albums.empty();
}

int Index::find(const char* id) const {
  for (size_t i = 0; i < albums.size(); ++i) {
    if (std::strcmp(str(albums[i].id), id) == 0) return static_cast<int>(i);
  }
  return -1;
}

void layout(const char* record, const size_t length, const bool spanish, const int width, const Measure measure,
            void* context, Document& out) {
  out.text.clear();
  out.lines.clear();
  out.related.clear();
  out.text.reserve(length + length / 8 + 256);
  out.lines.reserve(160);
  out.related.reserve(5);
  char last = 0;
  std::string piece;
  size_t at = 0;
  while (at < length) {
    size_t end = at;
    while (end < length && record[end] != '\n') ++end;
    if (end - at >= 3 && record[at + 1] == '\t') {
      const char key = record[at];
      const char* body = record + at + 2;
      const size_t bodyLen = end - at - 2;
      const char* tab = static_cast<const char*>(std::memchr(body, '\t', bodyLen));
      if (key == 'X') {
        if (tab != nullptr) {
          Related r;
          r.album = static_cast<uint16_t>(std::strtol(body, nullptr, 10));
          r.note.assign(tab + 1, bodyLen - static_cast<size_t>(tab + 1 - body));
          out.related.push_back(std::move(r));
        }
      } else {
        if (key != last) {
          for (const Heading& h : kHeadings) {
            if (h.key != key) continue;
            if (!out.lines.empty()) addLine(out, "", 0, Style::Gap);
            const char* name = spanish ? h.es : h.en;
            addLine(out, name, std::strlen(name), Style::Heading);
          }
          last = key;
        }
        piece.clear();
        const bool listed = key == 'L' || key == 'T' || key == 'U' || key == 'E' || key == 'R';
        if (listed) piece = "- ";
        if (tab != nullptr && key == 'R') {
          piece.append(body, static_cast<size_t>(tab - body));
          wrap(out, piece.data(), piece.size(), width, measure, context);
          piece.assign(tab + 1, bodyLen - static_cast<size_t>(tab + 1 - body));
        } else if (tab != nullptr) {
          piece.append(body, static_cast<size_t>(tab - body));
          piece += key == 'E' ? ": " : ". ";
          piece.append(tab + 1, bodyLen - static_cast<size_t>(tab + 1 - body));
        } else {
          piece.append(body, bodyLen);
        }
        wrap(out, piece.data(), piece.size(), width, measure, context);
      }
    }
    at = end + 1;
  }
}

Mark Kept::markOf(const char* id) const {
  for (const Marked& m : marks) {
    if (m.id == id) return m.mark;
  }
  return Mark::None;
}

void Kept::setMark(const char* id, const Mark mark) {
  for (size_t i = 0; i < marks.size(); ++i) {
    if (marks[i].id != id) continue;
    if (mark == Mark::None) {
      marks.erase(marks.begin() + static_cast<long>(i));
    } else {
      marks[i].mark = mark;
    }
    return;
  }
  if (mark != Mark::None) marks.push_back({id, mark});
}

void Kept::opened(const char* id) {
  for (size_t i = 0; i < recent.size(); ++i) {
    if (recent[i] == id) {
      recent.erase(recent.begin() + static_cast<long>(i));
      break;
    }
  }
  recent.insert(recent.begin(), id);
  if (recent.size() > kRecentMax) recent.resize(kRecentMax);
}

std::string Kept::pack() const {
  std::string out;
  out.reserve(32 + marks.size() * 12 + recent.size() * 10);
  if (spanish >= 0) out += spanish != 0 ? "L es\n" : "L en\n";
  for (const Marked& m : marks) {
    out += "M ";
    out += m.id;
    out += ' ';
    out += static_cast<char>('0' + static_cast<int>(m.mark));
    out += '\n';
  }
  for (const std::string& id : recent) {
    out += "R ";
    out += id;
    out += '\n';
  }
  return out;
}

void Kept::unpack(const std::string& text) {
  marks.clear();
  recent.clear();
  spanish = -1;
  marks.reserve(32);
  recent.reserve(kRecentMax);
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    if (end - at >= 3 && text[at + 1] == ' ') {
      const std::string rest = text.substr(at + 2, end - at - 2);
      if (text[at] == 'L') {
        spanish = rest == "es" ? 1 : 0;
      } else if (text[at] == 'R' && recent.size() < kRecentMax) {
        recent.push_back(rest);
      } else if (text[at] == 'M') {
        const size_t space = rest.find(' ');
        if (space != std::string::npos && space + 1 < rest.size()) {
          const int value = rest[space + 1] - '0';
          if (value > 0 && value < static_cast<int>(Mark::Count)) {
            marks.push_back({rest.substr(0, space), static_cast<Mark>(value)});
          }
        }
      }
    }
    at = end + 1;
  }
}

}  // namespace linernotes

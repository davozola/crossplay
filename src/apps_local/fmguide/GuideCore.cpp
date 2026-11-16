#include "GuideCore.h"

#include <cstring>

namespace fmguide {

namespace {

constexpr const char* kControlLabels[kControls] = {
    "MASTER", "SELECT", "PRESET", "ALGOR", "OCT-", "OCT+", "K1",  "K2",  "K3",   "K4",  "FX",       "SEL",
    "ENV",    "LFO",    "EDIT",   "GLO",   "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "PANTALLA", "TECLAS",
};

bool labelChar(const char c) {
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '+' || c == '/' || c == '.' ||
         c == '&' || c == '#' || c == ',';
}

}  // namespace

const char* controlLabel(const int bit) { return bit >= 0 && bit < kControls ? kControlLabels[bit] : ""; }

void pack(const Place* places, const int last, uint8_t* bytes) {
  bytes[0] = 1;
  bytes[1] = static_cast<uint8_t>(last >= 0 && last < kGuides ? last : 0xFF);
  for (int i = 0; i < kGuides; ++i) {
    bytes[2 + i * 2] = places[i].chapter;
    bytes[3 + i * 2] = places[i].page;
  }
}

bool unpack(const uint8_t* bytes, const int size, Place* places, int& last) {
  if (size != kSaveBytes || bytes[0] != 1) return false;
  for (int i = 0; i < kGuides; ++i) {
    // A chapter the guide no longer has starts it again; the page is checked
    // by the reader, which is the one that knows how many there are.
    if (bytes[2 + i * 2] >= guideAt(i).count) return false;
  }
  for (int i = 0; i < kGuides; ++i) {
    places[i].chapter = bytes[2 + i * 2];
    places[i].page = bytes[3 + i * 2];
  }
  last = bytes[1] < kGuides ? bytes[1] : -1;
  return true;
}

const char* checkProse(const char* text, const int maxChars) {
  if (!text || !*text) return "empty";
  int n = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p, ++n) {
    if (*p < 0x20) return "a control character";
    if (*p < 0x80) continue;
    if ((*p == 0xC2 || *p == 0xC3) && (p[1] & 0xC0) == 0x80) {
      ++p;
      continue;
    }
    return "outside Latin-1";
  }
  return n > maxChars ? "too long" : nullptr;
}

const char* checkLabel(const char* text, const int maxChars) {
  if (!text || !*text) return "empty";
  int n = 0;
  for (const char* p = text; *p; ++p, ++n) {
    if (!labelChar(*p)) return "not a label character";
  }
  return n > maxChars ? "too long" : nullptr;
}

int rowsOf(const char* spec) {
  if (!spec || !*spec) return 0;
  int rows = 1;
  for (const char* p = spec; *p; ++p) rows += *p == '|' ? 1 : 0;
  return rows;
}

bool rowAt(const char* spec, const int row, char* out, const int cap) {
  if (!spec || row < 0 || cap < 1) return false;
  const char* p = spec;
  for (int i = 0; i < row; ++i) {
    p = std::strchr(p, '|');
    if (!p) return false;
    ++p;
  }
  int n = 0;
  while (*p && *p != '|' && n + 1 < cap) out[n++] = *p++;
  out[n] = 0;
  return *p == 0 || *p == '|';
}

const char* checkFlow(const char* boxes) {
  const int rows = rowsOf(boxes);
  if (rows < 1 || rows > 4) return "one to four rows";
  for (int r = 0; r < rows; ++r) {
    char row[96];
    if (!rowAt(boxes, r, row, sizeof(row))) return "a row too long";
    int inRow = 0;
    const char* start = row;
    for (const char* p = row;; ++p) {
      if (*p != '>' && *p != 0) continue;
      char box[24];
      const int len = static_cast<int>(p - start);
      if (len < 1 || len >= static_cast<int>(sizeof(box))) return "an empty or long box";
      std::memcpy(box, start, static_cast<size_t>(len));
      box[len] = 0;
      if (checkLabel(box, 12)) return "a box that is not a short label";
      ++inRow;
      if (*p == 0) break;
      start = p + 1;
    }
    if (inRow > 5) return "more than five boxes in a row";
  }
  return nullptr;
}

const char* checkGrid(const char* rows) {
  const int count = rowsOf(rows);
  if (count < 1 || count > 8) return "one to eight rows";
  for (int r = 0; r < count; ++r) {
    char row[40];
    if (!rowAt(rows, r, row, sizeof(row))) return "a row too long";
    const char* colon = std::strchr(row, ':');
    if (!colon || colon == row || colon - row > 5) return "a name of one to five characters, then a colon";
    for (const char* p = row; p < colon; ++p) {
      if (!labelChar(*p)) return "a name that is not a label";
    }
    if (std::strlen(colon + 1) != 16) return "sixteen steps a row";
    for (const char* p = colon + 1; *p; ++p) {
      if (*p != 'x' && *p != 'X' && *p != '-') return "steps are x, X or -";
    }
  }
  return nullptr;
}

const char* checkBlock(const Block& block) {
  switch (block.kind) {
    case Kind::Head:
      return checkProse(block.a, 40);
    case Kind::Para:
      return checkProse(block.a, 620);
    case Kind::Bullet:
      return checkProse(block.a, 330);
    case Kind::Step:
      if (checkLabel(block.a, 2)) return "a step number of one or two digits";
      return checkProse(block.b, 360);
    case Kind::Note:
      if (checkLabel(block.a, 22)) return "a note label";
      return checkProse(block.b, 420);
    case Kind::Row:
      if (const char* why = checkProse(block.a, 34)) return why;
      return checkProse(block.b, 260);
    case Kind::Panel:
      if (block.mask == 0 || (block.mask & ~static_cast<uint32_t>(cAll)) != 0) return "a panel lights known controls";
      return checkProse(block.a, 240);
    case Kind::Keys:
      if (block.mask == 0 || (block.mask & ~(kAllWhite | kAllBlack)) != 0) return "a keyboard lights known keys";
      return checkProse(block.a, 240);
    case Kind::Flow:
      if (const char* why = checkFlow(block.a)) return why;
      return checkProse(block.b, 240);
    case Kind::Grid:
      if (const char* why = checkGrid(block.a)) return why;
      return checkProse(block.b, 240);
  }
  return "an unknown kind";
}

}  // namespace fmguide

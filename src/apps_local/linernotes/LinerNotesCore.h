#pragma once

// LINER NOTES without a screen: the pack's index, one album's record laid out
// as lines, and what the reader has marked. Freestanding, so the host tests
// read a real pack on a laptop.
//
// The pack is a folder on the card, made by tools_local/linernotes/ln_pack.py
// and never part of the firmware: index.tsv, es.txt, en.txt.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace linernotes {

constexpr int kCategories = 7;
extern const char* const kCategoryNames[kCategories];
constexpr uint8_t kHiddenBits = (1u << 3) | (1u << 6);  // hidden gem, collector's discovery
constexpr uint8_t kEssentialBit = 1u << 0;

struct Album {
  uint32_t id = 0;  // offsets into Index::pool, each a C string
  uint32_t artist = 0;
  uint32_t title = 0;
  uint32_t label = 0;
  uint32_t plexKey = 0;
  uint32_t offset[2] = {};  // es, en
  uint32_t length[2] = {};
  uint16_t year = 0;
  uint8_t categories = 0;
};

struct Index {
  std::string pool;
  std::vector<Album> albums;

  // Takes index.tsv whole. False when it is not a version 1 pack.
  bool parse(std::string&& text);
  const char* str(const uint32_t offset) const { return pool.c_str() + offset; }
  int find(const char* id) const;
};

// One album's record as lines ready to draw. Related albums come out apart,
// because they are buttons and not prose.
enum class Style : uint8_t { Heading, Body, Gap };
struct Line {
  uint32_t start = 0;
  uint16_t length = 0;
  Style style = Style::Body;
};
struct Related {
  uint16_t album = 0;
  std::string note;
};
struct Document {
  std::string text;
  std::vector<Line> lines;
  std::vector<Related> related;
};
// The width of `length` bytes of `text`, in the heading face when `heading`.
using Measure = int (*)(void* context, const char* text, size_t length, bool heading);
void layout(const char* record, size_t length, bool spanish, int width, Measure measure, void* context, Document& out);

// --- What the reader keeps ----------------------------------------------------

enum class Mark : uint8_t { None, Want, Listening, Rediscovered, Favourite, Count };
extern const char* const kMarkNames[static_cast<int>(Mark::Count)];
constexpr size_t kRecentMax = 24;

struct Kept {
  struct Marked {
    std::string id;
    Mark mark = Mark::None;
  };
  std::vector<Marked> marks;
  std::vector<std::string> recent;  // newest first
  int8_t spanish = -1;              // -1 follows the device's language

  Mark markOf(const char* id) const;
  void setMark(const char* id, Mark mark);
  void opened(const char* id);
  std::string pack() const;
  void unpack(const std::string& text);
};

}  // namespace linernotes

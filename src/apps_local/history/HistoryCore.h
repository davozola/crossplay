#pragma once

// EFEMERIDES' model: what happened on a day of the year, read out of
// Wikipedia's "on this day" feed as it streams off the card, sorted into
// science, history and culture, and kept a day a file. Freestanding (no
// Arduino), so host-tests/history runs it against an answer in the feed's shape.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace history {

enum class Kind : uint8_t { Science = 0, History = 1, Culture = 2 };
constexpr int kKinds = 3;
const char* kindName(Kind kind);  // "CIENCIA"

struct Event {
  int16_t year = 0;  // negative before the common era
  Kind kind = Kind::History;
  bool selected = false;  // one of the feed's own picks
  std::string text;
};

// By the words in it. Spanish, as the feed is asked for.
Kind classify(std::string_view text);

// Reads the feed's JSON a piece at a time: every object one level inside the
// root's arrays is an event, with a "text" and a "year" of its own and pages
// nested under it that are skipped whole.
class Scanner {
 public:
  explicit Scanner(std::vector<Event>& out, int maxEvents = 240) : out_(out), max_(maxEvents) {}
  void feed(const char* data, size_t length);
  // Events read from an array named "selected" are marked as such.
  int seen() const { return seen_; }

 private:
  void endString();
  void endNumber();
  void endItem();

  std::vector<Event>& out_;
  int max_;
  int seen_ = 0;
  int depth_ = 0;
  bool inString_ = false;
  bool escaped_ = false;
  int unicodeLeft_ = 0;
  uint32_t unicode_ = 0;
  uint32_t highSurrogate_ = 0;
  bool afterColon_ = false;  // the next token is a value
  bool inNumber_ = false;
  std::string token_;    // the string or number being read, when it is one that matters
  std::string key_;      // the key at item level
  std::string section_;  // the root key whose array this is
  bool keep_ = false;    // token_ is being filled
  std::string text_;
  int year_ = 0;
  bool hasYear_ = false;
};

// Newest first; the feed's own picks ahead of the rest within a year.
void sortEvents(std::vector<Event>& events);
// Drops an event whose year and opening words another already has.
void dropRepeats(std::vector<Event>& events);

// A day's events as text: a line each, kind, picked, year and text between tabs.
std::string pack(const std::vector<Event>& events);
void unpack(std::string_view text, std::vector<Event>& events);

// The feed for a day. `type` is "selected" or "events".
void feedUrl(const char* type, int month, int day, char* out, size_t cap);
// "17 DE NOVIEMBRE"
void dayLabel(int month, int day, char* out, size_t cap);
int daysInMonth(int month);  // February has 29: the feed has that day too
// The day before or after, wrapping round the year.
void stepDay(int& month, int& day, int by);
// "1969" / "44 A. C."
void yearLabel(int year, char* out, size_t cap);
// The one to lead with for a kind on a given day: a pick of the feed's when
// there is one, and otherwise chosen by the day. -1 when there is none.
int featured(const std::vector<Event>& events, Kind kind, int month, int day);
int countOf(const std::vector<Event>& events, Kind kind);

}  // namespace history

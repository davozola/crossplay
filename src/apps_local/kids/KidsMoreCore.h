#pragma once

// ILYA's four bigger games, the part with no panel in it: MEMORIA (pairs face
// down), SUMES (counting, adding, taking away, comparing, what is missing),
// LABERINT (a maze made fresh each time) and PUNTS (join the dots). Each has a
// page of choices a grown-up sets, so the same game suits a three-year-old and
// a six-year-old. Freestanding, so host-tests/kids can play them.

#include <cstdint>

#include "KidsCore.h"

namespace kidsmore {

// A choice on a setup page: its name and what each value is called. Catalan,
// capitals, ASCII.
struct Option {
  const char* name;
  const char* const* values;
  uint8_t count;
};
constexpr int kMaxOptions = 5;

// --- MEMORIA -------------------------------------------------------------------

namespace memory {

constexpr int kMaxCards = 24;
enum OptionIndex { OptCards, OptTheme, OptPlayers, OptPeek, OptCount };
extern const Option kOptions[OptCount];

enum class Theme : uint8_t { All, Animals, Food, Wheels, Nature, Things, Letters, Numbers, Words };
// What a card shows. A pair shares `pair`; the two faces may differ (a
// picture and its word, a number and that many dots).
enum class Face : uint8_t { Picture, Letter, Number, Dots, Word };
struct Card {
  uint8_t pair = 0;
  Face face = Face::Picture;
  uint8_t value = 0;  // a picture, a letter's index, or a number
  bool up = false;
  bool found = false;
};

int cardCount(const uint8_t* options);
int columns(int cards);
int rows(int cards);

struct Game {
  Card cards[kMaxCards];
  int count = 0;
  int8_t first = -1;   // face up, waiting for its partner
  int8_t second = -1;  // with `first`: a wrong pair left up until the next tap
  int moves = 0;
  int found = 0;
  int players = 1;
  int turn = 0;
  int score[2] = {0, 0};
  bool peeking = false;  // everything face up before the first tap

  bool done() const { return found * 2 == count; }
};

void deal(Game& game, const uint8_t* options, kids::Rng& rng);
enum class Tap : uint8_t { Nothing, Turned, Pair, Miss, Hidden, Done };
// A finger on card `index` (-1 for anywhere else).
Tap tap(Game& game, int index);

}  // namespace memory

// --- SUMES ---------------------------------------------------------------------

namespace sums {

enum OptionIndex { OptKind, OptLimit, OptHelp, OptCount };
extern const Option kOptions[OptCount];

enum class Kind : uint8_t { Count, Add, Take, More, Missing };
constexpr int kKinds = 5;
constexpr int kSequence = 5;

struct Round {
  Kind kind = Kind::Count;
  int a = 0;           // how many, or the left group
  int b = 0;           // added, taken away, or the right group
  bool fewer = false;  // More: the question is which has fewer
  int sequence[kSequence] = {};
  int hole = 0;  // Missing: which of the sequence is the blank
  int picture = 0;
  int options[4] = {};
  int optionCount = 0;
  int answer = 0;  // an index into options
  uint8_t wrongMask = 0;
  bool solved = false;

  int result() const { return options[answer]; }
  bool choose(int option);
};

int limitOf(const uint8_t* options);  // 5, 10 or 20
// The next question, never the same sum as `last`.
Round deal(const uint8_t* options, kids::Rng& rng, const Round& last);

}  // namespace sums

// --- LABERINT ------------------------------------------------------------------

namespace maze {

enum OptionIndex { OptSize, OptWho, OptTrail, OptStars, OptCount };
extern const Option kOptions[OptCount];

constexpr int kMaxW = 11;
constexpr int kMaxH = 15;
constexpr int kMaxCells = kMaxW * kMaxH;
constexpr int kMaxStars = 3;
enum Dir : uint8_t { North = 0, East = 1, South = 2, West = 3 };

// Who walks and what they walk to: two pictures.
struct Pair {
  uint8_t walker;
  uint8_t goal;
};
constexpr int kPairs = 5;
Pair pairOf(const uint8_t* options, kids::Rng& rng);

struct Maze {
  int w = 0;
  int h = 0;
  uint8_t open[kMaxCells] = {};  // bit d set: no wall on side d
  int at = 0;                    // where the walker is
  int goal = 0;
  bool seen[kMaxCells] = {};
  int stars[kMaxStars] = {-1, -1, -1};  // cells holding a star, -1 once taken or unused
  int starCount = 0;
  int taken = 0;
  int steps = 0;

  bool done() const { return at == goal; }
  bool canGo(int cell, Dir dir) const;
};

void sizeOf(const uint8_t* options, int& w, int& h);
void build(Maze& maze, const uint8_t* options, kids::Rng& rng);
// A finger on `cell`: the walker goes that way along its corridor as far as
// the cell tapped, or the wall before it. Returns the steps taken.
int walkTowards(Maze& maze, int cell);
// The next `max` cells of the way to the goal, in order. Returns how many.
int hint(const Maze& maze, int* cells, int max);

}  // namespace maze

// --- PUNTS ---------------------------------------------------------------------

namespace dots {

enum OptionIndex { OptLevel, OptLabels, OptOrder, OptHelp, OptCount };
extern const Option kOptions[OptCount];

enum class Labels : uint8_t { Numbers, Letters, Twos, None };

struct Game {
  int figure = 0;
  int level = 0;
  int count = 0;   // dots in this figure
  int joined = 0;  // dots reached so far; the next to tap is `joined`
  bool done() const { return count > 0 && joined >= count; }
};

// The next figure: in order after `previous`, or any other one.
void start(Game& game, const uint8_t* options, kids::Rng& rng, int previous, int figures, const uint8_t* dotCounts);
// A finger on dot `index`. True when it was the next one.
bool tap(Game& game, int index);
// What is written beside dot `index`: "7", "G" or "14". Empty for no labels.
void label(const uint8_t* options, int index, int count, char* out, int cap);

}  // namespace dots

// The choices of one game as bytes for the card, and back: `count` values,
// each kept only when it is one the option still has.
void packOptions(const uint8_t* values, int count, uint8_t* bytes);
void unpackOptions(const uint8_t* bytes, int size, const Option* options, int count, uint8_t* values);

}  // namespace kidsmore

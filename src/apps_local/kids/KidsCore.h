#pragma once

// ILYA: five small games for a three-year-old. Freestanding: the word list, the
// alphabet that has pictures, and how a round is dealt, so host-tests/kids can
// check that no round is ever unanswerable.
//
// Words are Catalan, chosen to need no accent: the panel's faces are ASCII, and
// a word spelt wrong is the wrong thing to show someone learning letters.

#include <cstdint>

namespace kids {

constexpr int kPictures = 45;
const char* wordOf(int picture);

enum class Game : uint8_t { Cards = 0, Find = 1, Match = 2, Letters = 3, Count = 4 };
constexpr int kGames = 5;
const char* gameName(Game game);
// The picture that stands for a game on the menu.
int gamePicture(Game game);

struct Rng {
  uint32_t state = 0x1234567u;
  uint32_t next();
  int below(int n);  // 0..n-1
};

constexpr int kMaxOptions = 4;
constexpr int kMaxCount = 6;

// One question. For Find and Match the options are pictures; for Count they
// are numbers and `target` is the picture being counted.
struct Round {
  int target = 0;
  int options[kMaxOptions] = {};
  int optionCount = 0;
  int answer = 0;         // index into options
  int count = 0;          // Count only: how many are drawn
  uint8_t wrongMask = 0;  // options already tried and wrong
  bool solved = false;

  // True when `option` is the answer. A wrong one is remembered and cannot be
  // chosen again; nothing else is lost.
  bool choose(int option);
};

// `optionCount` distinct pictures, one of them the target, which is never
// `avoid` (the last round's), so the same question does not come twice running.
Round dealPictures(Rng& rng, int optionCount, int avoid);
// A picture drawn 1..kMaxCount times and three ascending numbers to pick from.
Round dealCount(Rng& rng, int avoidCount);

// The letters that have a picture, in alphabetical order.
int letterCount();
char letterAt(int index);
int pictureOfLetter(int index);

// Steps through a ring of `size`.
int step(int current, int direction, int size);

}  // namespace kids

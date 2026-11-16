#pragma once

// MISTERIOS's model: short detective cases to be worked by asking and looking.
// A case is four suspects with three questions each and three places; some
// questions only come up once a clue has been found. An accusation holds when
// it names the right person AND the clues for motive, opportunity and a broken
// alibi are all in the notebook. Freestanding; the cases are in
// MysteryCases.cpp, in Spanish, set in the reading serif.

#include <cstddef>
#include <cstdint>

namespace mystery {

constexpr int kCases = 6;
constexpr int kSuspects = 4;
constexpr int kQuestions = 3;
constexpr int kPlaces = 3;
constexpr int kMaxClues = 6;

struct Question {
  const char* ask;
  const char* answer;
  int8_t needs;  // the clue that brings this question up, or -1
  int8_t gives;  // the clue the answer puts in the notebook, or -1
};

struct Suspect {
  const char* name;  // capitals, ASCII: it goes on a button
  const char* role;  // the same
  const char* about;
  const char* alibi;  // why it was not them, shown on a wrong accusation
  Question questions[kQuestions];
};

struct Place {
  const char* name;  // capitals, ASCII
  const char* text;
  int8_t gives;
};

struct Case {
  const char* title;  // capitals, ASCII
  const char* intro;
  Suspect suspects[kSuspects];
  Place places[kPlaces];
  const char* clues[kMaxClues];
  uint8_t clueCount;
  uint8_t culprit;
  uint8_t motive, opportunity, alibi;  // the three clues an accusation rests on
  const char* solution;
};

const Case& caseAt(int index);

struct Progress {
  uint8_t clues = 0;     // one bit a clue
  uint16_t asked = 0;    // bit suspect * 3 + question
  uint8_t examined = 0;  // one bit a place
  uint8_t steps = 0;     // questions asked and places examined
  uint8_t wrong = 0;     // accusations that did not hold
  uint8_t best = 0;      // the best rank reached, 0 for unsolved
  bool solved = false;
};

bool hasClue(const Progress& p, int clue);
int clueCount(const Progress& p);
bool wasAsked(const Progress& p, int suspect, int question);
bool canAsk(const Case& c, const Progress& p, int suspect, int question);
// How many of a suspect's questions are still hidden.
int hiddenQuestions(const Case& c, const Progress& p, int suspect);
// Asks, and returns the clue this put in the notebook for the first time, or -1.
int ask(const Case& c, Progress& p, int suspect, int question);
int examine(const Case& c, Progress& p, int place);

enum class Verdict : uint8_t { Solved, WrongSuspect, NoMotive, NoOpportunity, NoAlibi };
Verdict accuse(const Case& c, Progress& p, int suspect);

// 1 the best of four. Fewer steps and no wrong accusations rank higher.
int rankOf(const Progress& p);
const char* rankName(int rank);  // capitals, ASCII

// Starts a case again, keeping the best rank.
void restart(Progress& p);

constexpr size_t kSaveBytes = 1 + kCases * 8;
size_t save(const Progress* all, uint8_t* out, size_t cap);
// A save from a build with fewer cases restores those and leaves the rest new.
bool restore(Progress* all, const uint8_t* in, size_t size);

}  // namespace mystery

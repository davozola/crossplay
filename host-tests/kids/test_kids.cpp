// ILYA host tests: the word list, the alphabet, and that every round dealt can
// be answered.

#include <cstdio>
#include <cstring>

#include "KidsCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
}  // namespace

int main() {
  using namespace kids;

  for (int i = 0; i < kPictures; ++i) {
    const char* w = wordOf(i);
    check(w[0] != 0, "every picture has a word");
    for (const char* c = w; *c; ++c) check(*c >= 'A' && *c <= 'Z', "words are plain capitals");
    for (int k = 0; k < i; ++k) check(std::strcmp(w, wordOf(k)) != 0, "no word twice");
  }
  check(wordOf(-1)[0] == 0 && wordOf(kPictures)[0] == 0, "out of range is empty");

  for (int i = 0; i < letterCount(); ++i) {
    check(wordOf(pictureOfLetter(i))[0] == letterAt(i), "a letter's word starts with it");
    if (i > 0) check(letterAt(i) > letterAt(i - 1), "letters ascend");
  }
  for (int g = 0; g < kGames; ++g) {
    check(gameName(static_cast<Game>(g))[0] != 0, "every game is named");
    check(gamePicture(static_cast<Game>(g)) < kPictures, "every game has a picture");
  }

  Rng rng;
  rng.state = 42;
  int last = -1;
  for (int n = 0; n < 5000; ++n) {
    const int want = 2 + n % 3;
    Round r = dealPictures(rng, want, last);
    check(r.optionCount == want, "option count honoured");
    check(r.target != last, "target is not the one before");
    check(r.options[r.answer] == r.target, "the answer is among the options");
    for (int a = 0; a < r.optionCount; ++a) {
      check(r.options[a] >= 0 && r.options[a] < kPictures, "option in range");
      for (int b = 0; b < a; ++b) check(r.options[a] != r.options[b], "options distinct");
    }
    last = r.target;
  }
  int lastCount = 0;
  bool seen[kMaxCount + 1] = {};
  for (int n = 0; n < 5000; ++n) {
    Round r = dealCount(rng, lastCount);
    check(r.count >= 1 && r.count <= kMaxCount && r.count != lastCount, "count in range and fresh");
    check(r.optionCount == 3 && r.options[r.answer] == r.count, "the count is among the numbers");
    check(r.options[0] >= 1 && r.options[2] <= kMaxCount, "numbers stay in range");
    check(r.options[1] == r.options[0] + 1 && r.options[2] == r.options[1] + 1, "numbers ascend");
    seen[r.count] = true;
    lastCount = r.count;
  }
  for (int c = 1; c <= kMaxCount; ++c) check(seen[c], "every count comes up");

  {
    Round r = dealPictures(rng, 4, -1);
    const int wrong = (r.answer + 1) % 4;
    check(!r.choose(wrong) && (r.wrongMask & (1u << wrong)) && !r.solved, "a wrong pick is remembered");
    check(!r.choose(9), "an option that is not there is not chosen");
    check(r.choose(r.answer) && r.solved, "the right pick solves it");
    check(!r.choose(r.answer), "a solved round takes no more picks");
  }

  check(step(0, -1, 5) == 4 && step(4, 1, 5) == 0 && step(2, 1, 5) == 3 && step(0, 1, 0) == 0, "ring steps");
  Rng zero;
  zero.state = 0;
  check(zero.next() != 0, "a zero seed does not stick");

  if (failures == 0) std::printf("kids: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

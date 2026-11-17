#include <cstdio>
#include <cstring>

#include "LearnCore.h"

using namespace learn;

namespace {
int failures = 0;
int checks = 0;
void check(const bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL: %s\n", what);
  }
}

// Latin-1 as UTF-8 and nothing else: what the reading serif can set.
bool settable(const char* text) {
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0; ++p) {
    if (*p < 0x80) {
      if (*p < 0x20 && *p != '\n') return false;
      continue;
    }
    if ((*p != 0xC2 && *p != 0xC3) || (p[1] & 0xC0) != 0x80) return false;
    ++p;
  }
  return true;
}

int words(const char* text) {
  int n = 0;
  bool in = false;
  for (const char* p = text; *p != 0; ++p) {
    const bool space = *p == ' ' || *p == '\n';
    if (!space && !in) ++n;
    in = !space;
  }
  return n;
}
}  // namespace

int main() {
  {  // every lesson
    check(lessonCount() == 40 && lessonCount() <= kMaxLessons, "forty lessons");
    int total = 0;
    for (int i = 0; i < lessonCount(); ++i) {
      const Lesson& l = lessonAt(i);
      const char* const texts[5] = {l.title, l.idea, l.example, l.question, l.why};
      for (const char* t : texts) {
        check(t != nullptr && *t != 0 && settable(t), l.title);
      }
      check(std::strlen(l.title) <= 44, l.title);
      check(l.answer < 3, l.title);
      for (const char* o : l.options) check(o != nullptr && *o != 0 && settable(o) && std::strlen(o) <= 64, l.title);
      check(std::strcmp(l.options[0], l.options[1]) != 0 && std::strcmp(l.options[1], l.options[2]) != 0, l.title);
      const int n = words(l.idea) + words(l.example);
      check(n >= 70 && n <= 190, l.title);
      check(words(l.question) <= 40 && words(l.why) <= 45, l.title);
      total += n;
      for (int k = i + 1; k < lessonCount(); ++k) check(std::strcmp(l.title, lessonAt(k).title) != 0, "titles differ");
    }
    for (int s = 0; s < kSubjects; ++s) check(countIn(static_cast<Subject>(s)) == 10, "ten a subject");
    check(lessonOf(Subject::Economics, 0) == 10 && lessonOf(Subject::Ai, 9) == 39 && lessonOf(Subject::Ai, 10) == -1,
          "the nth of a subject");
    check(std::strcmp(subjectName(Subject::Electronics), "ELECTRONICA") == 0, "names");
    std::printf("lessons: %d, %d words of idea and example\n", lessonCount(), total);
  }

  {  // one a day
    Progress p;
    check(lessonFor(p, 20000) == lessonOf(static_cast<Subject>(20000 % 4), 0), "the day's subject, its first lesson");
    check(lessonFor(p, 20001) != lessonFor(p, 20000), "another subject the next day");
    const int first = lessonFor(p, 20000);
    finish(p, first, true, 20000);
    check(isDone(p, first) && wasRight(p, first) && p.streak == 1 && doneToday(p, 20000), "done, and right");
    check(lessonFor(p, 20000) == first + 1, "then the next of that subject");
    finish(p, first + 1, false, 20000);
    check(p.streak == 1 && !wasRight(p, first + 1), "a second the same day does not move the streak");
    finish(p, first + 1, true, 20000);
    check(!wasRight(p, first + 1), "the first answer is the one remembered");
    finish(p, lessonFor(p, 20001), true, 20001);
    check(p.streak == 2 && streakOn(p, 20001) == 2 && streakOn(p, 20002) == 2 && streakOn(p, 20003) == 0,
          "two days running, and gone after a day missed");
    finish(p, lessonFor(p, 20005), true, 20005);
    check(p.streak == 1 && p.bestStreak == 2, "a gap starts again");
    const uint16_t before = p.streak;
    finish(p, lessonFor(p, 0), true, 0);
    check(p.streak == before, "no clock, no change to the streak");
    check(doneCount(p) == 5 && doneIn(p, static_cast<Subject>(20000 % 4)) >= 2, "counts");

    uint8_t bytes[kProgressBytes];
    check(packProgress(p, bytes, sizeof(bytes)) == kProgressBytes && packProgress(p, bytes, 3) == 0, "pack size");
    Progress back;
    check(unpackProgress(bytes, sizeof(bytes), back) && doneCount(back) == 5 && back.lastDay == 20005 &&
              back.bestStreak == 2 && wasRight(back, first),
          "round trip");
    bytes[1] = 7;
    check(!unpackProgress(bytes, sizeof(bytes), back), "another version is not read");

    // A subject finished hands its days to the others; everything finished is -1.
    Progress all;
    for (int i = 0; i < 10; ++i) finish(all, lessonOf(Subject::Maths, i), true, 0);
    check(lessonAt(lessonFor(all, 0)).subject == Subject::Economics, "a finished subject passes its turn");
    for (int i = 0; i < lessonCount(); ++i) finish(all, i, true, 0);
    check(lessonFor(all, 123) == -1, "nothing left");
  }

  if (failures == 0) {
    std::printf("learn: all tests passed (%d checks)\n", checks);
    return 0;
  }
  return 1;
}

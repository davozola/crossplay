#pragma once

// APRENDE's model: short lessons in four subjects, one a day, each ending in a
// question; which have been done and how many days in a row. Freestanding (no
// Arduino), so host-tests/learn reads every lesson.

#include <cstddef>
#include <cstdint>

namespace learn {

enum class Subject : uint8_t { Maths = 0, Economics = 1, Electronics = 2, Ai = 3 };
constexpr int kSubjects = 4;
const char* subjectName(Subject subject);  // "MATEMATICAS"

struct Lesson {
  Subject subject;
  const char* title;
  const char* idea;     // paragraphs separated by '\n'
  const char* example;  // a worked case
  const char* question;
  const char* options[3];
  uint8_t answer;   // which option is right
  const char* why;  // read after answering
};

int lessonCount();
const Lesson& lessonAt(int index);
int countIn(Subject subject);
// The nth lesson of a subject, as an index for lessonAt; -1 past the end.
int lessonOf(Subject subject, int nth);

constexpr int kMaxLessons = 96;

struct Progress {
  uint8_t done[kMaxLessons / 8] = {};
  uint8_t right[kMaxLessons / 8] = {};  // answered right at the first go
  int32_t lastDay = 0;                  // days since 1970 of the last lesson finished
  uint16_t streak = 0;                  // days in a row
  uint16_t bestStreak = 0;
};
bool isDone(const Progress& progress, int lesson);
bool wasRight(const Progress& progress, int lesson);
int doneIn(const Progress& progress, Subject subject);
int doneCount(const Progress& progress);
// Marks a lesson finished on `day` (0 when the clock is not set, which keeps
// the streak as it is). The first one of a day moves the streak.
void finish(Progress& progress, int lesson, bool right, int32_t day);
// The streak as it stands on `day`: zero once a day has been missed.
int streakOn(const Progress& progress, int32_t day);
bool doneToday(const Progress& progress, int32_t day);

// The lesson for a day: the subjects take turns, and within the day's subject
// it is the first not yet done; when that subject is finished, the first
// undone of any. -1 when there is nothing left.
int lessonFor(const Progress& progress, int32_t day);

constexpr size_t kProgressBytes = 2 + 2 * (kMaxLessons / 8) + 4 + 2 + 2;
size_t packProgress(const Progress& progress, uint8_t* out, size_t cap);
bool unpackProgress(const uint8_t* in, size_t size, Progress& out);

}  // namespace learn

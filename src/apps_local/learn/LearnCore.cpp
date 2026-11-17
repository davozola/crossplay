#include "LearnCore.h"

#include <cstring>

#include "LearnData.h"

namespace learn {

const char* subjectName(const Subject subject) {
  static constexpr const char* kNames[kSubjects] = {"MATEMATICAS", "ECONOMIA", "ELECTRONICA", "IA"};
  const int i = static_cast<int>(subject);
  return kNames[i >= 0 && i < kSubjects ? i : 0];
}

int lessonCount() { return kLessonCount; }

const Lesson& lessonAt(const int index) { return kLessons[index >= 0 && index < kLessonCount ? index : 0]; }

int countIn(const Subject subject) {
  int n = 0;
  for (int i = 0; i < kLessonCount; ++i) n += kLessons[i].subject == subject ? 1 : 0;
  return n;
}

int lessonOf(const Subject subject, const int nth) {
  int seen = 0;
  for (int i = 0; i < kLessonCount; ++i) {
    if (kLessons[i].subject != subject) continue;
    if (seen == nth) return i;
    ++seen;
  }
  return -1;
}

bool isDone(const Progress& p, const int lesson) {
  return lesson >= 0 && lesson < kMaxLessons && (p.done[lesson / 8] & (1u << (lesson % 8))) != 0;
}

bool wasRight(const Progress& p, const int lesson) {
  return lesson >= 0 && lesson < kMaxLessons && (p.right[lesson / 8] & (1u << (lesson % 8))) != 0;
}

int doneIn(const Progress& p, const Subject subject) {
  int n = 0;
  for (int i = 0; i < kLessonCount; ++i) n += kLessons[i].subject == subject && isDone(p, i) ? 1 : 0;
  return n;
}

int doneCount(const Progress& p) {
  int n = 0;
  for (int i = 0; i < kLessonCount; ++i) n += isDone(p, i) ? 1 : 0;
  return n;
}

void finish(Progress& p, const int lesson, const bool right, const int32_t day) {
  if (lesson < 0 || lesson >= kLessonCount || lesson >= kMaxLessons) return;
  // The first answer is the one remembered.
  if (!isDone(p, lesson) && right) p.right[lesson / 8] |= static_cast<uint8_t>(1u << (lesson % 8));
  p.done[lesson / 8] |= static_cast<uint8_t>(1u << (lesson % 8));
  if (day <= 0 || day == p.lastDay) return;
  p.streak = static_cast<uint16_t>(p.lastDay == day - 1 ? p.streak + 1 : 1);
  if (p.streak > p.bestStreak) p.bestStreak = p.streak;
  p.lastDay = day;
}

int streakOn(const Progress& p, const int32_t day) {
  if (day <= 0) return p.streak;
  return p.lastDay == day || p.lastDay == day - 1 ? p.streak : 0;
}

bool doneToday(const Progress& p, const int32_t day) { return day > 0 && p.lastDay == day; }

int lessonFor(const Progress& p, const int32_t day) {
  const int turn = static_cast<int>(((day % kSubjects) + kSubjects) % kSubjects);
  for (int k = 0; k < kSubjects; ++k) {
    const Subject subject = static_cast<Subject>((turn + k) % kSubjects);
    for (int i = 0; i < kLessonCount; ++i) {
      if (kLessons[i].subject == subject && !isDone(p, i)) return i;
    }
  }
  return -1;
}

size_t packProgress(const Progress& p, uint8_t* out, const size_t cap) {
  if (cap < kProgressBytes) return 0;
  uint8_t* o = out;
  *o++ = 'L';
  *o++ = 1;
  std::memcpy(o, p.done, sizeof(p.done));
  o += sizeof(p.done);
  std::memcpy(o, p.right, sizeof(p.right));
  o += sizeof(p.right);
  for (int i = 0; i < 4; ++i) *o++ = static_cast<uint8_t>(static_cast<uint32_t>(p.lastDay) >> (8 * i));
  *o++ = static_cast<uint8_t>(p.streak & 0xFF);
  *o++ = static_cast<uint8_t>(p.streak >> 8);
  *o++ = static_cast<uint8_t>(p.bestStreak & 0xFF);
  *o++ = static_cast<uint8_t>(p.bestStreak >> 8);
  return static_cast<size_t>(o - out);
}

bool unpackProgress(const uint8_t* in, const size_t size, Progress& out) {
  if (size != kProgressBytes || in[0] != 'L' || in[1] != 1) return false;
  Progress p;
  const uint8_t* i = in + 2;
  std::memcpy(p.done, i, sizeof(p.done));
  i += sizeof(p.done);
  std::memcpy(p.right, i, sizeof(p.right));
  i += sizeof(p.right);
  uint32_t day = 0;
  for (int k = 0; k < 4; ++k) day |= static_cast<uint32_t>(*i++) << (8 * k);
  p.lastDay = static_cast<int32_t>(day);
  p.streak = static_cast<uint16_t>(i[0] | (i[1] << 8));
  p.bestStreak = static_cast<uint16_t>(i[2] | (i[3] << 8));
  if (p.bestStreak < p.streak) return false;
  out = p;
  return true;
}

}  // namespace learn

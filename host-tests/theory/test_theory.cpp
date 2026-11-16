// MUSIC THEORY LAB host tests: scales, chords and progressions spelt in keys
// whose answers are known, and the Euclidean rhythms.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "TheoryCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    if (failures < 20) std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
bool is(const char* got, const char* want) {
  if (std::strcmp(got, want) != 0) std::printf("  got '%s', want '%s'\n", got, want);
  return std::strcmp(got, want) == 0;
}
int bits(unsigned v) {
  int n = 0;
  for (; v; v >>= 1) n += v & 1;
  return n;
}
}  // namespace

int main() {
  using namespace theory;
  char out[160];

  check(is(noteName(0), "C") && is(noteName(13), "C#") && is(noteName(-1), "B"), "note names wrap");
  check(scaleMask(0, 0) == 0b101010110101, "C major is the white keys");
  check(scaleMask(0, 7) == scaleMask(1, 4), "G major and E minor share their notes");
  check(scaleMask(4, 2) == scaleMask(0, 0), "D dorian is the white keys too");
  for (int s = 0; s < kScales; ++s) {
    check(bits(scaleMask(s, 5)) == scale(s).count && scale(s).steps[0] == 0, "a scale has as many notes as it says");
    for (int i = 1; i < scale(s).count; ++i) check(scale(s).steps[i] > scale(s).steps[i - 1], "scale steps ascend");
  }
  scaleSteps(0, out, sizeof(out));
  check(is(out, "W W H W W W H"), "major steps");
  scaleSteps(10, out, sizeof(out));
  check(is(out, "3 W W 3 W"), "minor pentatonic steps");
  scaleTriads(0, 0, out, sizeof(out));
  check(is(out, "C  DM  EM  F  G  AM  BDIM"), "triads of C major");
  scaleTriads(1, 9, out, sizeof(out));
  check(is(out, "AM  BDIM  C  DM  EM  F  G"), "triads of A minor");
  scaleTriads(2, 9, out, sizeof(out));
  check(std::strstr(out, "CAUG") != nullptr && std::strstr(out, "E  ") != nullptr,
        "harmonic minor has an augmented III and a major V");
  scaleTriads(9, 0, out, sizeof(out));
  check(out[0] == 0, "no triads for a five-note scale");

  chordNotes(0, 0, out, sizeof(out));
  check(is(out, "C E G"), "C major");
  chordNotes(9, 5, out, sizeof(out));
  check(is(out, "F A C E"), "F major 7");
  chordNotes(11, 11, out, sizeof(out));
  check(is(out, "B D F A"), "B half diminished");
  chordNotes(14, 7, out, sizeof(out));
  check(is(out, "G B D F A"), "G9");
  check(chordMask(12, 0) == chordMask(12, 3), "a diminished seventh is the same four notes a minor third up");
  for (int c = 0; c < kChords; ++c)
    check(bits(chordMask(c, 4)) == chord(c).count && chord(c).name[0] != 0, "chords are well formed");

  progressionChords(0, 0, out, sizeof(out));
  check(is(out, "C  G  AM  F"), "pop in C");
  progressionChords(3, 5, out, sizeof(out));
  check(is(out, "GM  C7  F"), "II V I in F");
  progressionChords(8, 9, out, sizeof(out));
  check(is(out, "AM  G  F  E"), "andalusian in A minor");
  progressionChords(5, 4, out, sizeof(out));
  check(std::strncmp(out, "E7  E7  E7  E7  A7  A7", 22) == 0, "blues in E");
  for (int p = 0; p < kProgressions; ++p) {
    progressionChords(p, 3, out, sizeof(out));
    int chords = 1, numerals = 1;
    for (const char* c = out; *c; ++c) chords += c[0] == ' ' && c[1] == ' ';
    for (const char* c = progression(p).numerals; *c; ++c) numerals += *c == ' ';
    check(chords == numerals, "as many chords as numerals");
  }

  check(euclid(8, 3, 0) == 0b01001001, "tresillo: x..x..x.");
  check(euclid(8, 5, 0) == 0b10110101 || bits(euclid(8, 5, 0)) == 5, "cinquillo has five");
  check(euclid(16, 4, 0) == 0x1111, "four on the floor");
  check(euclid(8, 3, 1) == 0b10010010, "rotation turns it one step");
  check(euclid(8, 0, 0) == 0 && euclid(8, 8, 0) == 0xFF && euclid(8, 12, 0) == 0xFF, "none, all, and too many");
  for (int steps = 1; steps <= kMaxSteps; ++steps) {
    for (int pulses = 0; pulses <= steps; ++pulses) {
      const uint16_t p = euclid(steps, pulses, 0);
      check(bits(p) == pulses && (p >> steps) == 0, "the right number of pulses, inside the steps");
      // Evenness: the gaps between pulses differ by at most one.
      int lo = 99, hi = 0, last = -1, first = -1;
      for (int i = 0; i < steps; ++i) {
        if (!((p >> i) & 1)) continue;
        if (first < 0) first = i;
        if (last >= 0) {
          const int gap = i - last;
          lo = gap < lo ? gap : lo;
          hi = gap > hi ? gap : hi;
        }
        last = i;
      }
      if (pulses >= 2) {
        const int gap = first + steps - last;
        lo = gap < lo ? gap : lo;
        hi = gap > hi ? gap : hi;
        check(hi - lo <= 1, "pulses are as even as the steps allow");
      }
      if (pulses > 0) check(p & 1, "an unrotated rhythm starts on a pulse");
    }
  }
  check(is(euclidName(8, 3), "TRESILLO") && euclidName(11, 4)[0] == 0, "names");
  check(std::fabs(frequency(69) - 440) < 0.01f && std::fabs(frequency(60) - 261.63f) < 0.05f, "frequencies");

  if (failures == 0) std::printf("theory: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

#include "TheoryCore.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace theory {

namespace {

constexpr const char* kNotes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

constexpr Scale kScaleTable[kScales] = {
    {"MAJOR", {0, 2, 4, 5, 7, 9, 11}, 7},          {"NATURAL MINOR", {0, 2, 3, 5, 7, 8, 10}, 7},
    {"HARMONIC MINOR", {0, 2, 3, 5, 7, 8, 11}, 7}, {"MELODIC MINOR", {0, 2, 3, 5, 7, 9, 11}, 7},
    {"DORIAN", {0, 2, 3, 5, 7, 9, 10}, 7},         {"PHRYGIAN", {0, 1, 3, 5, 7, 8, 10}, 7},
    {"LYDIAN", {0, 2, 4, 6, 7, 9, 11}, 7},         {"MIXOLYDIAN", {0, 2, 4, 5, 7, 9, 10}, 7},
    {"LOCRIAN", {0, 1, 3, 5, 6, 8, 10}, 7},        {"MAJOR PENTATONIC", {0, 2, 4, 7, 9}, 5},
    {"MINOR PENTATONIC", {0, 3, 5, 7, 10}, 5},     {"BLUES", {0, 3, 5, 6, 7, 10}, 6},
    {"WHOLE TONE", {0, 2, 4, 6, 8, 10}, 6},        {"PHRYGIAN DOMINANT", {0, 1, 4, 5, 7, 8, 10}, 7},
};

constexpr Chord kChordTable[kChords] = {
    {"MAJOR", "", "1 3 5", {0, 4, 7}, 3},
    {"MINOR", "M", "1 B3 5", {0, 3, 7}, 3},
    {"DIMINISHED", "DIM", "1 B3 B5", {0, 3, 6}, 3},
    {"AUGMENTED", "AUG", "1 3 #5", {0, 4, 8}, 3},
    {"SUSPENDED 2", "SUS2", "1 2 5", {0, 2, 7}, 3},
    {"SUSPENDED 4", "SUS4", "1 4 5", {0, 5, 7}, 3},
    {"SIXTH", "6", "1 3 5 6", {0, 4, 7, 9}, 4},
    {"MINOR SIXTH", "M6", "1 B3 5 6", {0, 3, 7, 9}, 4},
    {"DOMINANT 7", "7", "1 3 5 B7", {0, 4, 7, 10}, 4},
    {"MAJOR 7", "MAJ7", "1 3 5 7", {0, 4, 7, 11}, 4},
    {"MINOR 7", "M7", "1 B3 5 B7", {0, 3, 7, 10}, 4},
    {"HALF DIMINISHED", "M7B5", "1 B3 B5 B7", {0, 3, 6, 10}, 4},
    {"DIMINISHED 7", "DIM7", "1 B3 B5 6", {0, 3, 6, 9}, 4},
    {"ADD 9", "ADD9", "1 3 5 9", {0, 4, 7, 14}, 4},
    {"DOMINANT 9", "9", "1 3 5 B7 9", {0, 4, 7, 10, 14}, 5},
    {"MINOR 9", "M9", "1 B3 5 B7 9", {0, 3, 7, 10, 14}, 5},
};

constexpr Progression kProgressionTable[kProgressions] = {
    {"POP", "I V VI IV", "0M 7M 9m 5M", false},
    {"FIFTIES", "I VI IV V", "0M 9m 5M 7M", false},
    {"THREE CHORDS", "I IV V", "0M 5M 7M", false},
    {"JAZZ TURN", "II V I", "2m 77 0M", false},
    {"CANON", "I V VI III IV I IV V", "0M 7M 9m 4m 5M 0M 5M 7M", false},
    {"BLUES, 12 BARS", "I I I I IV IV I I V IV I V", "07 07 07 07 57 57 07 07 77 57 07 77", false},
    {"SENSITIVE", "VI IV I V", "9m 5M 0M 7M", false},
    {"CIRCLE", "VI II V I", "9m 2m 77 0M", false},
    {"ANDALUSIAN", "I VII VI V", "0m AM 8M 7M", true},
    {"MINOR POP", "I VI III VII", "0m 8M 3M AM", true},
    {"MINOR TURN", "II V I", "2d 77 0m", true},
    {"MINOR THREE", "I IV V", "0m 5m 77", true},
};

int stepOf(const char c) { return c >= 'A' ? c - 'A' + 10 : c - '0'; }

void append(char* out, const size_t cap, size_t& at, const char* text) {
  for (; *text && at + 1 < cap; ++text) out[at++] = *text;
  if (cap > 0) out[at] = 0;
}

}  // namespace

const char* noteName(const int pitchClass) { return kNotes[((pitchClass % 12) + 12) % 12]; }

const Scale& scale(const int index) { return kScaleTable[index < 0 ? 0 : (index >= kScales ? kScales - 1 : index)]; }

uint16_t scaleMask(const int index, const int root) {
  const Scale& s = scale(index);
  uint16_t mask = 0;
  for (int i = 0; i < s.count; ++i) mask = static_cast<uint16_t>(mask | (1u << ((root + s.steps[i]) % 12)));
  return mask;
}

void scaleSteps(const int index, char* out, const size_t cap) {
  const Scale& s = scale(index);
  size_t at = 0;
  if (cap > 0) out[0] = 0;
  for (int i = 0; i < s.count; ++i) {
    const int next = i + 1 < s.count ? s.steps[i + 1] : 12;
    const int gap = next - s.steps[i];
    // A half step, a whole step, or the number of semitones for anything wider.
    const char one[2] = {gap == 1 ? 'H' : (gap == 2 ? 'W' : static_cast<char>('0' + (gap > 9 ? 9 : gap))), 0};
    if (i > 0) append(out, cap, at, " ");
    append(out, cap, at, one);
  }
}

void scaleTriads(const int index, const int root, char* out, const size_t cap) {
  const Scale& s = scale(index);
  size_t at = 0;
  if (cap > 0) out[0] = 0;
  if (s.count != 7) return;
  for (int d = 0; d < 7; ++d) {
    // Stack two thirds from the scale's own notes.
    const int a = s.steps[d];
    const int b = s.steps[(d + 2) % 7] + ((d + 2) >= 7 ? 12 : 0);
    const int c = s.steps[(d + 4) % 7] + ((d + 4) >= 7 ? 12 : 0);
    const int third = b - a;
    const int fifth = c - a;
    const char* quality = "";
    if (third == 3 && fifth == 7) quality = "M";
    if (third == 3 && fifth == 6) quality = "DIM";
    if (third == 4 && fifth == 8) quality = "AUG";
    if (d > 0) append(out, cap, at, "  ");
    append(out, cap, at, noteName(root + a));
    append(out, cap, at, quality);
  }
}

const Chord& chord(const int index) { return kChordTable[index < 0 ? 0 : (index >= kChords ? kChords - 1 : index)]; }

uint16_t chordMask(const int index, const int root) {
  const Chord& c = chord(index);
  uint16_t mask = 0;
  for (int i = 0; i < c.count; ++i) mask = static_cast<uint16_t>(mask | (1u << ((root + c.steps[i]) % 12)));
  return mask;
}

void chordNotes(const int index, const int root, char* out, const size_t cap) {
  const Chord& c = chord(index);
  size_t at = 0;
  if (cap > 0) out[0] = 0;
  for (int i = 0; i < c.count; ++i) {
    if (i > 0) append(out, cap, at, " ");
    append(out, cap, at, noteName(root + c.steps[i]));
  }
}

const Progression& progression(const int index) {
  return kProgressionTable[index < 0 ? 0 : (index >= kProgressions ? kProgressions - 1 : index)];
}

void progressionChords(const int index, const int root, char* out, const size_t cap) {
  const Progression& p = progression(index);
  size_t at = 0;
  if (cap > 0) out[0] = 0;
  bool first = true;
  for (const char* c = p.chords; *c; ++c) {
    if (*c == ' ') continue;
    if (!first) append(out, cap, at, "  ");
    first = false;
    append(out, cap, at, noteName(root + stepOf(c[0])));
    const char q = c[1];
    append(out, cap, at, q == 'm' ? "M" : (q == 'd' ? "DIM" : (q == '7' ? "7" : "")));
    ++c;
  }
}

uint16_t euclid(int steps, int pulses, const int rotation) {
  if (steps < 1) steps = 1;
  if (steps > kMaxSteps) steps = kMaxSteps;
  if (pulses < 0) pulses = 0;
  if (pulses > steps) pulses = steps;
  uint16_t pattern = 0;
  // Step i sounds when the running total of pulses crosses a whole number
  // there: the same spread Bjorklund's algorithm gives, turned to start on a beat.
  for (int i = 0; i < steps; ++i) {
    if ((i * pulses) % steps < pulses) {
      const int at = (((i + rotation) % steps) + steps) % steps;
      pattern = static_cast<uint16_t>(pattern | (1u << at));
    }
  }
  return pattern;
}

const char* euclidName(const int steps, const int pulses) {
  struct Named {
    uint8_t steps, pulses;
    const char* name;
  };
  static const Named kNamed[] = {
      {8, 3, "TRESILLO"},    {8, 5, "CINQUILLO"},        {16, 5, "BOSSA NOVA"},        {12, 7, "WEST AFRICAN BELL"},
      {16, 9, "RUMBA-LIKE"}, {5, 2, "KHAFIF-E-RAMAL"},   {7, 3, "RUCHENITZA"},         {12, 5, "VENDA CLAPPING"},
      {4, 3, "CUMBIA"},      {8, 2, "HALF TIME"},        {16, 4, "FOUR ON THE FLOOR"}, {8, 4, "STRAIGHT EIGHTHS"},
      {9, 4, "AKSAK"},       {16, 7, "BRAZILIAN SAMBA"}, {6, 2, "WALTZ PULSE"},
  };
  for (const Named& n : kNamed) {
    if (n.steps == steps && n.pulses == pulses) return n.name;
  }
  return "";
}

float frequency(const int midi) { return 440.0f * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f); }

}  // namespace theory

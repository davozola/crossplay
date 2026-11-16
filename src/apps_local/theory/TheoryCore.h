#pragma once

// MUSIC THEORY LAB's model: scales, chords and progressions spelt out in a
// key, and Euclidean rhythms. Freestanding. Notes are numbered 0..11 from C.

#include <cstddef>
#include <cstdint>

namespace theory {

const char* noteName(int pitchClass);  // sharps: "C", "C#", ...

struct Scale {
  const char* name;
  uint8_t steps[8];  // semitones above the root, in order
  uint8_t count;
};
constexpr int kScales = 14;
const Scale& scale(int index);
// The scale as a 12-bit set from `root`: bit n is the pitch class n.
uint16_t scaleMask(int index, int root);
// "W W H W W W H" for the steps between its notes; 3 for a step of three.
void scaleSteps(int index, char* out, size_t cap);
// The triad on each degree of a seven-note scale: "C  Dm  Em  F  G  Am  Bdim".
// Empty for a scale without seven notes.
void scaleTriads(int index, int root, char* out, size_t cap);

struct Chord {
  const char* name;     // "MAJOR 7"
  const char* suffix;   // "maj7" as written after the root, in capitals: "MAJ7"
  const char* formula;  // "1 3 5 7"
  uint8_t steps[6];
  uint8_t count;
};
constexpr int kChords = 16;
const Chord& chord(int index);
uint16_t chordMask(int index, int root);
// "C E G B"
void chordNotes(int index, int root, char* out, size_t cap);

struct Progression {
  const char* name;
  const char* numerals;  // "I V VI IV": degree, case-free; quality comes from `chords`
  // Each chord as semitones above the key's root and a quality: 'M' major,
  // 'm' minor, 'd' diminished, '7' dominant seventh.
  const char* chords;  // "0M 7M 9m 5M"
  bool minorKey;
};
constexpr int kProgressions = 12;
const Progression& progression(int index);
// The chords of a progression in a key: "C  G  Am  F".
void progressionChords(int index, int root, char* out, size_t cap);

// A Euclidean rhythm: `pulses` onsets spread as evenly as `steps` allow,
// turned by `rotation`. Bit i of the result is step i.
constexpr int kMaxSteps = 16;
uint16_t euclid(int steps, int pulses, int rotation);
// The name the rhythm goes by when it has one, or "".
const char* euclidName(int steps, int pulses);

// The frequency of a note in hertz: 60 is middle C, 69 is 440.
float frequency(int midi);

}  // namespace theory

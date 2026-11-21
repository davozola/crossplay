#pragma once

// RELOJ's freestanding half: everything a face needs that is not a pixel. The
// time and the date said in Spanish, the week of the year, and the pen-stroke
// numerals HOY borrows.

#include <cstddef>
#include <cstdint>

namespace clockcore {

constexpr int kFaces = 11;
const char* faceName(int face);

// One stroke of a numeral, in a box 1 wide and 1.6 tall with y downwards.
// Line: (x0, y0) to (x1, y1). Arc: centre (cx, cy), radii (rx, ry), from angle
// a0 to a1 in degrees, measured clockwise on the page from three o'clock.
struct Stroke {
  char kind;  // 'L' or 'A'
  float v[6];
};
constexpr float kNumeralHeight = 1.6f;
// The strokes of digit 0..9; returns how many.
int strokesOf(int digit, const Stroke*& out);

// A number from 0 to 59 in words, with its accents, in UTF-8: "nueve",
// "cincuenta y nueve".
void numberEs(int n, char* out, size_t cap);

// The time as it is said, to the minute, in three parts: "son las" / "diez" /
// "y nueve". Past the half hour it counts down to the next: "menos veinte".
struct Said {
  const char* lead;
  const char* hour;
  char tail[32];
};
Said saidEs(int hour, int minute);

// The time in one line with the part of the day it falls in.
void longTimeEs(int hour, int minute, char* out, size_t cap);

// The ISO 8601 week of a date, from its year, its day of the year (0 for the
// first of January) and its weekday (0 Sunday).
int isoWeek(int year, int yearDay, int weekday);

// Names with their accents, in UTF-8. Weekday 0 is Sunday; months are 1-based.
const char* weekdayLong(int weekday, bool capitals);
const char* weekdayShort(int weekday, bool capitals);
const char* monthLong(int month);
const char* monthShort(int month, bool capitals);
const char* monthRoman(int month);

// Capitals without accents, for the Toybox faces: "DOM", "ENE".
const char* weekdayEs(int weekday);
const char* monthEs(int month);

}  // namespace clockcore

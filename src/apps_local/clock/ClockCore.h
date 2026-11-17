#pragma once

// RELOJ's freestanding half: everything a face needs that is not a pixel. The
// time in Spanish words, the numerals as pen strokes, as seven segments and as
// a 5x7 dot matrix, and the short names of days and months.

#include <cstdint>

namespace clockcore {

constexpr int kFaces = 9;
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

// Seven segments, bit 0 = a (top) clockwise to f, bit 6 = g (middle).
uint8_t segmentsOf(int digit);
// Seven rows of five dots, bit 4 the leftmost.
const uint8_t* dotsOf(int digit);

// The time said in Spanish, to the nearest five minutes, on up to three lines:
// "SON LAS" / "DIEZ" / "Y VEINTE". Capitals without accents: the display face
// has none.
struct Words {
  const char* lead;  // "SON LAS", "ES LA"
  const char* hour;  // "DIEZ"
  const char* tail;  // "Y VEINTE", "EN PUNTO"
};
Words wordsEs(int hour, int minute);

// A clock made of clocks: each numeral is six small dials, two across and
// three down, whose two hands are set so that together they draw it. A hand
// points Up, Right, Down or Left; a dial with nothing to draw rests both hands
// on the diagonal.
enum Hand : uint8_t { Up = 0, Right = 1, Down = 2, Left = 3, Rest = 4 };
struct Dial {
  Hand a;
  Hand b;
};
// The six dials of digit 0..9, row by row.
const Dial* dialsOf(int digit);

const char* weekdayEs(int weekday);  // 0 is Sunday: "DOM"
const char* monthEs(int month);      // 1-based: "ENE"

}  // namespace clockcore

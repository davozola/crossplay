#include "ClockCore.h"

namespace clockcore {

namespace {

constexpr Stroke k0[] = {{'A', {0.5f, 0.8f, 0.5f, 0.8f, 0, 360}}};
constexpr Stroke k1[] = {{'L', {0.55f, 0, 0.55f, 1.6f}}, {'L', {0.2f, 0.32f, 0.55f, 0}}};
constexpr Stroke k2[] = {
    {'A', {0.5f, 0.5f, 0.5f, 0.5f, 180, 400}}, {'L', {0.883f, 0.821f, 0, 1.6f}}, {'L', {0, 1.6f, 1, 1.6f}}};
constexpr Stroke k3[] = {{'A', {0.5f, 0.4f, 0.46f, 0.4f, 200, 450}}, {'A', {0.5f, 1.2f, 0.5f, 0.4f, 270, 520}}};
constexpr Stroke k4[] = {{'L', {0.72f, 0, 0.72f, 1.6f}}, {'L', {0.72f, 0, 0, 1.12f}}, {'L', {0, 1.12f, 1, 1.12f}}};
constexpr Stroke k5[] = {
    {'L', {0.92f, 0, 0.14f, 0}}, {'L', {0.14f, 0, 0.07f, 0.74f}}, {'A', {0.48f, 1.08f, 0.52f, 0.52f, 225, 505}}};
constexpr Stroke k6[] = {
    {'A', {0.5f, 1.08f, 0.5f, 0.52f, 0, 360}}, {'A', {0.75f, 0.8f, 0.75f, 0.8f, 180, 280}}, {'L', {0, 0.8f, 0, 1.08f}}};
constexpr Stroke k7[] = {{'L', {0, 0, 1, 0}}, {'L', {1, 0, 0.38f, 1.6f}}};
constexpr Stroke k8[] = {{'A', {0.5f, 0.39f, 0.42f, 0.39f, 0, 360}}, {'A', {0.5f, 1.19f, 0.5f, 0.41f, 0, 360}}};
constexpr Stroke k9[] = {
    {'A', {0.5f, 0.52f, 0.5f, 0.52f, 0, 360}}, {'A', {0.25f, 0.8f, 0.75f, 0.8f, 0, 100}}, {'L', {1, 0.52f, 1, 0.8f}}};

struct Numeral {
  const Stroke* strokes;
  int count;
};
constexpr Numeral kNumerals[10] = {{k0, 1}, {k1, 2}, {k2, 3}, {k3, 2}, {k4, 3},
                                   {k5, 3}, {k6, 3}, {k7, 2}, {k8, 2}, {k9, 3}};

//                                    abcdefg, a in bit 0
constexpr uint8_t kSegments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

constexpr uint8_t kDots[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
};

constexpr const char* kHours[12] = {"DOCE", "UNA",   "DOS",  "TRES",  "CUATRO", "CINCO",
                                    "SEIS", "SIETE", "OCHO", "NUEVE", "DIEZ",   "ONCE"};
constexpr const char* kTails[12] = {"EN PUNTO",     "Y CINCO",       "Y DIEZ",     "Y CUARTO",
                                    "Y VEINTE",     "Y VEINTICINCO", "Y MEDIA",    "MENOS VEINTICINCO",
                                    "MENOS VEINTE", "MENOS CUARTO",  "MENOS DIEZ", "MENOS CINCO"};

int clampDigit(const int d) { return d < 0 ? 0 : (d > 9 ? 9 : d); }

}  // namespace

const char* faceName(const int face) {
  static const char* const kNames[kFaces] = {"TRAZO",     "PALETAS",  "ESFERA", "ANILLOS", "MATRIZ",
                                             "SEGMENTOS", "PALABRAS", "TAMBOR", "RELOJES"};
  return kNames[face < 0 ? 0 : (face >= kFaces ? kFaces - 1 : face)];
}

const Dial* dialsOf(const int digit) {
  constexpr Dial X{Rest, Rest};
  static constexpr Dial kDials[10][6] = {
      {{Right, Down}, {Left, Down}, {Up, Down}, {Up, Down}, {Up, Right}, {Up, Left}},          // 0
      {X, {Down, Down}, X, {Up, Down}, X, {Up, Up}},                                           // 1
      {{Right, Right}, {Left, Down}, {Right, Down}, {Left, Up}, {Up, Right}, {Left, Left}},    // 2
      {{Right, Right}, {Left, Down}, {Right, Right}, {Up, Down}, {Right, Right}, {Left, Up}},  // 3
      {{Down, Down}, {Down, Down}, {Up, Right}, {Up, Down}, X, {Up, Up}},                      // 4
      {{Right, Down}, {Left, Left}, {Up, Right}, {Left, Down}, {Right, Right}, {Left, Up}},    // 5
      {{Right, Down}, {Left, Left}, {Up, Down}, {Left, Down}, {Up, Right}, {Left, Up}},        // 6
      {{Right, Right}, {Left, Down}, X, {Up, Down}, X, {Up, Up}},                              // 7
      {{Right, Down}, {Left, Down}, {Up, Right}, {Up, Left}, {Up, Right}, {Up, Left}},         // 8
      {{Right, Down}, {Left, Down}, {Up, Right}, {Up, Down}, {Right, Right}, {Left, Up}},      // 9
  };
  return kDials[clampDigit(digit)];
}

int strokesOf(const int digit, const Stroke*& out) {
  const Numeral& n = kNumerals[clampDigit(digit)];
  out = n.strokes;
  return n.count;
}

uint8_t segmentsOf(const int digit) { return kSegments[clampDigit(digit)]; }

const uint8_t* dotsOf(const int digit) { return kDots[clampDigit(digit)]; }

Words wordsEs(const int hour, const int minute) {
  // To the nearest five; from twenty-five to on, the hour named is the next one.
  int slot = (minute + 2) / 5;
  int h = hour;
  if (slot == 12) {
    slot = 0;
    ++h;
  }
  if (slot >= 7) ++h;
  h = ((h % 12) + 12) % 12;
  Words w;
  w.lead = h == 1 ? "ES LA" : "SON LAS";
  w.hour = kHours[h];
  w.tail = kTails[slot];
  return w;
}

const char* weekdayEs(const int weekday) {
  static const char* const kDays[7] = {"DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB"};
  return kDays[((weekday % 7) + 7) % 7];
}

const char* monthEs(const int month) {
  static const char* const kMonths[12] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN",
                                          "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};
  return kMonths[month < 1 ? 0 : (month > 12 ? 11 : month - 1)];
}

}  // namespace clockcore

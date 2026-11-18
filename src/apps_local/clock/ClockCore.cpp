#include "ClockCore.h"

#include <cstdio>

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

int clampDigit(const int d) { return d < 0 ? 0 : (d > 9 ? 9 : d); }

}  // namespace

const char* faceName(const int face) {
  static const char* const kNames[kFaces] = {"EDITORIAL", "PUNTOS",  "POSTER", "GROTESCA", "NOIR",
                                             "TERMINAL",  "CAPSULA", "PIXEL",  "PALABRAS", "ANALOGICO"};
  return kNames[face < 0 ? 0 : (face >= kFaces ? kFaces - 1 : face)];
}

int strokesOf(const int digit, const Stroke*& out) {
  const Numeral& n = kNumerals[clampDigit(digit)];
  out = n.strokes;
  return n.count;
}

namespace {

constexpr const char* kUnits[30] = {
    "cero",         "uno",         "dos",        "tres",        "cuatro",     "cinco",      "seis",      "siete",
    "ocho",         "nueve",       "diez",       "once",        "doce",       "trece",      "catorce",   "quince",
    "dieciséis",    "diecisiete",  "dieciocho",  "diecinueve",  "veinte",     "veintiuno",  "veintidós", "veintitrés",
    "veinticuatro", "veinticinco", "veintiséis", "veintisiete", "veintiocho", "veintinueve"};
constexpr const char* kTens[6] = {"", "", "", "treinta", "cuarenta", "cincuenta"};
constexpr const char* kHourNames[12] = {"doce", "una",   "dos",  "tres",  "cuatro", "cinco",
                                        "seis", "siete", "ocho", "nueve", "diez",   "once"};

}  // namespace

void numberEs(int n, char* out, const size_t cap) {
  if (n < 0) n = 0;
  if (n > 59) n = 59;
  if (n < 30) {
    std::snprintf(out, cap, "%s", kUnits[n]);
  } else if (n % 10 == 0) {
    std::snprintf(out, cap, "%s", kTens[n / 10]);
  } else {
    std::snprintf(out, cap, "%s y %s", kTens[n / 10], kUnits[n % 10]);
  }
}

Said saidEs(const int hour, const int minute) {
  Said said;
  const int m = minute < 0 ? 0 : (minute > 59 ? 59 : minute);
  // Past the half the hour named is the next one, and the minutes count down.
  const bool toward = m > 30;
  const int h = (((hour + (toward ? 1 : 0)) % 12) + 12) % 12;
  said.lead = h == 1 ? "es la" : "son las";
  said.hour = kHourNames[h];
  const int count = toward ? 60 - m : m;
  if (count == 0) {
    std::snprintf(said.tail, sizeof(said.tail), "en punto");
  } else {
    char number[24];
    if (count == 15) {
      std::snprintf(number, sizeof(number), "cuarto");
    } else if (count == 30) {
      std::snprintf(number, sizeof(number), "media");
    } else {
      numberEs(count, number, sizeof(number));
    }
    std::snprintf(said.tail, sizeof(said.tail), "%s %s", toward ? "menos" : "y", number);
  }
  return said;
}

void longTimeEs(const int hour, const int minute, char* out, const size_t cap) {
  const int m = minute < 0 ? 0 : (minute > 59 ? 59 : minute);
  const int h24 = ((hour % 24) + 24) % 24;
  const int h = h24 % 12;
  const char* part = h24 < 6 ? "de la madrugada" : h24 < 12 ? "de la mañana" : h24 < 21 ? "de la tarde" : "de la noche";
  char tail[32];
  if (m == 0) {
    std::snprintf(tail, sizeof(tail), "en punto");
  } else if (m == 15) {
    std::snprintf(tail, sizeof(tail), "y cuarto");
  } else if (m == 30) {
    std::snprintf(tail, sizeof(tail), "y media");
  } else {
    char number[24];
    numberEs(m, number, sizeof(number));
    std::snprintf(tail, sizeof(tail), "y %s", number);
  }
  std::snprintf(out, cap, "%s %s %s %s", h == 1 ? "la" : "las", kHourNames[h], tail, part);
}

int isoWeek(const int year, const int yearDay, const int weekday) {
  // ISO 8601: weeks begin on Monday and the first is the one holding the
  // year's first Thursday.
  const auto leap = [](const int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; };
  const int isoDay = weekday == 0 ? 7 : weekday;  // Monday 1 .. Sunday 7
  // The ISO day the year began on.
  const int jan1 = ((isoDay - 1 - yearDay) % 7 + 7) % 7 + 1;
  int week = (yearDay + 1 - isoDay + 10) / 7;
  if (week < 1) {
    // The last week of the year before, which has 53 when that year began on
    // a Thursday, or on a Wednesday and was a leap year.
    const int lastYear = year - 1;
    const int before = ((jan1 - 1 - (leap(lastYear) ? 366 : 365)) % 7 + 7) % 7 + 1;
    week = (before == 4 || (before == 3 && leap(lastYear))) ? 53 : 52;
  } else if (week == 53 && !(jan1 == 4 || (jan1 == 3 && leap(year)))) {
    week = 1;
  }
  return week;
}

const char* weekdayLong(const int weekday, const bool capitals) {
  static const char* const kLower[7] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
  static const char* const kUpper[7] = {"DOMINGO", "LUNES", "MARTES", "MIÉRCOLES", "JUEVES", "VIERNES", "SÁBADO"};
  return (capitals ? kUpper : kLower)[((weekday % 7) + 7) % 7];
}

const char* weekdayShort(const int weekday, const bool capitals) {
  static const char* const kLower[7] = {"dom", "lun", "mar", "mié", "jue", "vie", "sáb"};
  static const char* const kUpper[7] = {"DOM", "LUN", "MAR", "MIÉ", "JUE", "VIE", "SÁB"};
  return (capitals ? kUpper : kLower)[((weekday % 7) + 7) % 7];
}

const char* monthLong(const int month) {
  static const char* const kMonths[12] = {"enero", "febrero", "marzo",      "abril",   "mayo",      "junio",
                                          "julio", "agosto",  "septiembre", "octubre", "noviembre", "diciembre"};
  return kMonths[month < 1 ? 0 : (month > 12 ? 11 : month - 1)];
}

const char* monthShort(const int month, const bool capitals) {
  static const char* const kLower[12] = {"ene", "feb", "mar", "abr", "may", "jun",
                                         "jul", "ago", "sep", "oct", "nov", "dic"};
  return capitals ? monthEs(month) : kLower[month < 1 ? 0 : (month > 12 ? 11 : month - 1)];
}

const char* monthRoman(const int month) {
  static const char* const kMonths[12] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII"};
  return kMonths[month < 1 ? 0 : (month > 12 ? 11 : month - 1)];
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

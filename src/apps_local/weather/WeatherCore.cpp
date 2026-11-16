#include "WeatherCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace weather {

namespace {

// A view into the response. The parser is not a general JSON reader: it finds
// a key at the top level of one object and reads the value after it, which is
// all Open-Meteo's flat answers need.
struct Span {
  const char* p = nullptr;
  const char* end = nullptr;
  bool ok() const { return p != nullptr && p < end; }
};

const char* skipString(const char* p, const char* end) {
  // p at the opening quote.
  ++p;
  while (p < end && *p != '"') {
    if (*p == '\\' && p + 1 < end) ++p;
    ++p;
  }
  return p < end ? p + 1 : end;
}

// Past one value of any kind.
const char* skipValue(const char* p, const char* end) {
  while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
  if (p >= end) return end;
  if (*p == '"') return skipString(p, end);
  if (*p == '{' || *p == '[') {
    int depth = 0;
    while (p < end) {
      if (*p == '"') {
        p = skipString(p, end);
        continue;
      }
      if (*p == '{' || *p == '[') ++depth;
      if (*p == '}' || *p == ']') {
        --depth;
        if (depth == 0) return p + 1;
      }
      ++p;
    }
    return end;
  }
  while (p < end && *p != ',' && *p != '}' && *p != ']') ++p;
  return p;
}

const char* skipSpace(const char* p, const char* end) {
  while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
  return p;
}

// The value of `key` among the members of the object `obj` (which spans the
// braces). Nested objects are stepped over, so "hourly" never matches inside
// "hourly_units" and "temperature_2m" never matches one level down.
Span member(const Span& obj, const char* key) {
  Span none;
  if (!obj.ok() || *obj.p != '{') return none;
  const size_t keyLen = std::strlen(key);
  const char* p = obj.p + 1;
  while (p < obj.end) {
    p = skipSpace(p, obj.end);
    if (p >= obj.end || *p == '}') break;
    if (*p == ',') {
      ++p;
      continue;
    }
    if (*p != '"') return none;
    const char* k = p + 1;
    const char* kEnd = skipString(p, obj.end) - 1;
    p = skipSpace(kEnd + 1, obj.end);
    if (p >= obj.end || *p != ':') return none;
    p = skipSpace(p + 1, obj.end);
    const char* vEnd = skipValue(p, obj.end);
    if (static_cast<size_t>(kEnd - k) == keyLen && std::strncmp(k, key, keyLen) == 0) {
      Span v;
      v.p = p;
      v.end = vEnd;
      return v;
    }
    p = vEnd;
  }
  return none;
}

bool isNull(const Span& v) { return v.ok() && v.end - v.p >= 4 && std::strncmp(v.p, "null", 4) == 0; }

bool number(const Span& v, float& out) {
  if (!v.ok() || isNull(v)) return false;
  char buf[32];
  size_t n = static_cast<size_t>(v.end - v.p);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  std::memcpy(buf, v.p, n);
  buf[n] = 0;
  char* stop = nullptr;
  out = std::strtof(buf, &stop);
  return stop != buf;
}

// The raw bytes between the quotes, escapes left as they are except \" and \\.
void string(const Span& v, char* out, size_t cap) {
  if (cap == 0) return;
  out[0] = 0;
  if (!v.ok() || *v.p != '"') return;
  size_t at = 0;
  for (const char* p = v.p + 1; p < v.end && *p != '"' && at + 1 < cap; ++p) {
    if (*p == '\\' && p + 1 < v.end) ++p;
    out[at++] = *p;
  }
  out[at] = 0;
}

// Calls `each(index, element)` for each element of the array `arr`, up to max.
template <typename F>
int forEach(const Span& arr, const int max, F each) {
  if (!arr.ok() || *arr.p != '[') return 0;
  const char* p = arr.p + 1;
  int i = 0;
  while (p < arr.end && i < max) {
    p = skipSpace(p, arr.end);
    if (p >= arr.end || *p == ']') break;
    if (*p == ',') {
      ++p;
      continue;
    }
    Span el;
    el.p = p;
    el.end = skipValue(p, arr.end);
    each(i, el);
    ++i;
    p = el.end;
  }
  return i;
}

int toInt(const Span& v, int fallback = 0) {
  float f = 0;
  return number(v, f) ? static_cast<int>(std::lround(f)) : fallback;
}

int digits(const char* s, int n) {
  int v = 0;
  for (int i = 0; i < n; ++i) {
    if (s[i] < '0' || s[i] > '9') return -1;
    v = v * 10 + (s[i] - '0');
  }
  return v;
}

// "2026-11-16T07:58" -> "07:58"
void clockOf(const char* iso, char* out, size_t cap) {
  const char* t = std::strchr(iso, 'T');
  std::snprintf(out, cap, "%.5s", t ? t + 1 : "");
}

// Appends one Unicode code point's Latin reading, upper case.
void foldCodepoint(const uint32_t cp, char* out, size_t& at, const size_t cap) {
  auto put = [&](const char* s) {
    for (; *s && at + 1 < cap; ++s) out[at++] = *s;
  };
  if (cp < 0x80) {
    char c = static_cast<char>(cp);
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c >= 0x20 && c < 0x7F) {
      const char s[2] = {c, 0};
      put(s);
    }
    return;
  }
  // Latin-1 Supplement and Latin Extended-A, by family.
  struct Range {
    uint16_t from, to;
    const char* as;
  };
  static const Range kRanges[] = {
      {0xC0, 0xC5, "A"},    {0xC6, 0xC6, "AE"},  {0xC7, 0xC7, "C"},   {0xC8, 0xCB, "E"},     {0xCC, 0xCF, "I"},
      {0xD0, 0xD0, "D"},    {0xD1, 0xD1, "N"},   {0xD2, 0xD6, "O"},   {0xD8, 0xD8, "O"},     {0xD9, 0xDC, "U"},
      {0xDD, 0xDD, "Y"},    {0xDF, 0xDF, "SS"},  {0xE0, 0xE5, "A"},   {0xE6, 0xE6, "AE"},    {0xE7, 0xE7, "C"},
      {0xE8, 0xEB, "E"},    {0xEC, 0xEF, "I"},   {0xF0, 0xF0, "D"},   {0xF1, 0xF1, "N"},     {0xF2, 0xF6, "O"},
      {0xF8, 0xF8, "O"},    {0xF9, 0xFC, "U"},   {0xFD, 0xFF, "Y"},   {0x100, 0x105, "A"},   {0x106, 0x10D, "C"},
      {0x10E, 0x111, "D"},  {0x112, 0x11B, "E"}, {0x11C, 0x123, "G"}, {0x124, 0x127, "H"},   {0x128, 0x131, "I"},
      {0x134, 0x135, "J"},  {0x136, 0x138, "K"}, {0x139, 0x142, "L"}, {0x143, 0x14B, "N"},   {0x14C, 0x151, "O"},
      {0x152, 0x153, "OE"}, {0x154, 0x159, "R"}, {0x15A, 0x161, "S"}, {0x162, 0x167, "T"},   {0x168, 0x173, "U"},
      {0x174, 0x175, "W"},  {0x176, 0x178, "Y"}, {0x179, 0x17E, "Z"}, {0x2018, 0x2019, "'"}, {0x2010, 0x2015, "-"},
  };
  for (const Range& r : kRanges) {
    if (cp >= r.from && cp <= r.to) {
      put(r.as);
      return;
    }
  }
}

}  // namespace

Sky skyOf(const int wmo) {
  switch (wmo) {
    case 0:
      return Sky::Clear;
    case 1:
    case 2:
      return Sky::PartCloud;
    case 3:
      return Sky::Cloud;
    case 45:
    case 48:
      return Sky::Fog;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
      return Sky::Drizzle;
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
      return Sky::Rain;
    case 80:
    case 81:
    case 82:
      return Sky::Showers;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
      return Sky::Snow;
    case 95:
    case 96:
    case 99:
      return Sky::Thunder;
    default:
      return Sky::Cloud;
  }
}

const char* labelOf(const int wmo) {
  switch (wmo) {
    case 0:
      return "CLEAR";
    case 1:
      return "MOSTLY CLEAR";
    case 2:
      return "PARTLY CLOUDY";
    case 3:
      return "OVERCAST";
    case 45:
      return "FOG";
    case 48:
      return "FREEZING FOG";
    case 51:
      return "LIGHT DRIZZLE";
    case 53:
      return "DRIZZLE";
    case 55:
      return "HEAVY DRIZZLE";
    case 56:
    case 57:
      return "FREEZING DRIZZLE";
    case 61:
      return "LIGHT RAIN";
    case 63:
      return "RAIN";
    case 65:
      return "HEAVY RAIN";
    case 66:
    case 67:
      return "FREEZING RAIN";
    case 71:
      return "LIGHT SNOW";
    case 73:
      return "SNOW";
    case 75:
      return "HEAVY SNOW";
    case 77:
      return "SNOW GRAINS";
    case 80:
      return "LIGHT SHOWERS";
    case 81:
      return "SHOWERS";
    case 82:
      return "HEAVY SHOWERS";
    case 85:
    case 86:
      return "SNOW SHOWERS";
    case 95:
      return "THUNDERSTORM";
    case 96:
    case 99:
      return "STORM, HAIL";
    default:
      return "CLOUDY";
  }
}

bool parseForecast(const char* json, const size_t len, Forecast& out) {
  out = Forecast{};
  Span root;
  root.p = skipSpace(json, json + len);
  root.end = json + len;
  if (!root.ok() || *root.p != '{') return false;

  const Span cur = member(root, "current");
  if (!cur.ok()) return false;
  float f = 0;
  if (!number(member(cur, "temperature_2m"), out.current.temp)) return false;
  if (!number(member(cur, "apparent_temperature"), out.current.feels)) out.current.feels = out.current.temp;
  if (number(member(cur, "wind_speed_10m"), f)) out.current.wind = f;
  out.current.humidity = toInt(member(cur, "relative_humidity_2m"));
  out.current.code = toInt(member(cur, "weather_code"));
  out.current.isDay = toInt(member(cur, "is_day"), 1) != 0;
  string(member(cur, "time"), out.current.time, sizeof(out.current.time));

  const Span hourly = member(root, "hourly");
  if (hourly.ok()) {
    Hour* hours = out.hours;
    const int n = forEach(member(hourly, "time"), kHours, [hours](const int i, const Span& el) {
      char iso[20];
      string(el, iso, sizeof(iso));
      const char* t = std::strchr(iso, 'T');
      hours[i].hour = static_cast<int8_t>(t ? digits(t + 1, 2) : 0);
    });
    forEach(member(hourly, "temperature_2m"), n, [hours](const int i, const Span& el) { number(el, hours[i].temp); });
    forEach(member(hourly, "weather_code"), n,
            [hours](const int i, const Span& el) { hours[i].code = static_cast<int16_t>(toInt(el)); });
    forEach(member(hourly, "precipitation_probability"), n,
            [hours](const int i, const Span& el) { hours[i].precip = static_cast<int8_t>(toInt(el)); });
    out.hourCount = n;
  }

  const Span daily = member(root, "daily");
  if (!daily.ok()) return false;
  Day* days = out.days;
  const int n = forEach(member(daily, "time"), kDays, [days](const int i, const Span& el) {
    char iso[12];
    string(el, iso, sizeof(iso));
    days[i].year = static_cast<int16_t>(digits(iso, 4));
    days[i].month = static_cast<int8_t>(digits(iso + 5, 2));
    days[i].day = static_cast<int8_t>(digits(iso + 8, 2));
  });
  if (n == 0) return false;
  forEach(member(daily, "temperature_2m_max"), n, [days](const int i, const Span& el) { number(el, days[i].max); });
  forEach(member(daily, "temperature_2m_min"), n, [days](const int i, const Span& el) { number(el, days[i].min); });
  forEach(member(daily, "weather_code"), n,
          [days](const int i, const Span& el) { days[i].code = static_cast<int16_t>(toInt(el)); });
  forEach(member(daily, "precipitation_probability_max"), n,
          [days](const int i, const Span& el) { days[i].precip = static_cast<int8_t>(toInt(el)); });
  forEach(member(daily, "sunrise"), n, [days](const int i, const Span& el) {
    char iso[20];
    string(el, iso, sizeof(iso));
    clockOf(iso, days[i].sunrise, sizeof(days[i].sunrise));
  });
  forEach(member(daily, "sunset"), n, [days](const int i, const Span& el) {
    char iso[20];
    string(el, iso, sizeof(iso));
    clockOf(iso, days[i].sunset, sizeof(days[i].sunset));
  });
  out.dayCount = n;
  return true;
}

int parseGeocode(const char* json, const size_t len, Place* out, const int max) {
  Span root;
  root.p = skipSpace(json, json + len);
  root.end = json + len;
  if (!root.ok() || *root.p != '{') return 0;
  int kept = 0;
  forEach(member(root, "results"), kMaxResults * 2, [&](int, const Span& el) {
    if (kept >= max) return;
    Place& p = out[kept];
    p = Place{};
    char raw[96];
    string(member(el, "name"), raw, sizeof(raw));
    foldAscii(raw, std::strlen(raw), p.name, sizeof(p.name));
    if (!p.name[0]) return;
    if (!number(member(el, "latitude"), p.lat) || !number(member(el, "longitude"), p.lon)) return;
    char admin[64];
    char country[64];
    string(member(el, "admin1"), raw, sizeof(raw));
    foldAscii(raw, std::strlen(raw), admin, sizeof(admin));
    string(member(el, "country"), raw, sizeof(raw));
    foldAscii(raw, std::strlen(raw), country, sizeof(country));
    // Truncated by hand: the line is cut where it runs out of room.
    size_t at = 0;
    auto put = [&](const char* s) {
      for (; *s && at + 1 < sizeof(p.region); ++s) p.region[at++] = *s;
    };
    if (admin[0] && country[0] && std::strcmp(admin, p.name) != 0) {
      put(admin);
      put(", ");
      put(country);
    } else {
      put(country[0] ? country : admin);
    }
    p.region[at] = 0;
    for (int i = 0; i < kept; ++i) {
      if (samePlace(out[i], p)) return;
    }
    ++kept;
  });
  return kept;
}

void foldAscii(const char* utf8, const size_t len, char* out, const size_t cap) {
  if (cap == 0) return;
  size_t at = 0;
  const auto* s = reinterpret_cast<const uint8_t*>(utf8);
  size_t i = 0;
  while (i < len && s[i]) {
    uint32_t cp = s[i];
    int extra = 0;
    if (cp >= 0xF0) {
      cp &= 0x07;
      extra = 3;
    } else if (cp >= 0xE0) {
      cp &= 0x0F;
      extra = 2;
    } else if (cp >= 0xC0) {
      cp &= 0x1F;
      extra = 1;
    } else if (cp >= 0x80) {
      ++i;  // stray continuation byte
      continue;
    }
    ++i;
    for (int k = 0; k < extra && i < len; ++k, ++i) cp = (cp << 6) | (s[i] & 0x3F);
    foldCodepoint(cp, out, at, cap);
  }
  // No leading or doubled spaces left behind by a skipped character.
  size_t w = 0;
  for (size_t r = 0; r < at; ++r) {
    if (out[r] == ' ' && (w == 0 || out[w - 1] == ' ')) continue;
    out[w++] = out[r];
  }
  while (w > 0 && out[w - 1] == ' ') --w;
  out[w] = 0;
}

void forecastUrl(const Place& place, char* out, const size_t cap) {
  std::snprintf(out, cap,
                "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
                "&hourly=temperature_2m,weather_code,precipitation_probability"
                "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
                "&timezone=auto&forecast_days=%d&forecast_hours=%d",
                static_cast<double>(place.lat), static_cast<double>(place.lon), kDays, kHours);
}

void geocodeUrl(const char* query, char* out, const size_t cap) {
  int at =
      std::snprintf(out, cap, "https://geocoding-api.open-meteo.com/v1/search?count=%d&format=json&name=", kMaxResults);
  static const char kHex[] = "0123456789ABCDEF";
  for (const char* q = query; *q && at + 4 < static_cast<int>(cap); ++q) {
    const auto c = static_cast<unsigned char>(*q);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
        c == '_') {
      out[at++] = static_cast<char>(c);
    } else {
      out[at++] = '%';
      out[at++] = kHex[c >> 4];
      out[at++] = kHex[c & 0xF];
    }
  }
  if (at < static_cast<int>(cap)) out[at] = 0;
}

int weekday(int year, const int month, const int day) {
  // Sakamoto, shifted so Monday is 0.
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;
  const int sunday0 = (year + year / 4 - year / 100 + year / 400 + t[(month - 1) % 12] + day) % 7;
  return (sunday0 + 6) % 7;
}

const char* weekdayName(const int wd) {
  static const char* const kNames[7] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
  return wd >= 0 && wd < 7 ? kNames[wd] : "";
}

const char* monthName(const int month) {
  static const char* const kNames[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                         "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  return month >= 1 && month <= 12 ? kNames[month - 1] : "";
}

int displayTemp(const float celsius, const bool fahrenheit) {
  const float v = fahrenheit ? celsius * 9.0f / 5.0f + 32.0f : celsius;
  return static_cast<int>(std::lround(v));
}

int displayWind(const float kmh, const bool imperial) {
  return static_cast<int>(std::lround(imperial ? kmh / 1.609344f : kmh));
}

bool samePlace(const Place& a, const Place& b) {
  return std::fabs(a.lat - b.lat) < 0.01f && std::fabs(a.lon - b.lon) < 0.01f;
}

}  // namespace weather

#pragma once

// A reader for flat JSON answers, freestanding and allocation-free.
//
// Not a general parser: it finds a key among the members of one object and
// hands back the span of its value, steps through an array, and reads a number
// or a string. That is all the weather and market answers need, and it runs
// over the response where it lies.

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace flatjson {

// A view into the response. The parser is not a general JSON reader: it finds
// a key at the top level of one object and reads the value after it, which is
// all Open-Meteo's flat answers need.
struct Span {
  const char* p = nullptr;
  const char* end = nullptr;
  bool ok() const { return p != nullptr && p < end; }
};

inline const char* skipString(const char* p, const char* end) {
  // p at the opening quote.
  ++p;
  while (p < end && *p != '"') {
    if (*p == '\\' && p + 1 < end) ++p;
    ++p;
  }
  return p < end ? p + 1 : end;
}

// Past one value of any kind.
inline const char* skipValue(const char* p, const char* end) {
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

inline const char* skipSpace(const char* p, const char* end) {
  while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
  return p;
}

// The value of `key` among the members of the object `obj` (which spans the
// braces). Nested objects are stepped over, so "hourly" never matches inside
// "hourly_units" and "temperature_2m" never matches one level down.
inline Span member(const Span& obj, const char* key) {
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

inline bool isNull(const Span& v) { return v.ok() && v.end - v.p >= 4 && std::strncmp(v.p, "null", 4) == 0; }

inline bool number(const Span& v, float& out) {
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
inline void string(const Span& v, char* out, size_t cap) {
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

// The whole document as a span, or one that is not ok() when it is not an
// object.
inline Span root(const char* json, const size_t len) {
  Span r;
  r.p = skipSpace(json, json + len);
  r.end = json + len;
  if (r.p >= r.end || *r.p != '{') r.p = nullptr;
  return r;
}

// Element `index` of an array, or a span that is not ok().
inline Span element(const Span& arr, const int index) {
  Span found;
  forEach(arr, index + 1, [&](const int i, const Span& el) {
    if (i == index) found = el;
  });
  return found;
}

inline bool numberD(const Span& v, double& out) {
  if (!v.ok() || isNull(v)) return false;
  char buf[40];
  size_t n = static_cast<size_t>(v.end - v.p);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  std::memcpy(buf, v.p, n);
  buf[n] = 0;
  char* stop = nullptr;
  out = std::strtod(buf, &stop);
  return stop != buf;
}

}  // namespace flatjson

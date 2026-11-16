#pragma once

// Where NAVAL WEATHER keeps a place's two answers. HOY reads and writes the
// same files, so the paths are said once, here.

#include <cstddef>
#include <cstdio>

namespace naval {

constexpr char kCacheDir[] = "/.crosspoint/naval";
// `which` is "sea" for the marine answer or "wind" for the forecast one.
inline void cachePathOf(const float lat, const float lon, const char* which, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%d_%d-%s.json", kCacheDir, static_cast<int>(lat * 100), static_cast<int>(lon * 100),
                which);
}

}  // namespace naval

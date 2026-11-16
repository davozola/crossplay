#pragma once

#include <cstring>

#include "ShelfHidden.h"

// The order a folder lists its items in: the few opened last come first, the
// newest at the top, and everything else keeps the order of the table.
// `recent` holds titles oldest first, of any folder.

namespace shelf {

constexpr int kRecentRows = 3;
constexpr size_t kRecentKept = 12;
constexpr int kFavouriteSlots = 24;

template <typename TitleAt>
int recentItemsIn(const HiddenSet& hidden, const HiddenSet& recent, const int count, TitleAt titleAt, int* out) {
  int n = 0;
  for (size_t r = recent.size(); r > 0 && n < kRecentRows; --r) {
    const std::string& title = recent.at(r - 1);
    if (hidden.contains(title.c_str())) continue;
    for (int i = 0; i < count; ++i) {
      if (title == titleAt(i)) {
        out[n++] = i;
        break;
      }
    }
  }
  return n;
}

template <typename TitleAt>
int orderedItemIn(const HiddenSet& hidden, const HiddenSet& recent, const int count, const int row, TitleAt titleAt) {
  if (row < 0) return -1;
  int first[kRecentRows];
  const int n = recentItemsIn(hidden, recent, count, titleAt, first);
  if (row < n) return first[row];
  int seen = n;
  for (int i = 0; i < count; ++i) {
    if (hidden.contains(titleAt(i))) continue;
    bool lifted = false;
    for (int k = 0; k < n; ++k) lifted = lifted || first[k] == i;
    if (lifted) continue;
    if (seen == row) return i;
    ++seen;
  }
  return -1;
}

// The row an item sits on. For a hidden item, the row of what follows it.
template <typename TitleAt>
int orderedRowForIn(const HiddenSet& hidden, const HiddenSet& recent, const int count, const int item,
                    TitleAt titleAt) {
  int first[kRecentRows];
  const int n = recentItemsIn(hidden, recent, count, titleAt, first);
  for (int k = 0; k < n; ++k) {
    if (first[k] == item) return k;
  }
  int before = n;
  for (int i = 0; i < item && i < count; ++i) {
    if (hidden.contains(titleAt(i))) continue;
    bool lifted = false;
    for (int k = 0; k < n; ++k) lifted = lifted || first[k] == i;
    if (!lifted) ++before;
  }
  return before;
}

// Moves a title to the newest end. False when it was already there.
inline bool noteRecent(HiddenSet& recent, const char* title) {
  if (title == nullptr || *title == '\0') return false;
  if (recent.size() > 0 && recent.at(recent.size() - 1) == title) return false;
  recent.set(title, false);
  recent.set(title, true);
  while (recent.size() > kRecentKept) {
    const std::string oldest = recent.at(0);
    recent.set(oldest.c_str(), false);
  }
  return true;
}

// The favourites of one folder, in the order they were chosen. `out` needs
// room for kFavouriteSlots.
template <typename TitleAt>
int favouriteItemsIn(const HiddenSet& hidden, const HiddenSet& favourites, const int count, TitleAt titleAt, int* out) {
  int n = 0;
  for (size_t f = 0; f < favourites.size() && n < kFavouriteSlots; ++f) {
    const std::string& title = favourites.at(f);
    if (hidden.contains(title.c_str())) continue;
    for (int i = 0; i < count; ++i) {
      if (title == titleAt(i)) {
        out[n++] = i;
        break;
      }
    }
  }
  return n;
}

// Adds or removes a favourite. A folder that is full gives up its oldest.
template <typename TitleAt>
void toggleFavouriteIn(HiddenSet& favourites, const int count, const int item, TitleAt titleAt) {
  if (item < 0 || item >= count) return;
  if (favourites.set(titleAt(item), false)) return;
  const HiddenSet none;
  int held[kFavouriteSlots];
  if (favouriteItemsIn(none, favourites, count, titleAt, held) >= kFavouriteSlots) {
    favourites.set(titleAt(held[0]), false);
  }
  favourites.set(titleAt(item), true);
}

}  // namespace shelf

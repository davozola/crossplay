#pragma once

// EFEMERIDES' network and card: a day's events fetched from Wikipedia and kept
// as one file per day, for the app itself and for HOY, which prints from them.

#include <cstddef>
#include <vector>

#include "HistoryCore.h"

namespace historystore {

// "/.crosspoint/efemerides/MM-DD.txt"
void dayPath(int month, int day, char* out, size_t cap);

// Fetches the day's events, sorted and without repeats, and saves them to the
// card. Needs WiFi up. False, with `out` empty, when nothing came.
bool fetchDay(int month, int day, std::vector<history::Event>& out);

}  // namespace historystore

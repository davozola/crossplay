#pragma once

// TACTICS' model: a puzzle out of a pack, the solving of it move by move, the
// rating that follows how it went and the numbers kept about it. The puzzles
// are Lichess's (CC0), packed by tools_local/tactics/make_pack.py. Freestanding
// (no Arduino), so host-tests/tactics runs it.

#include <cstddef>
#include <cstdint>

#include "../chess/ChessCore.h"

namespace tactics {

constexpr size_t kHeaderBytes = 32;
constexpr size_t kRecordBytes = 72;
constexpr int kMaxMoves = 12;
constexpr int kThemes = 32;

struct Header {
  uint32_t count = 0;
  char name[20] = "";
};
bool readHeader(const uint8_t* bytes, size_t size, Header& out);

struct Puzzle {
  char id[6] = "";
  uint16_t rating = 0;
  uint32_t themes = 0;
  uint8_t moveCount = 0;
  chess::Move moves[kMaxMoves];  // the first is the opponent's, played before you see it
  chess::Position start;
};
// False for a record that is not a puzzle this can play.
bool decode(const uint8_t* record, Puzzle& out);
// The rating in a record, without decoding the rest: what a search reads.
uint16_t ratingOf(const uint8_t* record);
const char* themeName(int theme);  // "FORK"

// One puzzle being solved.
struct Run {
  Puzzle puzzle;
  chess::Position position;  // as it stands, with the opponent's move played
  uint8_t step = 0;          // the move of puzzle.moves wanted next
  bool missed = false;       // a wrong move or a shown answer: it no longer counts as solved
  uint8_t hints = 0;         // 1 names the piece, 2 the square
  bool done = false;
  chess::Move last;  // the move last played, for the board to mark
  bool hasLast = false;
};
void begin(Run& run, const Puzzle& puzzle);
bool whiteSolves(const Run& run);
// The move wanted now, or false when the puzzle is done.
bool wanted(const Run& run, chess::Move& out);

enum class Outcome : uint8_t { NotAMove, Wrong, Right, Solved };
// A move from one square to another. A right one is played, and the
// opponent's answer after it. A different move that mates on the last step is
// right too, as it is on Lichess. A wrong one leaves the board as it was.
Outcome tryMove(Run& run, int from, int to);
// Plays the wanted move and the answer to it: the solution, a move at a time.
// Marks the run missed.
Outcome showMove(Run& run);

// Elo, with the puzzle as the opponent. K is 32 for the first 20 and 20 after.
int ratedAfter(int rating, int puzzleRating, bool solved, int played);

constexpr int kHistory = 48;
constexpr int kRecent = 96;

struct Stats {
  int16_t rating = 1000;
  int16_t peak = 1000;
  uint16_t solved = 0;
  uint16_t missed = 0;
  uint16_t streak = 0;
  uint16_t bestStreak = 0;
  int32_t dailyDay = 0;      // days since 1970 of the last daily played
  uint8_t dailySolved = 0;   // and how it went
  uint16_t dailyStreak = 0;  // days in a row with a daily solved
  uint16_t themeSeen[kThemes] = {};
  uint16_t themeSolved[kThemes] = {};
  int16_t history[kHistory] = {};  // the rating after each of the last ones, oldest first
  uint8_t historyCount = 0;
  uint32_t recent[kRecent] = {};  // ids lately seen, so they do not come straight back
  uint8_t recentAt = 0;
};
uint32_t idKey(const char* id);
bool seenLately(const Stats& stats, const char* id);
// Counts a finished puzzle. `rated` is false for one retried or already done.
void record(Stats& stats, const Puzzle& puzzle, bool solved, bool rated);
void recordDaily(Stats& stats, int32_t day, bool solved);
bool dailyDone(const Stats& stats, int32_t day);

constexpr size_t kStatsBytes = 2 + 2 * 6 + 4 + 1 + 2 + kThemes * 4 + kHistory * 2 + 1 + kRecent * 4 + 1;
size_t packStats(const Stats& stats, uint8_t* out, size_t cap);
bool unpackStats(const uint8_t* in, size_t size, Stats& out);

// What to practise.
enum class Focus : uint8_t { Everything = 0, Mates = 1, Forks = 2, Endgames = 3, Sacrifices = 4 };
constexpr int kFocuses = 5;
const char* focusName(Focus focus);
bool fits(Focus focus, uint32_t themes);

// The first index at or past `rating` in a pack of `count` sorted by rating;
// `ratingAt` reads one record's.
template <typename RatingAt>
uint32_t lowerBound(const uint32_t count, const int rating, RatingAt ratingAt) {
  uint32_t lo = 0;
  uint32_t hi = count;
  while (lo < hi) {
    const uint32_t mid = lo + (hi - lo) / 2;
    if (static_cast<int>(ratingAt(mid)) < rating) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo;
}

uint32_t nextRandom(uint32_t& state);
// The day's puzzle: the same for a given day and pack size, whoever asks.
uint32_t dailyPick(int32_t day, uint32_t first, uint32_t past);

}  // namespace tactics

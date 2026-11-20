#pragma once

// What a game against the device keeps so it can be left and come back to:
// the position, the positions before it for taking a move back, and the
// record against each level. Shared by the board games that had none of it.
//
// A game here is a plain struct of bytes (they all are: they travel over the
// link that way), so the file is the struct itself behind a small header that
// says how big one is. A file from another build, or cut short, does not load.

#include <HalStorage.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace sologame {

// The last `N` positions it was the player's turn in, oldest first.
template <class Game, int N>
struct History {
  Game states[N];
  int count = 0;

  void clear() { count = 0; }
  void push(const Game& game) {
    if (count == N) {
      std::memmove(&states[0], &states[1], sizeof(Game) * static_cast<size_t>(N - 1));
      --count;
    }
    states[count++] = game;
  }
  bool pop(Game& out) {
    if (count == 0) return false;
    out = states[--count];
    return true;
  }
};

struct Header {
  uint8_t magic[2];  // 'S', 'G'
  uint8_t version;
  uint8_t level;
  uint8_t seat;
  uint8_t count;  // positions of history that follow the current one
  uint16_t gameBytes;
} __attribute__((packed));

constexpr uint8_t kVersion = 1;

template <class Game, int N>
bool save(const char* tag, const char* path, const Game& game, const History<Game, N>& history, const uint8_t level,
          const uint8_t seat) {
  static_assert(std::is_trivially_copyable<Game>::value, "a saved game is its own bytes");
  HalFile file;
  if (!Storage.openFileForWrite(tag, path, file)) return false;
  Header header{};
  header.magic[0] = 'S';
  header.magic[1] = 'G';
  header.version = kVersion;
  header.level = level;
  header.seat = seat;
  header.count = static_cast<uint8_t>(history.count);
  header.gameBytes = static_cast<uint16_t>(sizeof(Game));
  file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  file.write(reinterpret_cast<const uint8_t*>(&game), sizeof(Game));
  if (history.count > 0) {
    file.write(reinterpret_cast<const uint8_t*>(&history.states[0]), sizeof(Game) * static_cast<size_t>(history.count));
  }
  file.flush();
  return true;
}

template <class Game, int N>
bool load(const char* tag, const char* path, Game& game, History<Game, N>& history, uint8_t& level, uint8_t& seat) {
  static_assert(std::is_trivially_copyable<Game>::value, "a saved game is its own bytes");
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead(tag, path, file)) return false;
  Header header{};
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != static_cast<int>(sizeof(header))) return false;
  if (header.magic[0] != 'S' || header.magic[1] != 'G' || header.version != kVersion ||
      header.gameBytes != sizeof(Game) || header.count > N) {
    return false;
  }
  Game current;
  if (file.read(reinterpret_cast<uint8_t*>(&current), sizeof(Game)) != static_cast<int>(sizeof(Game))) return false;
  History<Game, N> fresh;
  const int bytes = static_cast<int>(sizeof(Game)) * header.count;
  if (bytes > 0 && file.read(reinterpret_cast<uint8_t*>(&fresh.states[0]), static_cast<size_t>(bytes)) != bytes) {
    return false;
  }
  fresh.count = header.count;
  game = current;
  history = fresh;
  level = header.level;
  seat = header.seat;
  return true;
}

inline void drop(const char* path) {
  if (Storage.exists(path)) Storage.remove(path);
}

// The record against each level of the built-in opponent.
constexpr int kStatLevels = 4;
struct LevelStats {
  uint16_t won[kStatLevels] = {};
  uint16_t lost[kStatLevels] = {};
  uint16_t drawn[kStatLevels] = {};

  // 0 won, 1 lost, 2 drawn.
  void count(const int level, const int outcome) {
    if (level < 0 || level >= kStatLevels) return;
    uint16_t* which = outcome == 0 ? won : (outcome == 1 ? lost : drawn);
    if (which[level] < 0xFFFF) ++which[level];
  }
};

inline void loadStats(const char* tag, const char* path, LevelStats& stats) {
  stats = LevelStats{};
  if (!Storage.exists(path)) return;
  HalFile file;
  LevelStats read;
  if (!Storage.openFileForRead(tag, path, file)) return;
  if (file.read(reinterpret_cast<uint8_t*>(&read), sizeof(read)) == static_cast<int>(sizeof(read))) stats = read;
}

inline void saveStats(const char* tag, const char* path, const LevelStats& stats) {
  HalFile file;
  if (!Storage.openFileForWrite(tag, path, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(&stats), sizeof(stats));
  file.flush();
}

}  // namespace sologame

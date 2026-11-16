#include "MoleCore.h"

namespace mole {

namespace {

uint32_t next(Game& game) {
  // xorshift32
  uint32_t x = game.rng ? game.rng : 0x9E3779B9u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  game.rng = x;
  return x;
}

bool passed(const uint32_t now, const uint32_t since, const uint32_t ms) { return now - since >= ms; }

// Puts somebody in an empty hole, never the one just left when there is a
// choice. False when the meadow is full.
bool pop(Game& game, const uint32_t now) {
  const int n = holeCount(game.config);
  int free[kMaxHoles];
  int count = 0;
  for (int i = 0; i < n; ++i) {
    if (game.holes[i].state == State::Empty && i != game.lastLeft) free[count++] = i;
  }
  if (count == 0 && game.lastLeft >= 0 && game.lastLeft < n && game.holes[game.lastLeft].state == State::Empty) {
    free[count++] = game.lastLeft;
  }
  if (count == 0) return false;
  Hole& hole = game.holes[free[next(game) % static_cast<uint32_t>(count)]];
  hole.state = State::Up;
  hole.animal = game.config.who < kAnimals ? game.config.who : static_cast<uint8_t>(next(game) % kAnimals);
  hole.since = now;
  return true;
}

void fill(Game& game, const uint32_t now) {
  while (out(game) < crowd(game.config) && pop(game, now)) {
  }
}

}  // namespace

int holeCount(const Config& config) { return config.holes == 0 ? 4 : (config.holes == 1 ? 6 : 9); }
int columns(const Config& config) { return config.holes == 2 ? 3 : 2; }
int rows(const Config& config) { return config.holes == 0 ? 2 : 3; }
int crowd(const Config& config) { return config.holes == 0 ? 1 : 2; }

const char* holesName(const Config& config) { return config.holes == 0 ? "4" : (config.holes == 1 ? "6" : "9"); }

const char* whoName(const Config& config) {
  switch (config.who) {
    case 0:
      return "TALP";
    case 1:
      return "CONILL";
    case 2:
      return "GRANOTA";
    case 3:
      return "POLLET";
    default:
      return "TOTS";
  }
}

const char* hidesName(const Config& config) {
  constexpr const char* kNames[kHideChoices] = {"MAI", "6 S", "4 S", "3 S", "2 S"};
  return kNames[config.hides < kHideChoices ? config.hides : 0];
}

uint32_t stayMs(const Config& config) {
  constexpr uint32_t kMs[kHideChoices] = {0, 6000, 4000, 3000, 2000};
  return kMs[config.hides < kHideChoices ? config.hides : 0];
}

int faceOf(const Config& config) { return config.who < kAnimals ? config.who : 0; }

int out(const Game& game) {
  int n = 0;
  for (int i = 0; i < holeCount(game.config); ++i) n += game.holes[i].state != State::Empty ? 1 : 0;
  return n;
}

void start(Game& game, const Config& config, const uint32_t seed, const uint32_t now) {
  game = Game{};
  game.config = config;
  game.rng = seed ? seed : 1;
  fill(game, now);
}

bool tap(Game& game, const int hole, const uint32_t now) {
  if (hole < 0 || hole >= holeCount(game.config)) return false;
  Hole& h = game.holes[hole];
  if (h.state != State::Up) return false;
  h.state = State::Bopped;
  h.since = now;
  ++game.stars;
  if (game.stars % kPartyEvery == 0) {
    game.party = true;
    game.partyAnimal = h.animal;
  }
  return true;
}

bool tick(Game& game, const uint32_t now) {
  bool changed = false;
  const int n = holeCount(game.config);
  const uint32_t stay = stayMs(game.config);
  for (int i = 0; i < n; ++i) {
    Hole& h = game.holes[i];
    const bool leaves = (h.state == State::Bopped && passed(now, h.since, kBoppedMs)) ||
                        (h.state == State::Up && stay != 0 && passed(now, h.since, stay));
    if (!leaves) continue;
    h.state = State::Empty;
    game.lastLeft = static_cast<int8_t>(i);
    changed = true;
    // Its place is taken at once, so the meadow is repainted once, not twice.
    fill(game, now);
  }
  return changed;
}

void pack(const Config& config, uint8_t* bytes) {
  bytes[0] = 2;
  bytes[1] = config.holes;
  bytes[2] = config.who;
  bytes[3] = config.hides;
  bytes[4] = config.sound;
}

bool unpack(const uint8_t* bytes, const int size, Config& config) {
  if (size != kConfigBytes || bytes[0] != 2) return false;
  if (bytes[1] >= kHoleChoices || bytes[2] >= kWhoChoices || bytes[3] >= kHideChoices || bytes[4] > 1) return false;
  config.holes = bytes[1];
  config.who = bytes[2];
  config.hides = bytes[3];
  config.sound = bytes[4];
  return true;
}

}  // namespace mole

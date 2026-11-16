#pragma once

// EL TALP: somebody pops out of a hole, you bop them, they see stars, and
// somebody pops out of another. Nothing is timed against the player and
// nothing can be missed: an animal waits, or wanders to another hole, until
// a finger finds it. Freestanding, so host-tests/kids can play it.

#include <cstdint>

namespace mole {

constexpr int kMaxHoles = 9;
constexpr int kAnimals = 4;  // mole, rabbit, frog, chick: the order of the pictures

// What a grown-up chooses before play. Each is an index that steps round.
struct Config {
  uint8_t holes = 0;  // 0: four, 1: six, 2: nine
  uint8_t who = 0;    // 0..3 one animal, 4 all of them
  uint8_t hides = 0;  // 0: they wait for ever; otherwise how soon they move on
  uint8_t sound = 1;
};
constexpr int kHoleChoices = 3;
constexpr int kWhoChoices = kAnimals + 1;
constexpr int kHideChoices = 5;  // never, then 6, 4, 3 and 2 seconds

int holeCount(const Config& config);
int columns(const Config& config);
int rows(const Config& config);
// How many are out at once: one on the small meadow, two on the others.
int crowd(const Config& config);
const char* holesName(const Config& config);
const char* whoName(const Config& config);  // Catalan, capitals, ASCII
const char* hidesName(const Config& config);
// How long somebody stays out before moving to another hole; 0 for ever. The
// shortest is two seconds: the panel needs most of one to draw them.
uint32_t stayMs(const Config& config);
// The animal that stands for the choice on the setup page.
int faceOf(const Config& config);

constexpr uint32_t kBoppedMs = 1100;  // how long the stars go round
constexpr int kPartyEvery = 10;       // stars between celebrations

enum class State : uint8_t { Empty, Up, Bopped };

struct Hole {
  State state = State::Empty;
  uint8_t animal = 0;
  uint32_t since = 0;
};

struct Game {
  Config config;
  Hole holes[kMaxHoles];
  int stars = 0;
  bool party = false;  // a celebration is owed; the caller shows it and clears it
  uint8_t partyAnimal = 0;
  uint32_t rng = 1;
  int8_t lastLeft = -1;  // the hole most recently vacated, not reused at once
};

void start(Game& game, const Config& config, uint32_t seed, uint32_t now);
// A finger on a hole. True when it bopped somebody.
bool tap(Game& game, int hole, uint32_t now);
// Time passing. True when the meadow changed and wants painting.
bool tick(Game& game, uint32_t now);
int out(const Game& game);  // how many holes are not empty

// The setup page's choices as bytes for the card, and back.
constexpr int kConfigBytes = 5;
void pack(const Config& config, uint8_t* bytes);
bool unpack(const uint8_t* bytes, int size, Config& config);

}  // namespace mole

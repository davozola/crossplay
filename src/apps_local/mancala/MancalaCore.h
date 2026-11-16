#pragma once

// MANCALA: Kalah. Two rows of six pits, a store each, and seeds sown one at a
// time round the board. Freestanding: the rules and the machine that plays
// them, so host-tests/mancala can play whole games.
//
// The board is fourteen slots going the way the seeds go: South's six pits,
// South's store, North's six pits, North's store.

#include <cstdint>

namespace mancala {

constexpr int kPits = 6;
constexpr int kSlots = 14;
constexpr int kSouthStore = 6;
constexpr int kNorthStore = 13;

enum : uint8_t { South = 0, North = 1 };

struct Board {
  uint8_t slot[kSlots] = {};
  uint8_t turn = South;
};

void setup(Board& board, int seedsPerPit, uint8_t first);

int storeOf(uint8_t side);  // the slot
int sideOf(int slot);       // whose pit or store it is
bool isPit(int slot);
int opposite(int pit);                     // the pit across the board
bool legal(const Board& board, int slot);  // a pit of the side to move, with seeds in it
bool over(const Board& board);             // one row is empty: nothing left to play
int seedsOnBoard(const Board& board);      // all fourteen slots: never changes in a game

struct Outcome {
  int8_t last = -1;      // where the last seed fell
  bool again = false;    // it fell in the mover's store: they move again
  uint8_t captured = 0;  // seeds taken to the store by a capture, the capturing seed included
  bool ended = false;    // the game is over; what was left has gone to its owners' stores
};

// Sows `slot`, which must be legal. The turn passes unless `again`.
Outcome play(Board& board, int slot);

// The machine's move for the side to move: a legal pit, or -1 when there is
// none. `depth` is how many moves ahead it looks; `rng` breaks ties, so two
// games do not go the same way.
int bestMove(const Board& board, int depth, uint32_t& rng);

// What a grown-up chooses before a game. Each is an index that steps round.
struct Config {
  uint8_t friendly = 0;  // 0: against the machine, 1: two players
  uint8_t level = 1;     // 0 easy, 1 medium, 2 hard
  uint8_t seeds = 1;     // 0: three a pit, 1: four, 2: five, 3: six
  uint8_t first = 0;     // 0: South (you), 1: North
};
constexpr int kLevels = 3;
constexpr int kSeedChoices = 4;
int seedsOf(const Config& config);
int depthOf(const Config& config);
const char* levelName(const Config& config);

// A game on the card: the choices and the board as it stands.
constexpr int kSaveBytes = 1 + 4 + kSlots + 2;
void pack(const Config& config, const Board& board, bool inProgress, uint8_t* bytes);
bool unpack(const uint8_t* bytes, int size, Config& config, Board& board, bool& inProgress);

}  // namespace mancala

#include "MancalaCore.h"

namespace mancala {

namespace {

bool rowEmpty(const Board& board, const uint8_t side) {
  const int first = side == South ? 0 : 7;
  for (int i = first; i < first + kPits; ++i) {
    if (board.slot[i] != 0) return false;
  }
  return true;
}

// What is left on the board goes to the store of whoever's row it is in.
void sweep(Board& board) {
  for (int i = 0; i < kPits; ++i) {
    board.slot[kSouthStore] = static_cast<uint8_t>(board.slot[kSouthStore] + board.slot[i]);
    board.slot[i] = 0;
    board.slot[kNorthStore] = static_cast<uint8_t>(board.slot[kNorthStore] + board.slot[7 + i]);
    board.slot[7 + i] = 0;
  }
}

uint32_t next(uint32_t& rng) {
  uint32_t x = rng ? rng : 0x9E3779B9u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng = x;
  return x;
}

// The legal pits of the side to move, the ones that earn another move first:
// they are the likeliest to be best, which is what lets the search cut.
int ordered(const Board& board, int* out) {
  const int first = board.turn == South ? 0 : 7;
  const int store = storeOf(board.turn);
  int n = 0;
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = first + kPits - 1; i >= first; --i) {
      if (board.slot[i] == 0) continue;
      const bool again = board.slot[i] % 13 == store - i;
      if (again == (pass == 0)) out[n++] = i;
    }
  }
  return n;
}

// The lead `me` holds, looking `depth` moves on. A move that earns another
// does not use up depth for the other side: it is the same player's turn.
int search(const Board& board, const int depth, int alpha, int beta, const uint8_t me) {
  if (depth <= 0 || over(board)) {
    return static_cast<int>(board.slot[storeOf(me)]) - static_cast<int>(board.slot[storeOf(me ^ 1)]);
  }
  int moves[kPits];
  const int n = ordered(board, moves);
  const bool mine = board.turn == me;
  int best = mine ? -1000 : 1000;
  for (int k = 0; k < n; ++k) {
    Board after = board;
    play(after, moves[k]);
    const int value = search(after, depth - 1, alpha, beta, me);
    if (mine) {
      if (value > best) best = value;
      if (best > alpha) alpha = best;
    } else {
      if (value < best) best = value;
      if (best < beta) beta = best;
    }
    if (alpha >= beta) break;
  }
  return best;
}

}  // namespace

void setup(Board& board, const int seedsPerPit, const uint8_t first) {
  for (int i = 0; i < kSlots; ++i) board.slot[i] = static_cast<uint8_t>(seedsPerPit);
  board.slot[kSouthStore] = 0;
  board.slot[kNorthStore] = 0;
  board.turn = first == North ? North : South;
}

int storeOf(const uint8_t side) { return side == South ? kSouthStore : kNorthStore; }
int sideOf(const int slot) { return slot <= kSouthStore ? South : North; }
bool isPit(const int slot) { return slot >= 0 && slot < kSlots && slot != kSouthStore && slot != kNorthStore; }
int opposite(const int pit) { return 12 - pit; }

bool legal(const Board& board, const int slot) {
  return isPit(slot) && sideOf(slot) == board.turn && board.slot[slot] > 0 && !over(board);
}

bool over(const Board& board) { return rowEmpty(board, South) || rowEmpty(board, North); }

int seedsOnBoard(const Board& board) {
  int n = 0;
  for (int i = 0; i < kSlots; ++i) n += board.slot[i];
  return n;
}

Outcome play(Board& board, const int slot) {
  Outcome out;
  const uint8_t me = board.turn;
  const int mine = storeOf(me);
  const int theirs = storeOf(me ^ 1);
  int seeds = board.slot[slot];
  board.slot[slot] = 0;
  int at = slot;
  while (seeds > 0) {
    at = (at + 1) % kSlots;
    if (at == theirs) continue;
    ++board.slot[at];
    --seeds;
  }
  out.last = static_cast<int8_t>(at);
  if (at == mine) {
    out.again = true;
  } else if (sideOf(at) == me && board.slot[at] == 1 && board.slot[opposite(at)] > 0) {
    out.captured = static_cast<uint8_t>(board.slot[opposite(at)] + 1);
    board.slot[mine] = static_cast<uint8_t>(board.slot[mine] + out.captured);
    board.slot[at] = 0;
    board.slot[opposite(at)] = 0;
  }
  if (over(board)) {
    sweep(board);
    out.ended = true;
    out.again = false;
  }
  if (!out.again) board.turn = me ^ 1;
  return out;
}

int bestMove(const Board& board, const int depth, uint32_t& rng) {
  if (over(board)) return -1;
  int moves[kPits];
  const int n = ordered(board, moves);
  if (n == 0) return -1;
  int best = -1000;
  int picks[kPits];
  int count = 0;
  for (int k = 0; k < n; ++k) {
    Board after = board;
    play(after, moves[k]);
    // A full window for every root move, so equal moves are seen to be equal.
    const int value = search(after, depth - 1, -1000, 1000, board.turn);
    if (value > best) {
      best = value;
      count = 0;
    }
    if (value == best) picks[count++] = moves[k];
  }
  return picks[next(rng) % static_cast<uint32_t>(count)];
}

int seedsOf(const Config& config) { return 3 + (config.seeds < kSeedChoices ? config.seeds : 1); }

int depthOf(const Config& config) { return config.level == 0 ? 1 : (config.level == 1 ? 4 : 8); }

const char* levelName(const Config& config) {
  return config.level == 0 ? "EASY" : (config.level == 1 ? "MEDIUM" : "HARD");
}

void pack(const Config& config, const Board& board, const bool inProgress, uint8_t* bytes) {
  bytes[0] = 1;
  bytes[1] = config.friendly;
  bytes[2] = config.level;
  bytes[3] = config.seeds;
  bytes[4] = config.first;
  for (int i = 0; i < kSlots; ++i) bytes[5 + i] = board.slot[i];
  bytes[5 + kSlots] = board.turn;
  bytes[6 + kSlots] = inProgress ? 1 : 0;
}

bool unpack(const uint8_t* bytes, const int size, Config& config, Board& board, bool& inProgress) {
  if (size != kSaveBytes || bytes[0] != 1) return false;
  if (bytes[1] > 1 || bytes[2] >= kLevels || bytes[3] >= kSeedChoices || bytes[4] > 1) return false;
  if (bytes[5 + kSlots] > 1 || bytes[6 + kSlots] > 1) return false;
  Config c;
  c.friendly = bytes[1];
  c.level = bytes[2];
  c.seeds = bytes[3];
  c.first = bytes[4];
  Board b;
  for (int i = 0; i < kSlots; ++i) b.slot[i] = bytes[5 + i];
  b.turn = bytes[5 + kSlots];
  // A board in play holds exactly the seeds its game began with.
  const bool playing = bytes[6 + kSlots] == 1;
  if (playing && seedsOnBoard(b) != seedsOf(c) * kPits * 2) return false;
  config = c;
  board = b;
  inProgress = playing && !over(b);
  return true;
}

}  // namespace mancala

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "MancalaCore.h"

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

using namespace mancala;

static uint32_t rnd(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

static int randomMove(const Board& b, uint32_t& s) {
  int legalPits[kPits];
  int n = 0;
  for (int i = 0; i < kSlots; ++i) {
    if (legal(b, i)) legalPits[n++] = i;
  }
  return n ? legalPits[rnd(s) % static_cast<uint32_t>(n)] : -1;
}

int main() {
  {  // the opening: 4 a pit, and the third pit's last seed lands in the store
    Board b;
    setup(b, 4, South);
    CHECK(seedsOnBoard(b) == 48 && b.slot[kSouthStore] == 0 && b.turn == South);
    CHECK(legal(b, 0) && !legal(b, 7) && !legal(b, kSouthStore) && !legal(b, -1) && !legal(b, 14));
    Outcome o = play(b, 2);
    CHECK(o.last == kSouthStore && o.again && b.turn == South && b.slot[2] == 0 && b.slot[3] == 5 && b.slot[6] == 1);
    // And then the far pit: into the store and four of the other side's pits.
    o = play(b, 5);
    CHECK(!o.again && o.last == 10 && b.slot[6] == 2 && b.slot[7] == 5 && b.slot[10] == 5 && b.turn == North);
    CHECK(seedsOnBoard(b) == 48);
  }
  {  // a lap of the board skips the other side's store
    Board b;
    b.slot[5] = 10;
    b.slot[7] = 1;  // so the game is not over
    b.turn = South;
    Outcome o = play(b, 5);
    // Ten seeds from the last pit come round to the third, which was empty and
    // faces a seed just sown: a capture of two on top of the one in the store.
    CHECK(b.slot[kNorthStore] == 0 && o.last == 2 && b.slot[0] == 1 && b.slot[1] == 1 && o.captured == 2 &&
          b.slot[kSouthStore] == 3);
    Board n;
    n.slot[12] = 9;
    n.slot[0] = 1;
    n.turn = North;
    o = play(n, 12);
    CHECK(n.slot[kSouthStore] == 0 && o.last == 8 && o.captured == 2 && n.slot[kNorthStore] == 3);
  }
  {  // a capture: the last seed in an empty pit of one's own, with seeds across
    Board b;
    b.slot[0] = 2;
    b.slot[2] = 0;
    b.slot[opposite(2)] = 5;
    b.slot[4] = 1;
    b.slot[8] = 1;
    b.turn = South;
    const Outcome o = play(b, 0);
    CHECK(o.last == 2 && o.captured == 6 && b.slot[kSouthStore] == 6 && b.slot[2] == 0 && b.slot[10] == 0);
    CHECK(b.turn == North && !o.again);
  }
  {  // no capture when the pit across is empty, or on the other side's row
    Board b;
    b.slot[0] = 2;
    b.slot[4] = 1;
    b.slot[8] = 1;
    b.turn = South;
    Outcome o = play(b, 0);
    CHECK(o.captured == 0 && b.slot[2] == 1 && b.slot[kSouthStore] == 0);
    Board c;
    c.slot[5] = 2;
    c.slot[0] = 3;
    c.slot[9] = 4;
    c.turn = South;
    o = play(c, 5);
    CHECK(o.last == 7 && o.captured == 0 && c.slot[7] == 1);
  }
  {  // the end: a row empties, and what is left goes to its owner
    Board b;
    b.slot[5] = 1;
    b.slot[8] = 3;
    b.slot[11] = 2;
    b.slot[kSouthStore] = 20;
    b.slot[kNorthStore] = 22;
    b.turn = South;
    const Outcome o = play(b, 5);
    CHECK(o.ended && !o.again && over(b));
    CHECK(b.slot[kSouthStore] == 21 && b.slot[kNorthStore] == 27 && b.slot[8] == 0);
    CHECK(!legal(b, 8) && seedsOnBoard(b) == 48);
    uint32_t s = 5;
    CHECK(bestMove(b, 4, s) == -1);
  }
  {  // random games: seeds are conserved, every game ends, the machine's moves are legal
    uint32_t s = 2463534242u;
    for (int game = 0; game < 300; ++game) {
      Board b;
      const int seeds = 3 + game % 4;
      setup(b, seeds, static_cast<uint8_t>(game % 2));
      int moves = 0;
      while (!over(b) && moves < 600) {
        const int m = game % 3 == 0 ? bestMove(b, 2, s) : randomMove(b, s);
        CHECK(legal(b, m));
        if (!legal(b, m)) break;
        const uint8_t mover = b.turn;
        const Outcome o = play(b, m);
        CHECK(seedsOnBoard(b) == seeds * 12);
        CHECK(o.again == (b.turn == mover) || o.ended);
        ++moves;
      }
      CHECK(over(b));
      CHECK(b.slot[kSouthStore] + b.slot[kNorthStore] == seeds * 12);
    }
  }
  {  // looking ahead pays: each level beats random play, and hard beats easy
    uint32_t s = 88172645u;
    for (int depth : {1, 4, 8}) {
      int won = 0;
      const int games = depth == 8 ? 12 : 40;
      for (int game = 0; game < games; ++game) {
        Board b;
        setup(b, 4, static_cast<uint8_t>(game % 2));
        while (!over(b)) play(b, b.turn == North ? bestMove(b, depth, s) : randomMove(b, s));
        won += b.slot[kNorthStore] > b.slot[kSouthStore] ? 1 : 0;
      }
      CHECK(won * 10 >= games * (depth == 1 ? 6 : 9));
    }
    int hard = 0;
    for (int game = 0; game < 12; ++game) {
      Board b;
      setup(b, 4, static_cast<uint8_t>(game % 2));
      while (!over(b)) play(b, bestMove(b, b.turn == North ? 8 : 1, s));
      hard += b.slot[kNorthStore] > b.slot[kSouthStore] ? 1 : 0;
    }
    CHECK(hard >= 10);
  }
  {  // the machine takes a free move and a capture when it sees one
    Board b;
    setup(b, 4, North);
    uint32_t s = 1;
    CHECK(bestMove(b, 2, s) == 9);  // four seeds, four steps from the store
    Board c;
    c.slot[7] = 1;
    c.slot[8] = 0;
    c.slot[opposite(8)] = 9;
    c.slot[12] = 2;
    c.slot[0] = 2;
    c.turn = North;
    CHECK(bestMove(c, 1, s) == 7);
  }
  {  // a game on the card
    Config c;
    c.friendly = 1;
    c.level = 2;
    c.seeds = 3;
    c.first = 1;
    Board b;
    setup(b, seedsOf(c), North);
    play(b, 9);
    uint8_t bytes[kSaveBytes];
    pack(c, b, true, bytes);
    Config c2;
    Board b2;
    bool playing = false;
    CHECK(unpack(bytes, kSaveBytes, c2, b2, playing) && playing && c2.seeds == 3 && c2.level == 2 &&
          std::memcmp(b.slot, b2.slot, kSlots) == 0 && b2.turn == b.turn);
    CHECK(seedsOf(c) == 6 && depthOf(c) == 8);
    bytes[5] = 99;  // seeds from nowhere
    CHECK(!unpack(bytes, kSaveBytes, c2, b2, playing));
    pack(c, b, false, bytes);
    CHECK(unpack(bytes, kSaveBytes, c2, b2, playing) && !playing);
    CHECK(!unpack(bytes, kSaveBytes - 1, c2, b2, playing));
    bytes[2] = 3;
    CHECK(!unpack(bytes, kSaveBytes, c2, b2, playing));
  }
  if (failures == 0) std::printf("mancala: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

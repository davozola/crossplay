#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "TakCore.h"
#include "TakEngine.h"
#include "TakFlow.h"

using namespace tak;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                 \
    }                                                             \
  } while (0)

static MoveList lists[8];

static uint64_t perft(const Game& g, const int depth) {
  if (depth == 0) return 1;
  MoveList& list = lists[depth];
  generate(g, list);
  if (depth == 1) return static_cast<uint64_t>(list.count);
  uint64_t total = 0;
  for (int i = 0; i < list.count; ++i) {
    Game next = g;
    apply(next, list.moves[i]);
    total += perft(next, depth - 1);
    generate(g, list);  // the child overwrote nothing at this depth, but keep it honest
  }
  return total;
}

static bool play(Game& g, const char* const* moves, const int n) {
  for (int i = 0; i < n; ++i) {
    Move m;
    if (!fromPtn(g, moves[i], m)) {
      std::printf("bad ptn %s\n", moves[i]);
      return false;
    }
    if (!isLegal(g, m)) {
      std::printf("illegal %s at ply %d\n", moves[i], g.ply);
      return false;
    }
    apply(g, m);
  }
  return true;
}

// TPS, as playtak and Tiltak write positions.
static bool fromTps(Game& g, const char* tps) {
  int size = 1;
  for (const char* p = tps; *p && *p != ' '; ++p)
    if (*p == '/') ++size;
  reset(g, size);
  int row = 0, col = 0;
  int used[2] = {0, 0}, capsUsed[2] = {0, 0};
  const char* p = tps;
  while (*p && *p != ' ') {
    if (*p == '/') {
      ++row;
      col = 0;
      ++p;
      continue;
    }
    if (*p == ',') {
      ++p;
      continue;
    }
    if (*p == 'x') {
      ++p;
      int n = 1;
      if (*p >= '1' && *p <= '9') n = *p++ - '0';
      col += n;
      continue;
    }
    Square& s = g.sq[row * size + col];
    while (*p == '1' || *p == '2') {
      const int c = *p - '1';
      s.colors |= static_cast<uint64_t>(c) << s.height;
      ++s.height;
      ++used[c];
      ++p;
    }
    if (*p == 'S') {
      s.top = Stone::Wall;
      ++p;
    } else if (*p == 'C') {
      s.top = Stone::Cap;
      const int c = static_cast<int>(topColor(s));
      --used[c];
      ++capsUsed[c];
      ++p;
    }
    ++col;
  }
  ++p;
  const int side = *p - '1';
  p += 2;
  const int moveNo = std::atoi(p);
  g.turn = static_cast<uint8_t>(side);
  g.ply = static_cast<uint16_t>((moveNo - 1) * 2 + side);
  for (int c = 0; c < 2; ++c) {
    g.stones[c] = static_cast<uint8_t>(stonesFor(size) - used[c]);
    g.caps[c] = static_cast<uint8_t>(capsFor(size) - capsUsed[c]);
  }
  return true;
}

static void perftStart() {
  Game g;
  reset(g, 5);
  const uint64_t want5[] = {1, 25, 600, 43320, 2999784};
  for (int d = 0; d < 5; ++d) CHECK(perft(g, d) == want5[d]);
  reset(g, 6);
  const uint64_t want6[] = {1, 36, 1260, 132720};
  for (int d = 0; d < 4; ++d) CHECK(perft(g, d) == want6[d]);
}

static void perftMidgame() {
  {
    Game g;
    reset(g, 5);
    const char* m[] = {"d3", "c3", "c4", "1d3<", "1c4-", "Sc4"};
    CHECK(play(g, m, 6));
    const uint64_t want[] = {1, 87, 6155, 461800};
    for (int d = 0; d < 4; ++d) CHECK(perft(g, d) == want[d]);
  }
  {
    Game g;
    reset(g, 5);
    const char* m[] = {"c2", "c3", "d3", "b3", "c4", "1c2+", "1d3<", "1b3>", "1c4-", "Cc2", "a1", "1c2+", "a2"};
    CHECK(play(g, m, 13));
    const uint64_t want[] = {1, 104, 7743, 592645};
    for (int d = 0; d < 4; ++d) CHECK(perft(g, d) == want[d]);
  }
  {
    Game g;
    reset(g, 5);
    const char* m[] = {"c4",   "c2",   "d2",   "c3",   "b2",     "d3",   "1d2+", "b3",   "d2",     "b4",     "1c2+",
                       "1b3>", "2d3<", "1c4-", "d4",   "5c3<23", "c2",   "c4",   "1d4<", "d3",     "1d2+",   "1c3+",
                       "Cc3",  "2c4>", "1c3<", "d2",   "c3",     "1d2+", "1c3+", "1b4>", "2b3>11", "3c4-12", "d2",
                       "c4",   "b4",   "c5",   "1b3>", "1c4<",   "3c3-", "e5",   "e2"};
    CHECK(play(g, m, static_cast<int>(sizeof(m) / sizeof(m[0]))));
    const uint64_t want[] = {1, 85, 11204, 956736};
    for (int d = 0; d < 4; ++d) CHECK(perft(g, d) == want[d]);
  }
  {
    Game g;
    fromTps(
        g,
        "2,2,21S,2,2,2/2,x,222221,2,2,x/1,1,2221C,x,111112C,2S/x,1,2S,x2,121211212/1,1,1212S,1S,2,1S/x2,2,1,21,1 1 42");
    const uint64_t want[] = {1, 140, 21402, 2774593};
    for (int d = 0; d < 4; ++d) CHECK(perft(g, d) == want[d]);
  }
}

static void openingPlacesOpponentsFlat() {
  Game g;
  reset(g, 5);
  CHECK(!isLegal(g, place(0, Stone::Wall)));
  CHECK(!isLegal(g, place(0, Stone::Cap)));
  apply(g, place(0, Stone::Flat));
  CHECK(topColor(g.sq[0]) == kBlack);
  CHECK(g.stones[kBlack] == 20 && g.stones[kWhite] == 21);
  apply(g, place(24, Stone::Flat));
  CHECK(topColor(g.sq[24]) == kWhite);
  CHECK(g.turn == kWhite);
  apply(g, place(12, Stone::Cap));
  CHECK(g.caps[kWhite] == 0 && g.sq[12].top == Stone::Cap);
  CHECK(!isLegal(g, place(13, Stone::Cap)) == false);  // black still has its cap
}

static void capFlattensWall() {
  Game g;
  reset(g, 5);
  const char* m[] = {"a1", "e5", "Cc3", "Sd3"};
  CHECK(play(g, m, 4));
  Move smash;
  CHECK(fromPtn(g, "c3>", smash));
  CHECK(isLegal(g, smash));
  apply(g, smash);
  CHECK(g.sq[13].height == 2 && g.sq[13].top == Stone::Cap);
  CHECK(topColor(g.sq[13]) == kWhite);
  // A flat cannot do it.
  Game h;
  reset(h, 5);
  const char* n[] = {"a1", "e5", "c3", "Sd3"};
  CHECK(play(h, n, 4));
  Move blocked;
  CHECK(fromPtn(h, "c3>", blocked));
  CHECK(!isLegal(h, blocked));
}

static void roadsAndDragonClause() {
  Game g;
  reset(g, 4);
  // White builds the top row while black dawdles.
  const char* m[] = {"d1", "a1", "a4", "c1", "b4", "b1", "c4"};
  CHECK(play(g, m, 7));
  CHECK(!over(g.result));
  const char* last[] = {"a2", "d4"};
  CHECK(play(g, last, 2));
  CHECK(g.result == Result::WhiteRoad);
  // The top row, and not the white flat on a1 that touches only one edge.
  CHECK(roadSquares(g, kWhite) == 0xFull);
  // Walls do not make a road.
  Game h;
  reset(h, 4);
  const char* w[] = {"d1", "a1", "a4", "c1", "b4", "b1", "c4", "a2", "Sd4"};
  CHECK(play(h, w, 9));
  CHECK(!over(h.result));
}

static void flatWinWhenFull() {
  // A checkerboard has no road for either side. One square short of full, with
  // one black wall, so the last flat fills the board and white is ahead 8-7.
  Game g;
  fromTps(g, "1,2,1,2/2,1,2S,1/1,2,1,2/2,1,2,x 1 9");
  CHECK(!over(g.result));
  apply(g, place(15, Stone::Flat));
  CHECK(g.result == Result::WhiteFlats);
  // Without the wall it is level.
  Game h;
  fromTps(h, "1,2,1,2/2,1,2,1/1,2,1,2/2,1,2,x 1 9");
  apply(h, place(15, Stone::Flat));
  CHECK(h.result == Result::Draw);
}

static void ptnRoundTrip() {
  Game g;
  reset(g, 6);
  const char* texts[] = {"a1", "Sf6", "Cc3", "3c3>12", "c3<", "6a1+222"};
  for (const char* t : texts) {
    Move m;
    CHECK(fromPtn(g, t, m));
    char back[16];
    toPtn(g, m, back, sizeof(back));
    Move again;
    CHECK(fromPtn(g, back, again));
    CHECK(again == m);
  }
  Move bad;
  CHECK(!fromPtn(g, "g1", bad));
  CHECK(!fromPtn(g, "3c3>11", bad));
}

static SearchBuffers buffers;

static void engineTakesRoad() {
  // White has a1 b1 c1 on 4x4; d1 wins.
  Game g;
  fromTps(g, "x4/x4/2,2,x2/1,1,1,x 1 4");
  for (Level lv : {Level::Easy, Level::Medium, Level::Hard}) {
    const SearchResult r = search(g, buffers, limitsFor(lv, 7));
    CHECK(r.found);
    Game next = g;
    apply(next, r.best);
    CHECK(next.result == Result::WhiteRoad);
  }
}

static void engineBlocksRoad() {
  // Black to move; white threatens d1. Medium and Hard must stop it.
  Game g;
  fromTps(g, "x4/x4/2,x3/1,1,1,x 2 4");
  for (Level lv : {Level::Medium, Level::Hard}) {
    const SearchResult r = search(g, buffers, limitsFor(lv, 7));
    Game next = g;
    apply(next, r.best);
    MoveList& replies = buffers.plies[0];
    generate(next, replies);
    bool whiteWins = false;
    for (int i = 0; i < replies.count; ++i) {
      Game after = next;
      apply(after, replies.moves[i]);
      if (after.result == Result::WhiteRoad) whiteWins = true;
    }
    char ptn[12];
    toPtn(g, r.best, ptn, sizeof(ptn));
    if (whiteWins) std::printf("  level %d left the road open with %s\n", static_cast<int>(lv), ptn);
    CHECK(!whiteWins);
  }
}

static void engineRespectsBudget() {
  Game g;
  reset(g, 6);
  const char* m[] = {"a1", "f6", "c3", "d4", "c4", "d3", "Cc2", "Cd5", "b3", "e4"};
  CHECK(play(g, m, 10));
  Limits l = limitsFor(Level::Hard, 3);
  l.nodeBudget = 5000;
  const SearchResult r = search(g, buffers, l);
  CHECK(r.found);
  // The budget is checked between moves, so it can be passed by at most one
  // node's worth of win-scan trials.
  CHECK(r.nodes <= 5000 + 600);
  const SearchResult again = search(g, buffers, l);
  CHECK(again.best == r.best);
}

// Plays a whole game between two levels; returns the winner or 0xFF.
static uint8_t match(const int size, const Level white, const Level black, const uint32_t seed, double& worstMs,
                     uint32_t& worstNodes) {
  Game g;
  reset(g, size);
  uint32_t s = seed;
  while (!over(g.result) && g.ply < 400) {
    const Level lv = g.turn == kWhite ? white : black;
    const auto t0 = std::chrono::steady_clock::now();
    const SearchResult r = search(g, buffers, limitsFor(lv, s += 7919));
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (ms > worstMs) worstMs = ms;
    if (r.nodes > worstNodes) worstNodes = r.nodes;
    if (!r.found || !isLegal(g, r.best)) {
      ++failures;
      std::printf("FAIL engine produced an illegal move at ply %d\n", g.ply);
      return 0xFE;
    }
    apply(g, r.best);
  }
  return winner(g.result);
}

static void selfPlay() {
  for (int size = 4; size <= 6; ++size) {
    int hardWins = 0, mediumWins = 0, games = 0;
    double worstMs = 0;
    uint32_t worstNodes = 0;
    for (int i = 0; i < 4; ++i) {
      const bool hardWhite = (i % 2) == 0;
      const uint8_t w = match(size, hardWhite ? Level::Hard : Level::Easy, hardWhite ? Level::Easy : Level::Hard,
                              100 + i, worstMs, worstNodes);
      if (w == (hardWhite ? kWhite : kBlack)) ++hardWins;
      const uint8_t w2 = match(size, hardWhite ? Level::Medium : Level::Easy, hardWhite ? Level::Easy : Level::Medium,
                               200 + i, worstMs, worstNodes);
      if (w2 == (hardWhite ? kWhite : kBlack)) ++mediumWins;
      ++games;
    }
    std::printf("  %dx%d: hard beat easy %d/%d, medium beat easy %d/%d, slowest move %.0f ms, most nodes %u\n", size,
                size, hardWins, games, mediumWins, games, worstMs, worstNodes);
    CHECK(hardWins >= 3);
    CHECK(mediumWins >= 3);
  }
}

static int sq(const Game& g, const char* name) { return (g.size - (name[1] - '0')) * g.size + (name[0] - 'a'); }

static void handBuildsSlides() {
  // White owns a 3-stack on c3 (5x5).
  Game g;
  fromTps(g, "x5/x5/x2,121,x2/x5/x5 1 6");
  Hand hand;
  Move move;
  CHECK(tapSquare(g, hand, sq(g, "c3"), Stone::Flat, move) == TapResult::Changed);
  CHECK(hand.holding && hand.pick == 3);
  CHECK(tapSquare(g, hand, sq(g, "d3"), Stone::Flat, move) == TapResult::Changed);
  // Not in line: ignored.
  CHECK(tapSquare(g, hand, sq(g, "d4"), Stone::Flat, move) == TapResult::Ignored);
  CHECK(tapSquare(g, hand, sq(g, "e3"), Stone::Flat, move) == TapResult::Changed);
  CHECK(tapSquare(g, hand, sq(g, "e3"), Stone::Flat, move) == TapResult::Played);
  Move want;
  CHECK(fromPtn(g, "3c3>12", want));
  CHECK(move == want);
  CHECK(!hand.holding);

  // Putting pieces back, then a single.
  Hand h2;
  CHECK(tapSquare(g, h2, sq(g, "c3"), Stone::Flat, move) == TapResult::Changed);
  CHECK(tapSquare(g, h2, sq(g, "c3"), Stone::Flat, move) == TapResult::Changed);
  CHECK(tapSquare(g, h2, sq(g, "c3"), Stone::Flat, move) == TapResult::Changed);
  CHECK(h2.pick == 1);
  CHECK(tapSquare(g, h2, sq(g, "c2"), Stone::Flat, move) == TapResult::Played);
  CHECK(fromPtn(g, "c3-", want) && move == want);
  // ...and all the way back cancels.
  Hand h3;
  tapSquare(g, h3, sq(g, "c3"), Stone::Flat, move);
  for (int i = 0; i < 3; ++i) tapSquare(g, h3, sq(g, "c3"), Stone::Flat, move);
  CHECK(!h3.holding);

  // Preview shows the slide in flight.
  Hand h4;
  tapSquare(g, h4, sq(g, "c3"), Stone::Flat, move);
  tapSquare(g, h4, sq(g, "b3"), Stone::Flat, move);
  Game shown;
  preview(g, h4, shown);
  CHECK(shown.sq[sq(g, "c3")].height == 0);
  CHECK(shown.sq[sq(g, "b3")].height == 1);
  CHECK(inHand(h4) == 2);
}

static void handPlacesAndSmashes() {
  Game g;
  reset(g, 5);
  Hand hand;
  Move move;
  // Opening: whatever the tray says, it is a flat.
  CHECK(tapSquare(g, hand, 0, Stone::Wall, move) == TapResult::Played);
  CHECK(move == place(0, Stone::Flat));
  const char* m[] = {"a1", "e5", "Cc3", "Sd3", "b1", "e1"};
  reset(g, 5);
  CHECK(play(g, m, 6));
  // Black's wall on d3 blocks a white flat stack, but the white cap alone flattens it.
  CHECK(tapSquare(g, hand, sq(g, "c3"), Stone::Flat, move) == TapResult::Changed);
  CHECK(tapSquare(g, hand, sq(g, "d3"), Stone::Flat, move) == TapResult::Played);
  Move want;
  CHECK(fromPtn(g, "c3>", want) && move == want);
  // A tap on the opponent's stack does nothing.
  Hand h2;
  CHECK(tapSquare(g, h2, sq(g, "d3"), Stone::Flat, move) == TapResult::Ignored);
  // A cap onto a cap does nothing.
}

int main() {
  openingPlacesOpponentsFlat();
  capFlattensWall();
  roadsAndDragonClause();
  flatWinWhenFull();
  ptnRoundTrip();
  perftMidgame();
  perftStart();
  handBuildsSlides();
  handPlacesAndSmashes();
  engineTakesRoad();
  engineBlocksRoad();
  engineRespectsBudget();
  selfPlay();
  if (failures) {
    std::printf("%d tak check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("tak: all checks passed\n");
  return EXIT_SUCCESS;
}

// HIVE host tests: the placement and movement rules one at a time, then whole
// random games that must keep the hive in one piece, then the opponent.

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

#include "HiveCore.h"
#include "HiveEngine.h"

namespace {
using namespace hive;
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    if (failures < 25) std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
// Puts a piece down by hand, outside the rules, to build a position.
void put(Game& g, int color, Bug bug, int q, int r) {
  const int piece = g.handPiece(color, bug);
  g.pieces[piece].z = static_cast<uint8_t>(g.heightAt(q, r));
  g.pieces[piece].q = static_cast<int8_t>(q);
  g.pieces[piece].r = static_cast<int8_t>(r);
  g.pieces[piece].placed = 1;
}
int pieceAt(const Game& g, int q, int r) { return g.topAt(q, r); }
Move moves[kMaxMoves];
bool canReach(const Game& g, int piece, int q, int r) {
  const int n = g.destinations(piece, moves, kMaxMoves);
  for (int i = 0; i < n; ++i)
    if (moves[i].q == q && moves[i].r == r) return true;
  return false;
}
int reachCount(const Game& g, int piece) { return g.destinations(piece, moves, kMaxMoves); }
}  // namespace

int main() {
  // --- opening and placement ---
  {
    Game g;
    int n = g.legalMoves(moves, kMaxMoves);
    check(n == 5, "white's first move: one cell, five kinds of piece");
    for (int i = 0; i < n; ++i) check(moves[i].q == 0 && moves[i].r == 0, "the first piece goes at the origin");
    g.apply(Move{static_cast<int8_t>(g.handPiece(0, Bug::Ant)), 0, 0});
    n = g.legalMoves(moves, kMaxMoves);
    check(n == 30, "black's first: any of the six cells beside it, five kinds");
    g.apply(Move{static_cast<int8_t>(g.handPiece(1, Bug::Ant)), 1, 0});
    n = g.destinations(g.handPiece(0, Bug::Queen), moves, kMaxMoves);
    check(n == 3, "white's second touches white only: the three cells away from black");
    for (int i = 0; i < n; ++i)
      check(moves[i].q < 0 || (moves[i].q == 0 && moves[i].r != 0 && moves[i].q + moves[i].r <= 0),
            "none of them beside black");
    check(reachCount(g, pieceAt(g, 0, 0)) == 0, "nothing moves before its queen is down");
  }
  {
    Game g;
    // Three turns each without a queen: the fourth must be the queen.
    const Bug order[3] = {Bug::Ant, Bug::Ant, Bug::Hopper};
    int wq = 0, bq = 1;
    g.apply(Move{static_cast<int8_t>(g.handPiece(0, order[0])), 0, 0});
    g.apply(Move{static_cast<int8_t>(g.handPiece(1, order[0])), 1, 0});
    for (int k = 1; k < 3; ++k) {
      --wq;
      ++bq;
      g.apply(Move{static_cast<int8_t>(g.handPiece(0, order[k])), static_cast<int8_t>(wq), 0});
      g.apply(Move{static_cast<int8_t>(g.handPiece(1, order[k])), static_cast<int8_t>(bq), 0});
    }
    const int n = g.legalMoves(moves, kMaxMoves);
    check(n > 0, "the fourth turn has moves");
    for (int i = 0; i < n; ++i)
      check(g.pieces[moves[i].piece].bug == static_cast<uint8_t>(Bug::Queen), "and all of them place the queen");
  }

  // --- movement ---
  {
    // A row of three: the middle one holds the hive together.
    Game g;
    put(g, 0, Bug::Queen, 0, 0);
    put(g, 1, Bug::Queen, 1, 0);
    put(g, 0, Bug::Ant, -1, 0);
    g.turnsTaken[0] = g.turnsTaken[1] = 2;
    check(reachCount(g, pieceAt(g, 0, 0)) == 0, "the piece in the middle of a row cannot move");
    const int ant = pieceAt(g, -1, 0);
    check(reachCount(g, ant) == 7, "an ant at the end of a row of three reaches every free cell round the other two");
    check(canReach(g, ant, 2, 0) && !canReach(g, ant, -1, 0), "including the far end, not where it stands");
  }
  {
    // The queen steps one cell, keeping touch.
    Game g;
    put(g, 0, Bug::Queen, 0, 0);
    put(g, 1, Bug::Queen, 1, 0);
    g.turnsTaken[0] = g.turnsTaken[1] = 1;
    const int queen = pieceAt(g, 0, 0);
    check(reachCount(g, queen) == 2 && canReach(g, queen, 1, -1) && canReach(g, queen, 0, 1),
          "a queen slides round its neighbour");
  }
  {
    // Grasshopper: over a line of pieces to the first gap, and not over nothing.
    Game g;
    put(g, 0, Bug::Queen, 0, 0);
    put(g, 1, Bug::Queen, 1, 0);
    put(g, 1, Bug::Ant, 2, 0);
    put(g, 0, Bug::Hopper, -1, 0);
    g.turnsTaken[0] = g.turnsTaken[1] = 2;
    const int hopper = pieceAt(g, -1, 0);
    check(reachCount(g, hopper) == 1 && canReach(g, hopper, 3, 0), "a grasshopper jumps the whole line");
  }
  {
    // Spider: exactly three steps.
    Game g;
    put(g, 0, Bug::Queen, 0, 0);
    put(g, 1, Bug::Queen, 1, 0);
    put(g, 0, Bug::Spider, -1, 0);
    g.turnsTaken[0] = g.turnsTaken[1] = 2;
    const int spider = pieceAt(g, -1, 0);
    check(reachCount(g, spider) == 2, "a spider on the end of a pair has two places three steps away");
    check(canReach(g, spider, 1, 1) && canReach(g, spider, 2, -1), "three steps round either side");
    check(!canReach(g, spider, 0, 1) && !canReach(g, spider, 2, 0), "not two steps, and not four");
  }
  {
    // A gate: two pieces a cell apart cannot be squeezed between.
    Game g;
    put(g, 0, Bug::Queen, 0, 0);
    put(g, 1, Bug::Queen, 1, -1);
    put(g, 1, Bug::Ant, 2, -1);
    put(g, 1, Bug::Hopper, 2, 0);
    put(g, 0, Bug::Spider, 1, 1);
    put(g, 0, Bug::Hopper, 0, 1);
    put(g, 0, Bug::Ant, -1, 0);
    g.turnsTaken[0] = g.turnsTaken[1] = 4;
    // (1,0) is ringed on five sides with one way in, between (2,0) and (1,1)... which touch: no way in at all.
    check(!canReach(g, pieceAt(g, -1, 0), 1, 0), "an ant cannot squeeze into a hole");
    const int beetle = g.handPiece(0, Bug::Beetle);
    put(g, 0, Bug::Beetle, -1, 1);
    check(g.pieces[beetle].placed && canReach(g, beetle, 0, 1) && canReach(g, beetle, -1, 0),
          "a beetle climbs onto its neighbours");
  }
  {
    // A beetle on top pins what is under it and counts as the cell's colour.
    Game g;
    put(g, 0, Bug::Queen, 0, 0);
    put(g, 1, Bug::Queen, 1, 0);
    put(g, 0, Bug::Beetle, 1, 0);  // white beetle on the black queen
    g.turnsTaken[0] = g.turnsTaken[1] = 2;
    g.turn = 1;
    check(g.heightAt(1, 0) == 2 && g.pieces[g.topAt(1, 0)].color == 0, "the beetle is on top");
    check(reachCount(g, 1 * kPerPlayer) == 0, "the queen under a beetle cannot move");
    check(g.destinations(g.handPiece(1, Bug::Ant), moves, kMaxMoves) == 0,
          "black has nowhere to place: every cell touches white on top");
    g.turn = 0;
    check(canReach(g, g.handPiece(0, Bug::Ant), 2, 0), "and white may place beside its beetle");
    const int beetle = g.topAt(1, 0);
    check(reachCount(g, beetle) == 6, "a beetle on the hive steps off in any direction or onto a neighbour");
  }
  {
    // Surrounded.
    Game g;
    put(g, 1, Bug::Queen, 0, 0);
    put(g, 0, Bug::Queen, 1, 0);
    put(g, 0, Bug::Ant, 1, -1);
    put(g, 0, Bug::Ant, 0, -1);
    put(g, 0, Bug::Ant, -1, 0);
    put(g, 0, Bug::Hopper, -1, 1);
    check(g.winner() == -1 && g.queenNeighbours(1) == 5, "five round the queen is not yet a win");
    put(g, 1, Bug::Ant, 0, 1);
    check(g.winner() == 0, "six is, whoever's pieces they are");
    check(g.legalMoves(moves, kMaxMoves) == 1 && moves[0].pass(), "a finished game offers only a pass");
  }

  // --- whole games ---
  Rng rng;
  rng.state = 2026;
  int finished = 0;
  for (int game = 0; game < 300; ++game) {
    Game g;
    for (int ply = 0; ply < 300 && g.winner() < 0; ++ply) {
      const int n = g.legalMoves(moves, kMaxMoves);
      check(n >= 1 && n < kMaxMoves, "there is always a move, and never more than the buffer");
      const Move m = moves[rng.below(n)];
      g.apply(m);
      check(g.connected(), "the hive stays in one piece");
      for (int i = 0; i < kPieces; ++i) {
        const Piece& p = g.pieces[i];
        if (!p.placed) continue;
        check(p.z < g.heightAt(p.q, p.r), "a piece's height is inside its stack");
        for (int k = 0; k < i; ++k) {
          const Piece& o = g.pieces[k];
          check(!(o.placed && o.q == p.q && o.r == p.r && o.z == p.z), "no two pieces share a place");
        }
        if (p.z > 0) check(p.bug == static_cast<uint8_t>(Bug::Beetle), "only beetles climb");
      }
      if (g.turnsTaken[0] >= 4) check(g.queenPlaced(0), "white's queen is down by its fourth turn");
      if (g.turnsTaken[1] >= 4) check(g.queenPlaced(1), "black's queen is down by its fourth turn");
    }
    finished += g.winner() >= 0 ? 1 : 0;
  }
  check(finished > 0, "some random games end in a win");

  // --- the opponent ---
  auto buffers = std::make_unique<SearchBuffers>();
  {
    // One move from winning: it takes it, at every level.
    Game g;
    put(g, 1, Bug::Queen, 0, 0);
    put(g, 0, Bug::Queen, 1, 0);
    put(g, 0, Bug::Ant, 1, -1);
    put(g, 0, Bug::Beetle, 0, -1);
    put(g, 0, Bug::Ant, -1, 0);
    put(g, 0, Bug::Hopper, -1, 1);
    put(g, 0, Bug::Ant, 2, 0);  // free to walk round to the last gap
    g.turnsTaken[0] = g.turnsTaken[1] = 6;
    g.turn = 0;
    for (int level = 0; level < kLevels; ++level) {
      Game copy = g;
      copy.apply(chooseMove(copy, level, rng, *buffers));
      check(copy.winner() == 0, "the opponent closes the ring when it can");
    }
  }
  {
    // Engine against random play: it should win most, and never make a move
    // the rules do not offer.
    int engineWins = 0;
    long worst = 0;
    double slowestMs = 0;
    for (int game = 0; game < 12; ++game) {
      Game g;
      const int engineSide = game % 2;
      for (int ply = 0; ply < 160 && g.winner() < 0; ++ply) {
        const int n = g.legalMoves(moves, kMaxMoves);
        Move m = moves[rng.below(n)];
        if (g.turn == engineSide) {
          long nodes = 0;
          const auto t0 = std::chrono::steady_clock::now();
          m = chooseMove(g, game % kLevels, rng, *buffers, &nodes);
          const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
          if (ms > slowestMs) slowestMs = ms;
          if (nodes > worst) worst = nodes;
          bool legal = false;
          for (int i = 0; i < n; ++i)
            legal = legal || (moves[i].piece == m.piece && moves[i].q == m.q && moves[i].r == m.r);
          check(legal, "the opponent's move is one the rules offer");
        }
        g.apply(m);
      }
      engineWins += g.winner() == engineSide ? 1 : 0;
    }
    std::printf("engine: %d of 12 against random play, most nodes %ld, slowest move %.1f ms here\n", engineWins, worst,
                slowestMs);
    check(engineWins >= 7, "the opponent beats random play most of the time");
  }

  if (failures == 0) std::printf("hive: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

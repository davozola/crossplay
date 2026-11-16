// LOGIC host tests: every board the device ships is solved again here, must
// have exactly one solution, and that solution, played through the device's
// own state, must read as solved.

#include <cstdio>
#include <cstring>

#include "../../tools_local/logic/logic_solvers.h"
#include "LogicCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    if (failures < 20) std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
logicsolve::P asP(const logic::Board& b) {
  logicsolve::P p;
  p.w = b.w;
  p.h = b.h;
  p.c.assign(b.cells, static_cast<size_t>(b.w) * b.h);
  return p;
}
}  // namespace

int main() {
  using namespace logic;
  uint8_t blob[512];

  for (int size = 0; size < kSizes; ++size) {
    for (int n = 0; n < boardsPerSize(); ++n) {
      {
        const Board b = boardOf(Game::Slither, size, n);
        check(std::strlen(b.cells) == static_cast<size_t>(b.w) * b.h, "slither board length");
        std::string in;
        check(logicsolve::slither(asP(b), &in, 2, 50000000) == 1, "slither has one solution");
        auto s = new Slither;
        s->load(b);
        check(!s->solved(), "an empty slither is not solved");
        auto inside = [&](int x, int y) { return x >= 0 && y >= 0 && x < b.w && y < b.h && in[y * b.w + x] == '1'; };
        for (int y = 0; y <= b.h; ++y)
          for (int x = 0; x < b.w; ++x)
            if (inside(x, y - 1) != inside(x, y)) s->cycleH(x, y);
        for (int y = 0; y < b.h; ++y)
          for (int x = 0; x <= b.w; ++x)
            if (inside(x - 1, y) != inside(x, y)) s->cycleV(x, y);
        check(s->solved(), "slither solution reads as solved");
        const int len = s->save(blob, sizeof(blob));
        auto t = new Slither;
        t->load(b);
        check(len > 0 && t->restore(blob, len) && t->solved(), "slither survives a save");
        s->cycleH(0, 0);
        check(!s->solved(), "one changed edge unsolves it");
        s->cycleH(0, 0);  // now a cross where there was a line or none
        s->cycleH(0, 0);
        check(s->solved(), "three taps come back round");
        delete s;
        delete t;
      }
      {
        const Board b = boardOf(Game::Shikaku, size, n);
        std::vector<logicsolve::Rect> rects;
        check(logicsolve::shikaku(asP(b), &rects) == 1, "shikaku has one solution");
        auto s = new Shikaku;
        s->load(b);
        check(!s->solved(), "an empty shikaku is not solved");
        for (const auto& r : rects) {
          s->tap(r.x, r.y);
          s->tap(r.x + r.w - 1, r.y + r.h - 1);
        }
        check(s->solved(), "shikaku solution reads as solved");
        const int len = s->save(blob, sizeof(blob));
        auto t = new Shikaku;
        t->load(b);
        check(len > 0 && t->restore(blob, len) && t->solved(), "shikaku survives a save");
        s->tap(0, 0);
        check(!s->solved() && s->rectAt(0, 0) < 0, "tapping a rectangle removes it");
        delete s;
        delete t;
      }
      {
        const Board b = boardOf(Game::Akari, size, n);
        std::string bulbs;
        check(logicsolve::akari(asP(b), &bulbs) == 1, "akari has one solution");
        auto s = new Akari;
        s->load(b);
        check(!s->solved(), "an empty akari is not solved");
        for (int i = 0; i < b.w * b.h; ++i)
          if (bulbs[i] == '1') s->tap(i % b.w, i / b.w);
        check(s->solved(), "akari solution reads as solved");
        const int len = s->save(blob, sizeof(blob));
        auto t = new Akari;
        t->load(b);
        check(len > 0 && t->restore(blob, len) && t->solved(), "akari survives a save");
        delete s;
        delete t;
      }
      {
        const Board b = boardOf(Game::Hashi, size, n);
        std::vector<logicsolve::Bridge> bridges;
        check(logicsolve::hashi(asP(b), &bridges) == 1, "hashi has one solution");
        auto s = new Hashi;
        s->load(b);
        check(!s->solved(), "an empty hashi is not solved");
        check(s->edgeCount == static_cast<int>(bridges.size()), "device and solver list the same bridges");
        for (int e = 0; e < s->edgeCount && e < static_cast<int>(bridges.size()); ++e) {
          check(s->edges[e].a == bridges[e].a && s->edges[e].b == bridges[e].b, "in the same order");
          for (int k = 0; k < bridges[e].count; ++k) check(s->cycle(e), "a solution bridge is never blocked");
        }
        check(s->solved(), "hashi solution reads as solved");
        const int len = s->save(blob, sizeof(blob));
        auto t = new Hashi;
        t->load(b);
        check(len > 0 && t->restore(blob, len) && t->solved(), "hashi survives a save");
        // Every bridge can be found by a tap on its middle.
        for (int e = 0; e < s->edgeCount; ++e) {
          const auto& a = s->islands[s->edges[e].a];
          const auto& c = s->islands[s->edges[e].b];
          check(s->edgeNear((a.x + c.x) / 2.0f + 0.5f, (a.y + c.y) / 2.0f + 0.5f) == e ||
                    s->edgeNear((a.x + c.x) / 2.0f + 0.5f, (a.y + c.y) / 2.0f + 0.5f) >= 0,
                "a tap on a bridge's middle finds a bridge");
        }
        delete s;
        delete t;
      }
    }
  }

  // Akari by hand: two bulbs in sight of each other clash.
  {
    static const char cells[] = "....#....";
    Board b;
    b.w = b.h = 3;
    b.cells = cells;
    Akari a;
    a.load(b);
    a.tap(0, 0);
    a.tap(2, 0);
    check(a.clash(0, 0) && a.clash(2, 0), "bulbs in a row clash");
    a.tap(2, 0);
    check(a.mark[2] == Akari::Dot && !a.clash(0, 0), "second tap is a dot");
    a.tap(1, 1);
    check(a.mark[4] == Akari::Empty, "a wall takes no mark");
    check(a.lit(1, 0) && a.lit(0, 2) && !a.lit(2, 2), "light runs along rows and columns");
  }
  // Hashi by hand: a bridge cannot cross another.
  {
    static const char cells[] = ".1.1.1.1.";
    Board b;
    b.w = b.h = 3;
    b.cells = cells;
    Hashi h;
    h.load(b);
    check(h.islandCount == 4 && h.edgeCount == 2, "a cross of four islands has two bridges");
    check(h.cycle(0) && !h.cycle(1) && h.edges[1].count == 0, "the second cannot cross the first");
    check(h.edgeNear(1.5f, 1.5f) >= 0 && h.edgeNear(0.2f, 0.2f) < 0, "taps find bridges, not corners");
  }
  check(std::strcmp(gameName(Game::Hashi), "HASHI") == 0 && sideOf(Game::Hashi, 2) == 11 && sideOf(Game::Akari, 0) == 6,
        "names and sizes");

  if (failures == 0) std::printf("logic: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

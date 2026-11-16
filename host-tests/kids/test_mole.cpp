#include <cstdio>

#include "MoleCore.h"

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

using namespace mole;

static int firstUp(const Game& g) {
  for (int i = 0; i < holeCount(g.config); ++i) {
    if (g.holes[i].state == State::Up) return i;
  }
  return -1;
}

int main() {
  // Every arrangement: the right crowd is out from the start, and stays out.
  for (uint8_t holes = 0; holes < kHoleChoices; ++holes) {
    for (uint8_t who = 0; who < kWhoChoices; ++who) {
      for (uint8_t hides = 0; hides < kHideChoices; ++hides) {
        Config c;
        c.holes = holes;
        c.who = who;
        c.hides = hides;
        Game g;
        start(g, c, 77u + holes * 31u + who, 0);
        CHECK(out(g) == crowd(c));
        CHECK(columns(c) * rows(c) == holeCount(c));
        uint32_t now = 0;
        bool seen[kAnimals] = {};
        for (int round = 0; round < 400; ++round) {
          const int hole = firstUp(g);
          CHECK(hole >= 0);
          if (hole < 0) break;
          const uint8_t animal = g.holes[hole].animal;
          CHECK(animal < kAnimals);
          if (who < kAnimals) CHECK(animal == who);
          seen[animal] = true;
          CHECK(tap(g, hole, now));
          CHECK(!tap(g, hole, now));  // already seeing stars
          CHECK(g.stars == round + 1);
          CHECK(g.party == (g.stars % kPartyEvery == 0));
          g.party = false;
          CHECK(!tick(g, now + kBoppedMs - 1) || hides);
          now += kBoppedMs;
          CHECK(tick(g, now));
          // The one bopped is gone, somebody is out, and not where it just was.
          CHECK(g.holes[hole].state == State::Empty);
          CHECK(out(g) == crowd(c));
          now += 50;
        }
        if (who == kAnimals) CHECK(seen[0] && seen[1] && seen[2] && seen[3]);
      }
    }
  }
  {  // waiting: nobody moves by themselves
    Config c;
    Game g;
    start(g, c, 5, 0);
    const int hole = firstUp(g);
    CHECK(!tick(g, 1000000));
    CHECK(firstUp(g) == hole);
    CHECK(!tap(g, (hole + 1) % 4, 10));  // an empty hole is nothing
    CHECK(g.stars == 0);
    CHECK(!tap(g, -1, 10) && !tap(g, 4, 10));
  }
  {  // hiding: it moves on, to a different hole, without a star
    Config c;
    c.hides = 3;
    CHECK(stayMs(c) == 3000);
    Game g;
    start(g, c, 9, 0);
    const int hole = firstUp(g);
    CHECK(!tick(g, 2999));
    CHECK(tick(g, 3000));
    CHECK(firstUp(g) >= 0 && firstUp(g) != hole);
    CHECK(g.stars == 0 && out(g) == 1);
  }
  {  // the clock wrapping round does not strand anybody
    Config c;
    c.hides = 4;
    Game g;
    start(g, c, 3, 0xFFFFFF00u);
    CHECK(!tick(g, 0xFFFFFF00u + 100));
    CHECK(tick(g, 0xFFFFFF00u + 2000));
  }
  {  // the choices on the card
    Config c;
    c.holes = 2;
    c.who = 4;
    c.hides = 4;
    c.sound = 0;
    uint8_t bytes[kConfigBytes];
    pack(c, bytes);
    Config back;
    CHECK(unpack(bytes, kConfigBytes, back) && back.holes == 2 && back.who == 4 && back.hides == 4 && back.sound == 0);
    CHECK(!unpack(bytes, kConfigBytes - 1, back));
    bytes[1] = 3;
    CHECK(!unpack(bytes, kConfigBytes, back));
    bytes[1] = 0;
    bytes[0] = 1;
    CHECK(!unpack(bytes, kConfigBytes, back));
  }
  if (failures == 0) std::printf("mole: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

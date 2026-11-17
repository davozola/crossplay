// ILYA's four bigger games, played here: every deal is winnable, every maze
// has one way to everywhere, every sum has its answer among the choices.

#include <cstdio>
#include <cstring>

#include "KidsDots.h"
#include "KidsMoreCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
}  // namespace

int main() {
  using namespace kidsmore;
  kids::Rng rng;

  // --- MEMORIA: every table, every theme ---
  for (uint8_t cards = 0; cards < 6; ++cards) {
    for (uint8_t theme = 0; theme < 9; ++theme) {
      for (uint8_t players = 0; players < 2; ++players) {
        const uint8_t options[memory::OptCount] = {cards, theme, players, static_cast<uint8_t>(theme % 2)};
        memory::Game game;
        memory::deal(game, options, rng);
        check(game.count == memory::cardCount(options), "the table has its cards");
        check(memory::columns(game.count) * memory::rows(game.count) == game.count, "the table is a full grid");
        int seen[memory::kMaxCards] = {};
        for (int i = 0; i < game.count; ++i) ++seen[game.cards[i].pair];
        for (int p = 0; p < game.count / 2; ++p) check(seen[p] == 2, "every pair is two cards");
        // No two pairs look alike.
        for (int i = 0; i < game.count; ++i) {
          for (int j = i + 1; j < game.count; ++j) {
            const memory::Card& a = game.cards[i];
            const memory::Card& b = game.cards[j];
            if (a.pair != b.pair) check(!(a.face == b.face && a.value == b.value), "no two pairs share a face");
          }
        }
        if (game.peeking) check(memory::tap(game, 0) == memory::Tap::Hidden, "the first touch hides the peek");
        // Played by someone who remembers everything: pair by pair.
        for (int p = 0; p < game.count / 2; ++p) {
          int first = -1;
          for (int i = 0; i < game.count; ++i) {
            if (game.cards[i].pair != p) continue;
            if (first < 0) {
              first = i;
              check(memory::tap(game, i) == memory::Tap::Turned, "the first of a pair turns");
            } else {
              const memory::Tap t = memory::tap(game, i);
              check(t == memory::Tap::Pair || t == memory::Tap::Done, "the second of a pair is a pair");
            }
          }
        }
        check(game.done() && game.moves == game.count / 2, "a perfect game takes one move a pair");
        check(game.score[0] + game.score[1] == game.count / 2, "every pair is somebody's");
      }
    }
  }
  {
    const uint8_t options[memory::OptCount] = {0, 0, 1, 0};
    memory::Game game;
    memory::deal(game, options, rng);
    int other = 1;
    while (game.cards[other].pair == game.cards[0].pair) ++other;
    memory::tap(game, 0);
    check(memory::tap(game, other) == memory::Tap::Miss && game.turn == 1, "a miss passes the turn");
    check(game.cards[0].up && game.cards[other].up, "a wrong pair stays up to be looked at");
    check(memory::tap(game, -1) == memory::Tap::Hidden && !game.cards[0].up, "and the next touch turns it back");
  }

  // --- SUMES ---
  for (uint8_t kind = 0; kind < 6; ++kind) {
    for (uint8_t limit = 0; limit < 3; ++limit) {
      const uint8_t options[sums::OptCount] = {kind, limit, 0};
      const int top = sums::limitOf(options);
      sums::Round last;
      for (int n = 0; n < 400; ++n) {
        sums::Round r = sums::deal(options, rng, last);
        check(r.optionCount >= 2 && r.answer >= 0 && r.answer < r.optionCount, "the answer is one of the choices");
        for (int i = 0; i < r.optionCount; ++i) {
          check(r.options[i] >= 0 && r.options[i] <= top, "every choice is within the limit");
          for (int j = i + 1; j < r.optionCount; ++j) check(r.options[i] != r.options[j], "no choice twice");
        }
        switch (r.kind) {
          case sums::Kind::Count:
            check(r.result() == r.a && r.a >= 1, "counting");
            break;
          case sums::Kind::Add:
            check(r.result() == r.a + r.b && r.a >= 1 && r.b >= 1, "adding");
            break;
          case sums::Kind::Take:
            check(r.result() == r.a - r.b && r.b >= 1 && r.b <= r.a,
                  "taking away takes something, and never below nothing");
            break;
          case sums::Kind::More:
            check(
                r.a != r.b && (r.fewer ? r.result() == (r.a < r.b ? r.a : r.b) : r.result() == (r.a > r.b ? r.a : r.b)),
                "more or fewer");
            break;
          case sums::Kind::Missing:
            check(r.result() == r.sequence[r.hole] && r.sequence[4] <= top, "the missing number");
            break;
        }
        if (kind != 5 && n > 0) check(!(r.a == last.a && r.b == last.b) || top <= 5, "not the same sum twice running");
        check(!r.choose(r.answer == 0 ? 1 : 0) && r.wrongMask != 0 && r.choose(r.answer) && r.solved,
              "wrong, then right");
        last = r;
      }
    }
  }

  // --- LABERINT ---
  for (uint8_t size = 0; size < 4; ++size) {
    for (int n = 0; n < 60; ++n) {
      const uint8_t options[maze::OptCount] = {size, 5, 0, 0};
      maze::Maze m;
      maze::build(m, options, rng);
      const int cells = m.w * m.h;
      int doors = 0;
      for (int c = 0; c < cells; ++c) {
        for (int d = 0; d < 4; ++d) {
          if (!m.canGo(c, static_cast<maze::Dir>(d))) continue;
          ++doors;
          const int x = c % m.w + (d == 1) - (d == 3);
          const int y = c / m.w + (d == 2) - (d == 0);
          check(x >= 0 && y >= 0 && x < m.w && y < m.h, "no door leads off the maze");
          if (x >= 0 && y >= 0 && x < m.w && y < m.h) {
            check(m.canGo(y * m.w + x, static_cast<maze::Dir>((d + 2) % 4)), "every door opens both ways");
          }
        }
      }
      check(doors == 2 * (cells - 1), "one way to everywhere: a door fewer than cells");
      check(m.goal != m.at, "the goal is somewhere else");
      for (int s = 0; s < m.starCount; ++s)
        check(m.stars[s] > 0 && m.stars[s] != m.goal, "stars sit off the start and goal");
      // Following the hints gets there.
      for (int guard = 0; guard < cells * 2 && !m.done(); ++guard) {
        int next[1];
        check(maze::hint(m, next, 1) == 1, "there is always a next step");
        check(maze::walkTowards(m, next[0]) == 1, "and it can be taken");
      }
      check(m.done(), "the hints lead to the goal");
      const maze::Pair pair = maze::pairOf(options, rng);
      check(pair.walker < kids::kPictures && pair.goal < kids::kPictures && pair.walker != pair.goal,
            "walker and goal");
    }
  }
  {
    // A tap through a wall goes nowhere; a tap along a corridor stops at its end.
    const uint8_t options[maze::OptCount] = {0, 0, 1, 0};
    maze::Maze m;
    maze::build(m, options, rng);
    const bool east = m.canGo(0, maze::East);
    const int far = east ? m.w - 1 : (m.h - 1) * m.w;
    const int taken = maze::walkTowards(m, far);
    check(taken >= 1 && taken <= (east ? m.w - 1 : m.h - 1), "a long tap walks the corridor");
    check(m.steps == taken && m.seen[m.at], "and is counted and remembered");
  }

  // --- PUNTS ---
  {
    uint8_t counts[kidsdots::kFigures * 3];
    for (int i = 0; i < kidsdots::kFigures * 3; ++i) {
      counts[i] = kidsdots::kSpans[i].count;
      check(counts[i] >= 5 && counts[i] <= 40, "a figure has a sensible number of dots");
      const kidsdots::Span& span = kidsdots::kSpans[i];
      check((span.first + span.count) * 2 <= static_cast<int>(sizeof(kidsdots::kDots)), "its dots are in the table");
      for (int k = 0; k < span.count; ++k) {
        check(kidsdots::kDots[(span.first + k) * 2] < 96 && kidsdots::kDots[(span.first + k) * 2 + 1] < 96,
              "every dot is on the picture");
      }
    }
    for (int f = 0; f < kidsdots::kFigures; ++f) {
      check(kidsdots::kPicture[f] < kids::kPictures, "a figure is of a picture");
      check(counts[f * 3] <= counts[f * 3 + 1] && counts[f * 3 + 1] <= counts[f * 3 + 2],
            "harder levels have more dots");
    }
    const uint8_t inOrder[dots::OptCount] = {2, 0, 1, 1};
    dots::Game game;
    dots::start(game, inOrder, rng, kidsdots::kFigures - 1, kidsdots::kFigures, counts);
    check(game.figure == 0 && game.count == counts[2], "in order wraps to the first, at the level chosen");
    check(!dots::tap(game, 1) && game.joined == 0, "the wrong dot does nothing");
    for (int i = 0; i < game.count; ++i) check(dots::tap(game, i), "each dot in turn");
    check(game.done(), "and the figure is finished");
    const uint8_t random[dots::OptCount] = {0, 1, 0, 0};
    int previous = 3;
    for (int n = 0; n < 200; ++n) {
      dots::start(game, random, rng, previous, kidsdots::kFigures, counts);
      check(game.figure != previous && game.figure < kidsdots::kFigures, "never the same figure twice running");
      previous = game.figure;
    }
    char text[8];
    dots::label(random, 2, 9, text, sizeof(text));
    check(std::strcmp(text, "C") == 0, "letters");
    dots::label(random, 2, 30, text, sizeof(text));
    check(std::strcmp(text, "3") == 0, "numbers when the alphabet would run out");
    const uint8_t twos[dots::OptCount] = {0, 2, 0, 0};
    dots::label(twos, 2, 9, text, sizeof(text));
    check(std::strcmp(text, "6") == 0, "counting in twos");
  }

  // --- the choices on the card ---
  {
    const uint8_t stored[4] = {5, 200, 1, 1};
    uint8_t values[4];
    unpackOptions(stored, 3, memory::kOptions, 4, values);
    check(values[0] == 5 && values[1] == 0 && values[2] == 1 && values[3] == 0,
          "a value out of range or missing is the first");
  }

  if (failures == 0) std::printf("kids more: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

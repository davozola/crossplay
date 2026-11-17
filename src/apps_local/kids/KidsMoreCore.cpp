#include "KidsMoreCore.h"

#include <cstdio>

namespace kidsmore {

namespace {

template <typename T>
void shuffle(T* items, const int count, kids::Rng& rng) {
  for (int i = count - 1; i > 0; --i) {
    const int j = rng.below(i + 1);
    const T t = items[i];
    items[i] = items[j];
    items[j] = t;
  }
}

constexpr const char* kYesNo[] = {"NO", "SI"};
// For a choice that is on unless somebody turns it off.
constexpr const char* kYesFirst[] = {"SI", "NO"};

}  // namespace

// --- MEMORIA -------------------------------------------------------------------

namespace memory {

namespace {

constexpr const char* kCardNames[] = {"6", "8", "12", "16", "20", "24"};
constexpr int kCardCounts[] = {6, 8, 12, 16, 20, 24};
constexpr const char* kThemeNames[] = {"DE TOT", "ANIMALS", "MENJAR",  "RODES I VIATGES", "NATURA",
                                       "COSES",  "LLETRES", "NOMBRES", "PARAULES"};
constexpr const char* kPlayerNames[] = {"1", "2"};

// KidsCore's pictures by kind.
constexpr uint8_t kAnimals[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
constexpr uint8_t kFood[] = {10, 11, 12, 27, 28, 38, 39};
constexpr uint8_t kWheels[] = {13, 14, 15, 16, 17, 18, 41};
constexpr uint8_t kNature[] = {19, 20, 21, 22, 23, 32, 33, 34};
constexpr uint8_t kThings[] = {24, 25, 26, 29, 30, 31, 35, 36, 37, 40, 42, 43, 44};

}  // namespace

const Option kOptions[OptCount] = {
    {"CARTES", kCardNames, 6},
    {"TEMA", kThemeNames, 9},
    {"JUGADORS", kPlayerNames, 2},
    {"MIRAR ABANS", kYesNo, 2},
};

int cardCount(const uint8_t* options) { return kCardCounts[options[OptCards] % 6]; }

int columns(const int cards) { return cards <= 8 ? 2 : cards <= 12 ? 3 : 4; }
int rows(const int cards) { return cards / columns(cards); }

void deal(Game& game, const uint8_t* options, kids::Rng& rng) {
  game = Game{};
  game.count = cardCount(options);
  game.players = options[OptPlayers] ? 2 : 1;
  game.peeking = options[OptPeek] != 0;
  const Theme theme = static_cast<Theme>(options[OptTheme] % 9);
  const int pairs = game.count / 2;

  // The pool the pairs are drawn from, shuffled.
  uint8_t pool[kids::kPictures];
  int poolSize = 0;
  auto take = [&](const uint8_t* from, const int n) {
    for (int i = 0; i < n; ++i) pool[poolSize++] = from[i];
  };
  switch (theme) {
    case Theme::Animals:
      take(kAnimals, sizeof(kAnimals));
      break;
    case Theme::Food:
      take(kFood, sizeof(kFood));
      break;
    case Theme::Wheels:
      take(kWheels, sizeof(kWheels));
      break;
    case Theme::Nature:
      take(kNature, sizeof(kNature));
      break;
    case Theme::Things:
      take(kThings, sizeof(kThings));
      break;
    case Theme::Letters:
      for (int i = 0; i < kids::letterCount(); ++i) pool[poolSize++] = static_cast<uint8_t>(i);
      break;
    case Theme::Numbers:
      for (int i = 1; i <= 12; ++i) pool[poolSize++] = static_cast<uint8_t>(i);
      break;
    case Theme::All:
    case Theme::Words:
    default:
      for (int i = 0; i < kids::kPictures; ++i) pool[poolSize++] = static_cast<uint8_t>(i);
      break;
  }
  shuffle(pool, poolSize, rng);
  // A small theme on a big table is topped up from everything else.
  if (poolSize < pairs) {
    uint8_t rest[kids::kPictures];
    int restSize = 0;
    for (int i = 0; i < kids::kPictures; ++i) {
      bool used = false;
      for (int k = 0; k < poolSize; ++k) used = used || pool[k] == i;
      if (!used) rest[restSize++] = static_cast<uint8_t>(i);
    }
    shuffle(rest, restSize, rng);
    for (int i = 0; poolSize < pairs && i < restSize; ++i) pool[poolSize++] = rest[i];
  }

  for (int p = 0; p < pairs; ++p) {
    Card& a = game.cards[p * 2];
    Card& b = game.cards[p * 2 + 1];
    a.pair = b.pair = static_cast<uint8_t>(p);
    a.value = b.value = pool[p];
    if (theme == Theme::Letters) {
      a.face = Face::Letter;
      b.face = Face::Picture;
      b.value = static_cast<uint8_t>(kids::pictureOfLetter(pool[p]));
    } else if (theme == Theme::Numbers) {
      a.face = Face::Number;
      b.face = Face::Dots;
    } else if (theme == Theme::Words) {
      a.face = Face::Picture;
      b.face = Face::Word;
    } else {
      a.face = b.face = Face::Picture;
    }
  }
  shuffle(game.cards, game.count, rng);
  if (game.peeking) {
    for (int i = 0; i < game.count; ++i) game.cards[i].up = true;
  }
}

Tap tap(Game& game, const int index) {
  if (game.done()) return Tap::Done;
  if (game.peeking) {
    game.peeking = false;
    for (int i = 0; i < game.count; ++i) game.cards[i].up = false;
    return Tap::Hidden;
  }
  // A wrong pair lies face up until the next touch, wherever it lands.
  if (game.second >= 0) {
    game.cards[game.first].up = false;
    game.cards[game.second].up = false;
    game.first = game.second = -1;
    if (index < 0 || index >= game.count) return Tap::Hidden;
  }
  if (index < 0 || index >= game.count) return Tap::Nothing;
  Card& card = game.cards[index];
  if (card.found || card.up) return Tap::Nothing;
  card.up = true;
  if (game.first < 0) {
    game.first = static_cast<int8_t>(index);
    return Tap::Turned;
  }
  ++game.moves;
  Card& other = game.cards[game.first];
  if (other.pair == card.pair) {
    other.found = card.found = true;
    game.first = -1;
    ++game.found;
    ++game.score[game.turn];
    // Whoever finds a pair goes again.
    return game.done() ? Tap::Done : Tap::Pair;
  }
  game.second = static_cast<int8_t>(index);
  if (game.players == 2) game.turn ^= 1;
  return Tap::Miss;
}

}  // namespace memory

// --- SUMES ---------------------------------------------------------------------

namespace sums {

namespace {

constexpr const char* kKindNames[] = {"COMPTAR", "SUMAR", "RESTAR", "MES O MENYS", "QUIN FALTA?", "BARREJA"};
constexpr const char* kLimitNames[] = {"FINS A 5", "FINS A 10", "FINS A 20"};
constexpr const char* kHelpNames[] = {"AMB DIBUIXOS", "NOMES NOMBRES"};
constexpr int kLimits[] = {5, 10, 20};
// Pictures that read well small and in a crowd.
constexpr uint8_t kCountable[] = {10, 21, 2, 25, 28, 22, 38, 0, 4, 34};

// Three numbers near `right`, within 0..limit, one of them right, ascending.
void offer(Round& round, const int right, const int limit, kids::Rng& rng) {
  int low = right - rng.below(3);
  if (low < 0) low = 0;
  if (low + 2 > limit) low = limit - 2;
  if (low < 0) low = 0;
  round.optionCount = 3;
  for (int i = 0; i < 3; ++i) {
    round.options[i] = low + i;
    if (low + i == right) round.answer = i;
  }
}

}  // namespace

const Option kOptions[OptCount] = {
    {"JOC", kKindNames, 6},
    {"NOMBRES", kLimitNames, 3},
    {"AJUDA", kHelpNames, 2},
};

int limitOf(const uint8_t* options) { return kLimits[options[OptLimit] % 3]; }

bool Round::choose(const int option) {
  if (solved || option < 0 || option >= optionCount) return false;
  if (option == answer) {
    solved = true;
    return true;
  }
  wrongMask = static_cast<uint8_t>(wrongMask | (1u << option));
  return false;
}

Round deal(const uint8_t* options, kids::Rng& rng, const Round& last) {
  const int limit = limitOf(options);
  for (int attempt = 0;; ++attempt) {
    Round r;
    const int chosen = options[OptKind] % 6;
    r.kind = static_cast<Kind>(chosen == 5 ? rng.below(kKinds) : chosen);
    r.picture = kCountable[rng.below(static_cast<int>(sizeof(kCountable)))];
    switch (r.kind) {
      case Kind::Count:
        r.a = 1 + rng.below(limit);
        offer(r, r.a, limit, rng);
        break;
      case Kind::Add:
        r.a = 1 + rng.below(limit - 1);
        r.b = 1 + rng.below(limit - r.a);
        offer(r, r.a + r.b, limit, rng);
        break;
      case Kind::Take:
        r.a = 1 + rng.below(limit);
        r.b = 1 + rng.below(r.a);
        offer(r, r.a - r.b, limit, rng);
        break;
      case Kind::More: {
        // Two groups that differ; each side is an answer.
        const int side = limit > 10 ? 10 : limit;
        r.a = 1 + rng.below(side);
        do {
          r.b = 1 + rng.below(side);
        } while (r.b == r.a);
        r.fewer = rng.below(2) == 1;
        r.optionCount = 2;
        r.options[0] = r.a;
        r.options[1] = r.b;
        r.answer = (r.a > r.b) != r.fewer ? 0 : 1;
        break;
      }
      case Kind::Missing: {
        const int first = rng.below(limit - kSequence + 2);  // first..first+4 within 0..limit
        for (int i = 0; i < kSequence; ++i) r.sequence[i] = first + i;
        r.hole = rng.below(kSequence);
        r.a = first;
        r.b = r.hole;
        offer(r, r.sequence[r.hole], limit, rng);
        break;
      }
    }
    const bool same = r.kind == last.kind && r.a == last.a && r.b == last.b;
    if (!same || attempt > 8) return r;
  }
}

}  // namespace sums

// --- LABERINT ------------------------------------------------------------------

namespace maze {

namespace {

constexpr const char* kSizeNames[] = {"PETIT", "MITJA", "GRAN", "MOLT GRAN"};
constexpr int kSizes[][2] = {{4, 5}, {6, 8}, {8, 11}, {11, 15}};
constexpr const char* kWhoNames[] = {"CONILL I PASTANAGA", "RATA I GALETA", "GAT I PEIX",
                                     "OCELL I ARBRE",      "COET I LLUNA",  "CADA COP UN"};
constexpr Pair kPairsTable[kPairs] = {{4, 12}, {8, 38}, {0, 2}, {3, 23}, {17, 20}};
constexpr const char* kStarNames[] = {"TRES", "CAP"};
constexpr int kDx[] = {0, 1, 0, -1};
constexpr int kDy[] = {-1, 0, 1, 0};

int neighbour(const Maze& maze, const int cell, const int dir) {
  const int x = cell % maze.w + kDx[dir];
  const int y = cell / maze.w + kDy[dir];
  if (x < 0 || y < 0 || x >= maze.w || y >= maze.h) return -1;
  return y * maze.w + x;
}

// Steps from `from` to every cell, and the cell each was reached from.
void flood(const Maze& maze, const int from, int16_t* distance, int16_t* parent) {
  const int cells = maze.w * maze.h;
  for (int i = 0; i < cells; ++i) distance[i] = parent[i] = -1;
  int16_t queue[kMaxCells];
  int head = 0;
  int tail = 0;
  queue[tail++] = static_cast<int16_t>(from);
  distance[from] = 0;
  while (head < tail) {
    const int cell = queue[head++];
    for (int d = 0; d < 4; ++d) {
      if (!maze.canGo(cell, static_cast<Dir>(d))) continue;
      const int next = neighbour(maze, cell, d);
      if (next < 0 || distance[next] >= 0) continue;
      distance[next] = static_cast<int16_t>(distance[cell] + 1);
      parent[next] = static_cast<int16_t>(cell);
      queue[tail++] = static_cast<int16_t>(next);
    }
  }
}

}  // namespace

const Option kOptions[OptCount] = {
    {"MIDA", kSizeNames, 4},
    {"QUI", kWhoNames, 6},
    {"EL CAMI ES VEU", kYesFirst, 2},
    {"ESTRELLES", kStarNames, 2},
};

Pair pairOf(const uint8_t* options, kids::Rng& rng) {
  const int chosen = options[OptWho] % 6;
  return kPairsTable[chosen == 5 ? rng.below(kPairs) : chosen];
}

bool Maze::canGo(const int cell, const Dir dir) const {
  return cell >= 0 && cell < w * h && (open[cell] & (1u << dir)) != 0;
}

void sizeOf(const uint8_t* options, int& w, int& h) {
  w = kSizes[options[OptSize] % 4][0];
  h = kSizes[options[OptSize] % 4][1];
}

void build(Maze& maze, const uint8_t* options, kids::Rng& rng) {
  maze = Maze{};
  sizeOf(options, maze.w, maze.h);
  const int cells = maze.w * maze.h;
  // Depth first, knocking a wall out on the way to each cell not yet visited:
  // every cell is reached, and by exactly one way.
  bool visited[kMaxCells] = {};
  int16_t stack[kMaxCells];
  int depth = 0;
  stack[depth++] = 0;
  visited[0] = true;
  while (depth > 0) {
    const int cell = stack[depth - 1];
    int choices[4];
    int n = 0;
    for (int d = 0; d < 4; ++d) {
      const int next = neighbour(maze, cell, d);
      if (next >= 0 && !visited[next]) choices[n++] = d;
    }
    if (n == 0) {
      --depth;
      continue;
    }
    const int d = choices[rng.below(n)];
    const int next = neighbour(maze, cell, d);
    maze.open[cell] = static_cast<uint8_t>(maze.open[cell] | (1u << d));
    maze.open[next] = static_cast<uint8_t>(maze.open[next] | (1u << ((d + 2) % 4)));
    visited[next] = true;
    stack[depth++] = static_cast<int16_t>(next);
  }

  // The walker starts in a corner and the goal is the cell farthest from it.
  int16_t distance[kMaxCells];
  int16_t parent[kMaxCells];
  maze.at = 0;
  flood(maze, 0, distance, parent);
  maze.goal = cells - 1;
  for (int i = 0; i < cells; ++i) {
    if (distance[i] > distance[maze.goal]) maze.goal = i;
  }
  maze.seen[0] = true;

  // Stars wait at dead ends off the way, so fetching them is a detour.
  if (options[OptStars] == 0) {
    bool onWay[kMaxCells] = {};
    for (int c = maze.goal; c >= 0; c = parent[c]) onWay[c] = true;
    int ends[kMaxCells];
    int endCount = 0;
    for (int i = 1; i < cells; ++i) {
      const uint8_t o = maze.open[i];
      const bool deadEnd = o == 1 || o == 2 || o == 4 || o == 8;
      if (deadEnd && !onWay[i] && i != maze.goal) ends[endCount++] = i;
    }
    shuffle(ends, endCount, rng);
    for (int i = 0; i < endCount && maze.starCount < kMaxStars; ++i) maze.stars[maze.starCount++] = ends[i];
  }
}

int walkTowards(Maze& maze, const int cell) {
  if (maze.done() || cell < 0 || cell >= maze.w * maze.h || cell == maze.at) return 0;
  const int dx = cell % maze.w - maze.at % maze.w;
  const int dy = cell / maze.w - maze.at / maze.w;
  const bool sideways = (dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy);
  const Dir dir = sideways ? (dx > 0 ? East : West) : (dy > 0 ? South : North);
  int want = sideways ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
  int taken = 0;
  while (want-- > 0 && maze.canGo(maze.at, dir)) {
    maze.at = neighbour(maze, maze.at, dir);
    maze.seen[maze.at] = true;
    ++taken;
    for (int& star : maze.stars) {
      if (star == maze.at) {
        star = -1;
        ++maze.taken;
      }
    }
    if (maze.done()) break;
  }
  maze.steps += taken;
  return taken;
}

int hint(const Maze& maze, int* cells, const int max) {
  int16_t distance[kMaxCells];
  int16_t parent[kMaxCells];
  // From the goal outwards, so each cell's parent is its next step home.
  flood(maze, maze.goal, distance, parent);
  int n = 0;
  for (int c = parent[maze.at]; c >= 0 && n < max; c = parent[c]) cells[n++] = c;
  return n;
}

}  // namespace maze

// --- PUNTS ---------------------------------------------------------------------

namespace dots {

namespace {
constexpr const char* kLevelNames[] = {"POCS PUNTS", "UNS QUANTS", "MOLTS PUNTS"};
constexpr const char* kLabelNames[] = {"1 2 3", "A B C", "2 4 6", "SENSE"};
constexpr const char* kOrderNames[] = {"A L'ATZAR", "PER ORDRE"};
}  // namespace

const Option kOptions[OptCount] = {
    {"DIFICULTAT", kLevelNames, 3},
    {"ELS PUNTS DIUEN", kLabelNames, 4},
    {"DIBUIXOS", kOrderNames, 2},
    {"MARCA EL SEGUENT", kYesFirst, 2},
};

void start(Game& game, const uint8_t* options, kids::Rng& rng, const int previous, const int figures,
           const uint8_t* dotCounts) {
  game = Game{};
  game.level = options[OptLevel] % 3;
  if (options[OptOrder]) {
    game.figure = (previous + 1) % figures;
  } else {
    do {
      game.figure = rng.below(figures);
    } while (figures > 1 && game.figure == previous);
  }
  game.count = dotCounts[game.figure * 3 + game.level];
  // The first dot is where the pencil starts.
  game.joined = 0;
}

bool tap(Game& game, const int index) {
  if (game.done() || index != game.joined) return false;
  ++game.joined;
  return true;
}

void label(const uint8_t* options, const int index, const int count, char* out, const int cap) {
  Labels labels = static_cast<Labels>(options[OptLabels] % 4);
  // The alphabet runs out before a big figure does.
  if (labels == Labels::Letters && count > 26) labels = Labels::Numbers;
  if (cap < 4) return;
  switch (labels) {
    case Labels::Letters:
      out[0] = static_cast<char>('A' + index % 26);
      out[1] = 0;
      break;
    case Labels::Twos:
      std::snprintf(out, static_cast<size_t>(cap), "%d", ((index + 1) * 2) % 1000);
      break;
    case Labels::None:
      out[0] = 0;
      break;
    case Labels::Numbers:
    default:
      std::snprintf(out, static_cast<size_t>(cap), "%d", (index + 1) % 1000);
      break;
  }
}

}  // namespace dots

void packOptions(const uint8_t* values, const int count, uint8_t* bytes) {
  for (int i = 0; i < count; ++i) bytes[i] = values[i];
}

void unpackOptions(const uint8_t* bytes, const int size, const Option* options, const int count, uint8_t* values) {
  for (int i = 0; i < count; ++i) values[i] = i < size && bytes[i] < options[i].count ? bytes[i] : 0;
}

}  // namespace kidsmore

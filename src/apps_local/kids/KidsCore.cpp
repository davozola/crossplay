#include "KidsCore.h"

namespace kids {

namespace {

constexpr const char* kWords[kPictures] = {
    "GAT",     "GOS",      "PEIX",   "OCELL",     "CONILL", "TORTUGA", "CARGOL", "ESQUIROL", "RATA",
    "CUC",     "POMA",     "CIRERA", "PASTANAGA", "COTXE",  "TREN",    "BICI",   "VAIXELL",  "COET",
    "TRACTOR", "SOL",      "LLUNA",  "ESTRELLA",  "FLOR",   "ARBRE",   "CASA",   "COR",      "CLAU",
    "GELAT",   "OU",       "LLIBRE", "LLIT",      "ULL",    "FULLA",   "FOC",    "GOTA",     "CAMPANA",
    "REGAL",   "GUITARRA", "GALETA", "PIZZA",     "DAU",    "MAPA",    "NOTA",   "IMANT",    "JOC",
};

// One picture a letter, the plainest word for it.
struct Letter {
  char letter;
  int8_t picture;
};
constexpr Letter kLetters[] = {
    {'A', 23}, {'B', 15}, {'C', 24}, {'D', 40}, {'E', 21}, {'F', 22}, {'G', 0},  {'I', 43}, {'J', 44}, {'L', 20},
    {'M', 41}, {'N', 42}, {'O', 3},  {'P', 10}, {'R', 8},  {'S', 19}, {'T', 14}, {'U', 31}, {'V', 16},
};
constexpr int kLetterCount = sizeof(kLetters) / sizeof(kLetters[0]);

}  // namespace

const char* wordOf(const int picture) { return picture >= 0 && picture < kPictures ? kWords[picture] : ""; }

const char* gameName(const Game game) {
  switch (game) {
    case Game::Find:
      return "ON ES?";
    case Game::Match:
      return "PARELLES";
    case Game::Letters:
      return "LLETRES";
    case Game::Count:
      return "NOMBRES";
    case Game::Mole:
      return "EL TALP";
    case Game::Cards:
    default:
      return "DIBUIXOS";
  }
}

int gamePicture(const Game game) {
  switch (game) {
    case Game::Find:
      return 31;  // ULL
    case Game::Match:
      return 25;  // COR
    case Game::Letters:
      return 29;  // LLIBRE
    case Game::Count:
      return 40;  // DAU
    case Game::Mole:
      return -1;  // its own mole, from MoleArt
    case Game::Cards:
    default:
      return 0;  // GAT
  }
}

uint32_t Rng::next() {
  // xorshift32; a zero state would stick, so it is never allowed to be one.
  if (state == 0) state = 0x9E3779B9u;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

int Rng::below(const int n) { return n <= 1 ? 0 : static_cast<int>(next() % static_cast<uint32_t>(n)); }

bool Round::choose(const int option) {
  if (solved || option < 0 || option >= optionCount) return false;
  if (option == answer) {
    solved = true;
    return true;
  }
  wrongMask = static_cast<uint8_t>(wrongMask | (1u << option));
  return false;
}

Round dealPictures(Rng& rng, int optionCount, const int avoid) {
  if (optionCount < 2) optionCount = 2;
  if (optionCount > kMaxOptions) optionCount = kMaxOptions;
  Round r;
  r.optionCount = optionCount;
  do {
    r.target = rng.below(kPictures);
  } while (r.target == avoid);
  r.answer = rng.below(optionCount);
  for (int i = 0; i < optionCount; ++i) {
    if (i == r.answer) {
      r.options[i] = r.target;
      continue;
    }
    for (;;) {
      const int pick = rng.below(kPictures);
      bool taken = pick == r.target;
      for (int k = 0; k < i; ++k) taken = taken || r.options[k] == pick;
      if (!taken) {
        r.options[i] = pick;
        break;
      }
    }
  }
  return r;
}

Round dealCount(Rng& rng, const int avoidCount) {
  Round r;
  r.optionCount = 3;
  r.target = rng.below(kPictures);
  do {
    r.count = 1 + rng.below(kMaxCount);
  } while (r.count == avoidCount);
  // Three numbers in a row that include the count, wherever in the three it
  // can sit without leaving 1..kMaxCount.
  int low = r.count - rng.below(3);
  if (low < 1) low = 1;
  if (low + 2 > kMaxCount) low = kMaxCount - 2;
  for (int i = 0; i < 3; ++i) {
    r.options[i] = low + i;
    if (r.options[i] == r.count) r.answer = i;
  }
  return r;
}

int letterCount() { return kLetterCount; }
char letterAt(const int index) { return index >= 0 && index < kLetterCount ? kLetters[index].letter : '?'; }
int pictureOfLetter(const int index) { return index >= 0 && index < kLetterCount ? kLetters[index].picture : 0; }

int step(const int current, const int direction, const int size) {
  if (size <= 0) return 0;
  return ((current + direction) % size + size) % size;
}

}  // namespace kids

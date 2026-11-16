#include "TacticsCore.h"

#include <cmath>
#include <cstring>

namespace tactics {

namespace {

// The bit order of a record's themes. tools_local/tactics/make_pack.py holds
// the same list.
constexpr const char* kThemeNames[kThemes] = {
    "MATE",       "MATE IN 1",    "MATE IN 2",    "MATE IN 3",     "FORK",         "PIN",           "SKEWER",
    "DISCOVERED", "DOUBLE CHECK", "SACRIFICE",    "DEFLECTION",    "ATTRACTION",   "HANGING PIECE", "TRAPPED PIECE",
    "BACK RANK",  "SMOTHERED",    "PROMOTION",    "ENDGAME",       "MIDDLEGAME",   "OPENING",       "QUIET MOVE",
    "DEFENSIVE",  "ZUGZWANG",     "INTERMEZZO",   "CLEARANCE",     "INTERFERENCE", "X-RAY",         "REMOVE DEFENDER",
    "KINGSIDE",   "QUEENSIDE",    "EXPOSED KING", "ADVANCED PAWN",
};

uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// The legal move between two squares, promoting to `promotion` when it is one
// (a queen when none is named).
bool legalMove(const chess::Position& position, const int from, const int to, const uint8_t promotion,
               chess::Move& out) {
  chess::MoveList list;
  chess::generateLegalMoves(position, list);
  const uint8_t want = promotion != chess::Empty ? promotion : static_cast<uint8_t>(chess::Queen);
  for (int i = 0; i < list.count; ++i) {
    const chess::Move& m = list.moves[i];
    if (m.from != from || m.to != to) continue;
    if ((m.flags & chess::FlagPromotion) && m.promotion != want) continue;
    out = m;
    return true;
  }
  return false;
}

bool isMate(const chess::Position& position) {
  if (!chess::isInCheck(position)) return false;
  chess::MoveList list;
  chess::generateLegalMoves(position, list);
  return list.count == 0;
}

void play(Run& run, const chess::Move& move) {
  chess::Undo undo;
  chess::makeMove(run.position, move, undo);
  run.last = move;
  run.hasLast = true;
}

// After a right move at `step`: the opponent's answer, or the end.
Outcome advance(Run& run) {
  ++run.step;
  if (run.step >= run.puzzle.moveCount) {
    run.done = true;
    return Outcome::Solved;
  }
  chess::Move answer;
  const chess::Move& raw = run.puzzle.moves[run.step];
  if (!legalMove(run.position, raw.from, raw.to, raw.promotion, answer)) {
    // A pack that disagrees with the rules: stop here rather than go on wrong.
    run.done = true;
    return Outcome::Solved;
  }
  play(run, answer);
  ++run.step;
  if (run.step >= run.puzzle.moveCount) {
    run.done = true;
    return Outcome::Solved;
  }
  return Outcome::Right;
}

}  // namespace

bool readHeader(const uint8_t* bytes, const size_t size, Header& out) {
  if (size < kHeaderBytes || std::memcmp(bytes, "TACT", 4) != 0) return false;
  if (u16(bytes + 4) != 1 || u16(bytes + 6) != kRecordBytes) return false;
  out.count = u32(bytes + 8);
  std::memcpy(out.name, bytes + 12, sizeof(out.name));
  out.name[sizeof(out.name) - 1] = 0;
  return out.count > 0;
}

uint16_t ratingOf(const uint8_t* record) { return u16(record + 8); }

const char* themeName(const int theme) { return theme >= 0 && theme < kThemes ? kThemeNames[theme] : ""; }

bool decode(const uint8_t* r, Puzzle& out) {
  out = Puzzle{};
  for (int i = 0; i < 5; ++i) {
    if (r[i] < 0x21 || r[i] > 0x7e) return false;
    out.id[i] = static_cast<char>(r[i]);
  }
  const uint8_t flags = r[5];
  const uint8_t epFile = r[6];
  out.moveCount = r[7];
  out.rating = u16(r + 8);
  out.themes = u32(r + 10);
  if (out.moveCount < 2 || out.moveCount > kMaxMoves || (out.moveCount & 1) != 0) return false;
  chess::Position& p = out.start;
  int kings[2] = {0, 0};
  for (int i = 0; i < 64; ++i) {
    const uint8_t nibble = (r[16 + i / 2] >> ((i & 1) * 4)) & 0x0F;
    const uint8_t type = nibble & 7;
    if (type == 7) return false;
    p.squares[i] = type == 0 ? static_cast<uint8_t>(chess::Empty) : nibble;
    if (type == chess::King) ++kings[(nibble & 8) ? 1 : 0];
  }
  if (kings[0] != 1 || kings[1] != 1) return false;
  p.whiteToMove = (flags & 1) != 0;
  p.castling = (flags >> 1) & 0x0F;
  p.epSquare = epFile < 8 ? static_cast<chess::Square>((p.whiteToMove ? 5 : 2) * 8 + epFile) : chess::kNoSquare;
  for (int i = 0; i < out.moveCount; ++i) {
    const uint16_t w = u16(r + 48 + i * 2);
    out.moves[i].from = w & 63;
    out.moves[i].to = (w >> 6) & 63;
    out.moves[i].promotion = (w >> 12) & 7;
  }
  // The opponent's first move has to be one, or the rest means nothing.
  chess::Move first;
  return legalMove(p, out.moves[0].from, out.moves[0].to, out.moves[0].promotion, first);
}

void begin(Run& run, const Puzzle& puzzle) {
  run = Run{};
  run.puzzle = puzzle;
  run.position = puzzle.start;
  chess::Move first;
  if (!legalMove(run.position, puzzle.moves[0].from, puzzle.moves[0].to, puzzle.moves[0].promotion, first)) {
    run.done = true;
    return;
  }
  play(run, first);
  run.step = 1;
}

bool whiteSolves(const Run& run) { return !run.puzzle.start.whiteToMove; }

bool wanted(const Run& run, chess::Move& out) {
  if (run.done || run.step >= run.puzzle.moveCount) return false;
  const chess::Move& raw = run.puzzle.moves[run.step];
  return legalMove(run.position, raw.from, raw.to, raw.promotion, out);
}

Outcome tryMove(Run& run, const int from, const int to) {
  if (run.done) return Outcome::NotAMove;
  chess::Move want;
  if (!wanted(run, want)) return Outcome::NotAMove;
  chess::Move played;
  // The promotion wanted, when the squares are the wanted ones; a queen otherwise.
  const bool same = want.from == from && want.to == to;
  if (!legalMove(run.position, from, to, same ? want.promotion : static_cast<uint8_t>(chess::Empty), played)) {
    return Outcome::NotAMove;
  }
  if (!same) {
    // Any mate ends a puzzle whose last move is one.
    if (run.step + 1 == run.puzzle.moveCount) {
      chess::Position after = run.position;
      chess::Undo undo;
      chess::makeMove(after, played, undo);
      if (isMate(after)) {
        play(run, played);
        run.step = run.puzzle.moveCount;
        run.done = true;
        return Outcome::Solved;
      }
    }
    run.missed = true;
    return Outcome::Wrong;
  }
  play(run, played);
  return advance(run);
}

Outcome showMove(Run& run) {
  chess::Move want;
  if (!wanted(run, want)) return Outcome::NotAMove;
  run.missed = true;
  play(run, want);
  return advance(run);
}

int ratedAfter(const int rating, const int puzzleRating, const bool solved, const int played) {
  const double expected = 1.0 / (1.0 + std::pow(10.0, (puzzleRating - rating) / 400.0));
  const double k = played < 20 ? 32.0 : 20.0;
  int next = rating + static_cast<int>(std::lround(k * ((solved ? 1.0 : 0.0) - expected)));
  if (next < 400) next = 400;
  if (next > 3000) next = 3000;
  return next;
}

uint32_t idKey(const char* id) {
  uint32_t key = 2166136261u;
  for (int i = 0; i < 5 && id[i] != 0; ++i) key = (key ^ static_cast<uint8_t>(id[i])) * 16777619u;
  return key != 0 ? key : 1;
}

bool seenLately(const Stats& stats, const char* id) {
  const uint32_t key = idKey(id);
  for (const uint32_t seen : stats.recent) {
    if (seen == key) return true;
  }
  return false;
}

void record(Stats& stats, const Puzzle& puzzle, const bool solved, const bool rated) {
  stats.recent[stats.recentAt % kRecent] = idKey(puzzle.id);
  stats.recentAt = static_cast<uint8_t>((stats.recentAt + 1) % kRecent);
  if (!rated) return;
  const int played = stats.solved + stats.missed;
  stats.rating = static_cast<int16_t>(ratedAfter(stats.rating, puzzle.rating, solved, played));
  if (stats.rating > stats.peak) stats.peak = stats.rating;
  if (solved) {
    if (stats.solved < 65535) ++stats.solved;
    if (stats.streak < 65535) ++stats.streak;
    if (stats.streak > stats.bestStreak) stats.bestStreak = stats.streak;
  } else {
    if (stats.missed < 65535) ++stats.missed;
    stats.streak = 0;
  }
  for (int t = 0; t < kThemes; ++t) {
    if ((puzzle.themes & (1u << t)) == 0) continue;
    if (stats.themeSeen[t] < 65535) ++stats.themeSeen[t];
    if (solved && stats.themeSolved[t] < 65535) ++stats.themeSolved[t];
  }
  if (stats.historyCount == kHistory) {
    for (int i = 1; i < kHistory; ++i) stats.history[i - 1] = stats.history[i];
    --stats.historyCount;
  }
  stats.history[stats.historyCount++] = stats.rating;
}

bool dailyDone(const Stats& stats, const int32_t day) { return day > 0 && stats.dailyDay == day; }

void recordDaily(Stats& stats, const int32_t day, const bool solved) {
  if (day <= 0 || stats.dailyDay == day) return;
  const bool yesterday = stats.dailyDay == day - 1 && stats.dailySolved != 0;
  stats.dailyDay = day;
  stats.dailySolved = solved ? 1 : 0;
  if (!solved) {
    stats.dailyStreak = 0;
  } else {
    stats.dailyStreak = static_cast<uint16_t>(yesterday ? stats.dailyStreak + 1 : 1);
  }
}

namespace {
void put16(uint8_t*& o, const uint16_t v) {
  *o++ = static_cast<uint8_t>(v & 0xFF);
  *o++ = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t*& o, const uint32_t v) {
  put16(o, static_cast<uint16_t>(v & 0xFFFF));
  put16(o, static_cast<uint16_t>(v >> 16));
}
uint16_t get16(const uint8_t*& i) {
  const uint16_t v = u16(i);
  i += 2;
  return v;
}
uint32_t get32(const uint8_t*& i) {
  const uint32_t v = u32(i);
  i += 4;
  return v;
}
}  // namespace

size_t packStats(const Stats& s, uint8_t* out, const size_t cap) {
  if (cap < kStatsBytes) return 0;
  uint8_t* o = out;
  *o++ = 'T';
  *o++ = 1;
  put16(o, static_cast<uint16_t>(s.rating));
  put16(o, static_cast<uint16_t>(s.peak));
  put16(o, s.solved);
  put16(o, s.missed);
  put16(o, s.streak);
  put16(o, s.bestStreak);
  put32(o, static_cast<uint32_t>(s.dailyDay));
  *o++ = s.dailySolved;
  put16(o, s.dailyStreak);
  for (int t = 0; t < kThemes; ++t) put16(o, s.themeSeen[t]);
  for (int t = 0; t < kThemes; ++t) put16(o, s.themeSolved[t]);
  for (int i = 0; i < kHistory; ++i) put16(o, static_cast<uint16_t>(s.history[i]));
  *o++ = s.historyCount;
  for (int i = 0; i < kRecent; ++i) put32(o, s.recent[i]);
  *o++ = s.recentAt;
  return static_cast<size_t>(o - out);
}

bool unpackStats(const uint8_t* in, const size_t size, Stats& out) {
  if (size != kStatsBytes || in[0] != 'T' || in[1] != 1) return false;
  Stats s;
  const uint8_t* i = in + 2;
  s.rating = static_cast<int16_t>(get16(i));
  s.peak = static_cast<int16_t>(get16(i));
  s.solved = get16(i);
  s.missed = get16(i);
  s.streak = get16(i);
  s.bestStreak = get16(i);
  s.dailyDay = static_cast<int32_t>(get32(i));
  s.dailySolved = *i++;
  s.dailyStreak = get16(i);
  for (int t = 0; t < kThemes; ++t) s.themeSeen[t] = get16(i);
  for (int t = 0; t < kThemes; ++t) s.themeSolved[t] = get16(i);
  for (int k = 0; k < kHistory; ++k) s.history[k] = static_cast<int16_t>(get16(i));
  s.historyCount = *i++;
  for (int k = 0; k < kRecent; ++k) s.recent[k] = get32(i);
  s.recentAt = *i++;
  if (s.rating < 400 || s.rating > 3000 || s.peak < s.rating || s.historyCount > kHistory || s.recentAt >= kRecent) {
    return false;
  }
  out = s;
  return true;
}

const char* focusName(const Focus focus) {
  static constexpr const char* kNames[kFocuses] = {"EVERYTHING", "MATES", "FORKS AND PINS", "ENDGAMES", "SACRIFICES"};
  const int i = static_cast<int>(focus);
  return kNames[i >= 0 && i < kFocuses ? i : 0];
}

bool fits(const Focus focus, const uint32_t themes) {
  switch (focus) {
    case Focus::Mates:
      return (themes & 0x0000C00Fu) != 0;  // mate, mate in 1-3, back rank, smothered
    case Focus::Forks:
      return (themes & 0x000000F0u) != 0;  // fork, pin, skewer, discovered
    case Focus::Endgames:
      return (themes & (1u << 17)) != 0;
    case Focus::Sacrifices:
      return (themes & ((1u << 9) | (1u << 10) | (1u << 11))) != 0;
    case Focus::Everything:
    default:
      return true;
  }
}

uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  if (state == 0) state = 0x9E3779B9u;
  return state;
}

uint32_t dailyPick(const int32_t day, const uint32_t first, const uint32_t past) {
  if (past <= first) return first;
  uint32_t h = static_cast<uint32_t>(day) * 2654435761u;
  h ^= h >> 15;
  h *= 2246822519u;
  h ^= h >> 13;
  return first + h % (past - first);
}

}  // namespace tactics

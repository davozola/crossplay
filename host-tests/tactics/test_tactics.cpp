#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "TacticsCore.h"

using namespace tactics;

namespace {
int failures = 0;
int checks = 0;
void check(const bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL: %s\n", what);
  }
}

// A record as make_pack.py writes it, from a FEN's parts already in hand.
void makeRecord(uint8_t* r, const char* id, const char* fen, const uint16_t rating, const uint32_t themes,
                const std::vector<const char*>& moves) {
  std::memset(r, 0, kRecordBytes);
  std::memcpy(r, id, 5);
  chess::Position p;
  chess::parseFen(fen, p);
  r[5] = static_cast<uint8_t>((p.whiteToMove ? 1 : 0) | (p.castling << 1));
  r[6] = p.epSquare == chess::kNoSquare ? 255 : static_cast<uint8_t>(p.epSquare & 7);
  r[7] = static_cast<uint8_t>(moves.size());
  r[8] = static_cast<uint8_t>(rating & 0xFF);
  r[9] = static_cast<uint8_t>(rating >> 8);
  for (int i = 0; i < 4; ++i) r[10 + i] = static_cast<uint8_t>(themes >> (8 * i));
  for (int i = 0; i < 64; ++i) r[16 + i / 2] |= static_cast<uint8_t>((p.squares[i] & 0x0F) << ((i & 1) * 4));
  for (size_t i = 0; i < moves.size(); ++i) {
    const char* m = moves[i];
    const int from = (m[0] - 'a') + (m[1] - '1') * 8;
    const int to = (m[2] - 'a') + (m[3] - '1') * 8;
    int promo = 0;
    if (m[4] == 'q') promo = chess::Queen;
    if (m[4] == 'n') promo = chess::Knight;
    const uint16_t w = static_cast<uint16_t>(from | (to << 6) | (promo << 12));
    r[48 + i * 2] = static_cast<uint8_t>(w & 0xFF);
    r[49 + i * 2] = static_cast<uint8_t>(w >> 8);
  }
}

int sq(const char* s) { return (s[0] - 'a') + (s[1] - '1') * 8; }
}  // namespace

int main(int argc, char** argv) {
  {  // one puzzle, start to finish
    uint8_t rec[kRecordBytes];
    // Lichess 0009B: black plays bxc5, and white wins the knight on g4.
    makeRecord(rec, "0009B", "r2qr1k1/b1p2ppp/pp4n1/P1P1p3/4P1n1/B2P2Pb/3NBP1P/RN1QR1K1 b - - 1 16", 1103, 1u << 18,
               {"b6c5", "e2g4", "h3g4", "d1g4"});
    Puzzle p;
    check(decode(rec, p), "decodes");
    check(std::strcmp(p.id, "0009B") == 0 && p.rating == 1103 && p.moveCount == 4 && p.themes == (1u << 18), "fields");
    check(ratingOf(rec) == 1103, "the rating is read without decoding");
    Run run;
    begin(run, p);
    check(run.step == 1 && whiteSolves(run) && run.position.whiteToMove, "the opponent has moved, white solves");
    check(run.position.squares[sq("c5")] == (chess::Pawn | chess::BlackFlag), "bxc5 is on the board");
    check(tryMove(run, sq("e2"), sq("e5")) == Outcome::NotAMove && !run.missed, "an illegal move is not a miss");
    check(tryMove(run, sq("d1"), sq("c2")) == Outcome::Wrong && run.missed && run.step == 1, "a legal wrong one is");
    check(run.position.squares[sq("d1")] == chess::Queen, "and the board stays as it was");
    check(tryMove(run, sq("e2"), sq("g4")) == Outcome::Right && run.step == 3, "right: the answer is played too");
    check(run.position.squares[sq("g4")] == (chess::Bishop | chess::BlackFlag), "the bishop took back");
    chess::Move want;
    check(wanted(run, want) && want.from == sq("d1") && want.to == sq("g4"), "what is wanted next");
    check(tryMove(run, sq("d1"), sq("g4")) == Outcome::Solved && run.done, "solved");
    check(!wanted(run, want) && tryMove(run, sq("g4"), sq("g5")) == Outcome::NotAMove, "nothing after the end");

    begin(run, p);
    check(showMove(run) == Outcome::Right && run.missed && run.step == 3, "the answer shown, a move at a time");
    check(showMove(run) == Outcome::Solved, "to the end");

    rec[7] = 3;
    check(!decode(rec, p), "an odd number of moves is not a puzzle");
    rec[7] = 4;
    rec[48] = 0;
    rec[49] = 0;
    check(!decode(rec, p), "nor one whose first move cannot be played");
  }

  {  // any mate ends a mate
    uint8_t rec[kRecordBytes];
    // After ...Kh8 white mates with Ra8 (wanted) or equally with Rb8.
    makeRecord(rec, "MATE1", "6k1/5ppp/8/8/8/8/8/RR4K1 b - - 0 1", 600, 3, {"g8h8", "a1a8"});
    Puzzle p;
    check(decode(rec, p), "the mate decodes");
    Run run;
    begin(run, p);
    check(tryMove(run, sq("b1"), sq("b8")) == Outcome::Solved && !run.missed, "the other mate counts");
    begin(run, p);
    check(tryMove(run, sq("a1"), sq("a7")) == Outcome::Wrong, "a move that does not mate does not");
  }

  {  // promotion: the wanted piece, without asking
    uint8_t rec[kRecordBytes];
    makeRecord(rec, "PROMO", "8/4P1k1/8/8/8/8/6K1/8 b - - 0 1", 900, 1u << 16, {"g7f7", "e7e8n"});
    Puzzle p;
    check(decode(rec, p), "the promotion decodes");
    Run run;
    begin(run, p);
    check(tryMove(run, sq("e7"), sq("e8")) == Outcome::Solved, "the squares are enough");
    check(run.position.squares[sq("e8")] == chess::Knight, "and it is the knight that was wanted");
  }

  {  // the rating
    check(ratedAfter(1000, 1000, true, 0) == 1016 && ratedAfter(1000, 1000, false, 0) == 984, "even odds, K 32");
    check(ratedAfter(1000, 1000, true, 30) == 1010, "K 20 once settled");
    check(ratedAfter(1000, 1400, true, 30) > ratedAfter(1000, 1000, true, 30), "a harder one is worth more");
    check(ratedAfter(1000, 600, false, 30) < ratedAfter(1000, 1000, false, 30), "an easy miss costs more");
    check(ratedAfter(400, 2000, false, 0) == 400 && ratedAfter(3000, 400, true, 0) == 3000, "kept in range");
  }

  {  // the numbers kept
    Stats s;
    Puzzle p;
    std::snprintf(p.id, sizeof(p.id), "AAAAA");
    p.rating = 1000;
    p.themes = (1u << 4) | (1u << 18);
    record(s, p, true, true);
    check(s.rating == 1016 && s.peak == 1016 && s.solved == 1 && s.streak == 1 && s.historyCount == 1, "a solve");
    check(s.themeSeen[4] == 1 && s.themeSolved[4] == 1 && s.themeSeen[5] == 0, "counted under its themes");
    check(seenLately(s, "AAAAA") && !seenLately(s, "BBBBB"), "and remembered");
    record(s, p, false, true);
    check(s.rating < 1016 && s.peak == 1016 && s.missed == 1 && s.streak == 0 && s.bestStreak == 1, "a miss");
    const int16_t before = s.rating;
    record(s, p, true, false);
    check(s.rating == before && s.solved == 1, "an unrated one changes nothing but the memory");
    for (int i = 0; i < 100; ++i) record(s, p, true, true);
    check(s.historyCount == kHistory && s.history[kHistory - 1] == s.rating, "the history keeps the newest");

    recordDaily(s, 20000, true);
    check(dailyDone(s, 20000) && !dailyDone(s, 20001) && s.dailyStreak == 1, "a daily");
    recordDaily(s, 20000, false);
    check(s.dailySolved == 1, "only once a day");
    recordDaily(s, 20001, true);
    check(s.dailyStreak == 2, "two days running");
    recordDaily(s, 20003, true);
    check(s.dailyStreak == 1, "a day skipped starts again");
    recordDaily(s, 20004, false);
    check(s.dailyStreak == 0, "and a miss ends it");

    uint8_t bytes[kStatsBytes];
    check(packStats(s, bytes, sizeof(bytes)) == kStatsBytes && packStats(s, bytes, 10) == 0, "pack size");
    Stats back;
    check(unpackStats(bytes, sizeof(bytes), back) && back.rating == s.rating && back.solved == s.solved &&
              back.dailyDay == 20004 && back.themeSeen[4] == s.themeSeen[4] && back.history[3] == s.history[3] &&
              seenLately(back, "AAAAA"),
          "round trip");
    bytes[1] = 9;
    check(!unpackStats(bytes, sizeof(bytes), back), "another version is not read");
  }

  {  // choosing
    const int ratings[8] = {400, 600, 600, 900, 1200, 1200, 1500, 2000};
    auto at = [&](const uint32_t i) { return ratings[i]; };
    check(lowerBound(8, 0, at) == 0 && lowerBound(8, 600, at) == 1 && lowerBound(8, 601, at) == 3 &&
              lowerBound(8, 2001, at) == 8,
          "the first at or past a rating");
    check(dailyPick(20000, 10, 50) == dailyPick(20000, 10, 50) && dailyPick(20000, 10, 50) >= 10 &&
              dailyPick(20000, 10, 50) < 50 && dailyPick(20000, 7, 7) == 7,
          "the day's pick is the day's");
    int differing = 0;
    for (int d = 0; d < 30; ++d) differing += dailyPick(20000 + d, 0, 1000) != dailyPick(20001 + d, 0, 1000);
    check(differing >= 28, "and changes with the day");
    check(fits(Focus::Everything, 0) && fits(Focus::Mates, 1u << 1) && !fits(Focus::Mates, 1u << 4) &&
              fits(Focus::Forks, 1u << 5) && fits(Focus::Endgames, 1u << 17) && fits(Focus::Sacrifices, 1u << 9),
          "what a focus takes");
    check(std::strcmp(themeName(4), "FORK") == 0 && std::strcmp(themeName(17), "ENDGAME") == 0 && *themeName(40) == 0,
          "theme names");
  }

  // Every puzzle of the pack: decodes, and its own line solves it.
  if (argc > 1) {
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> pack((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Header header;
    check(readHeader(pack.data(), pack.size(), header), "the pack's header");
    check(pack.size() == kHeaderBytes + static_cast<size_t>(header.count) * kRecordBytes, "the pack's size");
    int bad = 0;
    int unsorted = 0;
    int stuck = 0;
    uint16_t previous = 0;
    for (uint32_t i = 0; i < header.count; ++i) {
      const uint8_t* rec = pack.data() + kHeaderBytes + static_cast<size_t>(i) * kRecordBytes;
      Puzzle p;
      if (!decode(rec, p)) {
        ++bad;
        continue;
      }
      if (p.rating < previous) ++unsorted;
      previous = p.rating;
      Run run;
      begin(run, p);
      int guard = 0;
      while (!run.done && guard++ < kMaxMoves) {
        chess::Move want;
        if (!wanted(run, want) || tryMove(run, want.from, want.to) == Outcome::Wrong) break;
      }
      if (!run.done || run.missed) ++stuck;
    }
    std::printf("pack: %u puzzles, %d undecodable, %d unsolvable\n", header.count, bad, stuck);
    check(bad == 0, "every record decodes");
    check(unsorted == 0, "sorted by rating");
    check(stuck == 0, "every puzzle is solved by its own line");
  }

  if (failures == 0) {
    std::printf("tactics: all tests passed (%d checks)\n", checks);
    return 0;
  }
  return 1;
}

#include "TacticsActivity.h"

#include <HalDisplay.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../chess/ChessPieces.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace tc = tactics;

namespace {

enum : fui::ActionId {
  ActionTrain = 1,
  ActionDaily,
  ActionFocus,
  ActionStats,
  ActionHint,
  ActionShow,
  ActionNext,
  ActionHome,
};

constexpr char kPackPath[] = "/tactics/puzzles.pack";
constexpr char kStatsPath[] = "/.crosspoint/tactics.sav";
constexpr uint8_t kCleanAfterPaints = 12;
constexpr int kBottomReserve = 40;
constexpr int kWindow = 90;  // rating points either side of yours a puzzle is drawn from

const uint8_t* solidFor(const uint8_t type) {
  switch (type) {
    case chess::Pawn:
      return chessart::kPawnSolid;
    case chess::Knight:
      return chessart::kKnightSolid;
    case chess::Bishop:
      return chessart::kBishopSolid;
    case chess::Rook:
      return chessart::kRookSolid;
    case chess::Queen:
      return chessart::kQueenSolid;
    case chess::King:
      return chessart::kKingSolid;
    default:
      return nullptr;
  }
}

const uint8_t* outlineFor(const uint8_t type) {
  switch (type) {
    case chess::Pawn:
      return chessart::kPawnLight;
    case chess::Knight:
      return chessart::kKnightLight;
    case chess::Bishop:
      return chessart::kBishopLight;
    case chess::Rook:
      return chessart::kRookLight;
    case chess::Queen:
      return chessart::kQueenLight;
    case chess::King:
      return chessart::kKingLight;
    default:
      return nullptr;
  }
}

// A light piece is its silhouette knocked out to paper and then outlined, so
// a dithered square does not show through it.
void drawPiece(const GfxRenderer& renderer, const uint8_t piece, const int x, const int y) {
  const uint8_t* solid = solidFor(chess::pieceType(piece));
  const uint8_t* outline = outlineFor(chess::pieceType(piece));
  if (solid == nullptr || outline == nullptr) return;
  if (chess::isBlack(piece)) {
    toybox::blit1bpp(renderer, solid, chessart::kPieceSize, x, y, true);
    return;
  }
  toybox::blit1bpp(renderer, solid, chessart::kPieceSize, x, y, false);
  toybox::blit1bpp(renderer, outline, chessart::kPieceSize, x, y, true);
}

// "+12" / "-8"
void signedNumber(const int value, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s%d", value >= 0 ? "+" : "-", (value >= 0 ? value : -value) % 10000);
}

}  // namespace

std::unique_ptr<Activity> TacticsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TacticsActivity>(renderer, mappedInput);
}

void TacticsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  rng_ = toybox::seed() | 1u;
  loadStats();
  openPack();
  view_ = View::Home;
  cleanNext_ = true;
  requestUpdate();
}

void TacticsActivity::onExit() {
  if (packOpen_) file_.close();
  packOpen_ = false;
  Activity::onExit();
}

int32_t TacticsActivity::today() const {
  const time_t now = time(nullptr);
  return now > 1700000000 ? static_cast<int32_t>(now / 86400) : 0;
}

void TacticsActivity::loadStats() {
  stats_ = tc::Stats{};
  focus_ = tc::Focus::Everything;
  if (!Storage.exists(kStatsPath)) return;
  HalFile file;
  if (!Storage.openFileForRead("TACT", kStatsPath, file)) return;
  uint8_t bytes[tc::kStatsBytes + 1];
  if (file.read(bytes, sizeof(bytes)) != static_cast<int>(sizeof(bytes))) return;
  if (!tc::unpackStats(bytes, tc::kStatsBytes, stats_)) return;
  if (bytes[tc::kStatsBytes] < tc::kFocuses) focus_ = static_cast<tc::Focus>(bytes[tc::kStatsBytes]);
}

void TacticsActivity::saveStats() {
  uint8_t bytes[tc::kStatsBytes + 1];
  if (tc::packStats(stats_, bytes, tc::kStatsBytes) == 0) return;
  bytes[tc::kStatsBytes] = static_cast<uint8_t>(focus_);
  HalFile file;
  if (!Storage.openFileForWrite("TACT", kStatsPath, file)) return;
  file.write(bytes, sizeof(bytes));
  file.flush();
}

bool TacticsActivity::openPack() {
  packOpen_ = false;
  pack_ = tc::Header{};
  if (!Storage.exists(kPackPath)) return false;
  if (!Storage.openFileForRead("TACT", kPackPath, file_)) return false;
  uint8_t header[tc::kHeaderBytes];
  if (file_.read(header, sizeof(header)) != static_cast<int>(sizeof(header)) ||
      !tc::readHeader(header, sizeof(header), pack_) ||
      file_.fileSize() < tc::kHeaderBytes + static_cast<size_t>(pack_.count) * tc::kRecordBytes) {
    LOG_ERR("TACT", "Not a pack this reads: %s", kPackPath);
    file_.close();
    pack_ = tc::Header{};
    return false;
  }
  packOpen_ = true;
  return true;
}

bool TacticsActivity::readRecord(const uint32_t index, uint8_t* record) {
  if (!packOpen_ || index >= pack_.count) return false;
  if (!file_.seek(tc::kHeaderBytes + static_cast<size_t>(index) * tc::kRecordBytes)) return false;
  return file_.read(record, tc::kRecordBytes) == static_cast<int>(tc::kRecordBytes);
}

// A puzzle within the window around `rating` that fits the focus and has not
// been seen lately. The window widens when it holds too few.
bool TacticsActivity::pickNear(const int rating, tc::Puzzle& out) {
  uint8_t record[tc::kRecordBytes];
  auto ratingAt = [&](const uint32_t i) { return readRecord(i, record) ? tc::ratingOf(record) : 0; };
  for (int window = kWindow; window <= kWindow * 16; window *= 2) {
    const uint32_t first = tc::lowerBound(pack_.count, rating - window, ratingAt);
    const uint32_t past = tc::lowerBound(pack_.count, rating + window + 1, ratingAt);
    if (past <= first) continue;
    const uint32_t span = past - first;
    for (int attempt = 0; attempt < 40; ++attempt) {
      const uint32_t index = first + tc::nextRandom(rng_) % span;
      if (!readRecord(index, record)) return false;
      tc::Puzzle candidate;
      if (!tc::decode(record, candidate)) continue;
      if (!tc::fits(focus_, candidate.themes)) continue;
      // With few to choose from, one seen lately is better than none.
      if (span > tc::kRecent * 2 && attempt < 30 && tc::seenLately(stats_, candidate.id)) continue;
      out = candidate;
      return true;
    }
  }
  return false;
}

void TacticsActivity::startTraining() {
  tc::Puzzle puzzle;
  if (!pickNear(stats_.rating, puzzle)) {
    said_ = "NO PUZZLE FOUND FOR THAT FOCUS.";
    return;
  }
  tc::begin(run_, puzzle);
  daily_ = false;
  settled_ = false;
  delta_ = 0;
  selected_ = -1;
  said_ = "";
  view_ = View::Solve;
  cleanNext_ = true;
}

void TacticsActivity::startDaily() {
  const int32_t day = today();
  if (day == 0 || tc::dailyDone(stats_, day)) return;
  // The day's puzzle is the same for everyone with this pack: from the middle
  // of it, where most people can have a go.
  uint8_t record[tc::kRecordBytes];
  auto ratingAt = [&](const uint32_t i) { return readRecord(i, record) ? tc::ratingOf(record) : 0; };
  const uint32_t first = tc::lowerBound(pack_.count, 1100, ratingAt);
  uint32_t past = tc::lowerBound(pack_.count, 1700, ratingAt);
  if (past <= first) past = pack_.count;
  tc::Puzzle puzzle;
  const uint32_t pick = tc::dailyPick(day, past > first ? first : 0, past);
  if (!readRecord(pick, record) || !tc::decode(record, puzzle)) return;
  tc::begin(run_, puzzle);
  daily_ = true;
  settled_ = false;
  delta_ = 0;
  selected_ = -1;
  said_ = "";
  view_ = View::Solve;
  cleanNext_ = true;
}

// Counts the puzzle, once: at the first miss, or when it is solved clean.
void TacticsActivity::settle(const bool solved) {
  if (settled_) return;
  settled_ = true;
  const int16_t before = stats_.rating;
  tc::record(stats_, run_.puzzle, solved, true);
  delta_ = static_cast<int16_t>(stats_.rating - before);
  if (daily_) tc::recordDaily(stats_, today(), solved);
  saveStats();
}

void TacticsActivity::after(const tc::Outcome outcome) {
  switch (outcome) {
    case tc::Outcome::Wrong:
      settle(false);
      said_ = "NOT THAT ONE. LOOK AGAIN.";
      break;
    case tc::Outcome::Right:
      said_ = run_.missed ? "AND NOW?" : "RIGHT. KEEP GOING.";
      break;
    case tc::Outcome::Solved:
      settle(!run_.missed);
      said_ = run_.missed ? "THAT WAS THE LINE." : "SOLVED.";
      cleanNext_ = true;
      break;
    case tc::Outcome::NotAMove:
    default:
      break;
  }
}

void TacticsActivity::tapBoard(const int square) {
  if (run_.done || square < 0) return;
  const uint8_t piece = run_.position.squares[square];
  const bool mine = chess::isColour(piece, run_.position.whiteToMove);
  if (selected_ < 0) {
    if (mine) selected_ = static_cast<int8_t>(square);
    return;
  }
  if (square == selected_) {
    selected_ = -1;
    return;
  }
  const tc::Outcome outcome = tc::tryMove(run_, selected_, square);
  if (outcome == tc::Outcome::NotAMove) {
    // Another of your own pieces is a change of mind; anything else is dropped.
    selected_ = mine ? static_cast<int8_t>(square) : static_cast<int8_t>(-1);
    return;
  }
  selected_ = -1;
  after(outcome);
}

int TacticsActivity::squareAt(const int tapX, const int tapY) const {
  if (cell_ <= 0 || tapX < boardX_ || tapY < boardY_) return -1;
  const int col = (tapX - boardX_) / cell_;
  const int row = (tapY - boardY_) / cell_;
  if (col > 7 || row > 7) return -1;
  const bool white = tc::whiteSolves(run_);
  const int file = white ? col : 7 - col;
  const int rank = white ? 7 - row : row;
  return rank * 8 + file;
}

void TacticsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::Home) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    view_ = View::Home;
    said_ = "";
    cleanNext_ = true;
    requestUpdate();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  if (view_ == View::Solve) {
    const int square = squareAt(tapX, tapY);
    if (square >= 0) {
      tapBoard(square);
      requestUpdate();
      return;
    }
  }
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  route(interactions_.route(input));
}

void TacticsActivity::route(const fui::ActionEvent& event) {
  switch (event.action) {
    case ActionTrain:
      if (!packOpen_) return;
      startTraining();
      break;
    case ActionDaily:
      if (!packOpen_) return;
      startDaily();
      break;
    case ActionFocus:
      focus_ = static_cast<tc::Focus>((static_cast<int>(focus_) + 1) % tc::kFocuses);
      said_ = "";
      saveStats();
      break;
    case ActionStats:
      view_ = View::Stats;
      cleanNext_ = true;
      break;
    case ActionHint:
      if (view_ != View::Solve || run_.done) return;
      // A hint is a miss: the puzzle no longer counts as solved.
      run_.missed = true;
      settle(false);
      if (run_.hints < 2) ++run_.hints;
      selected_ = -1;
      said_ = run_.hints == 1 ? "THIS PIECE MOVES." : "IT GOES THERE.";
      break;
    case ActionShow:
      if (view_ != View::Solve || run_.done) return;
      settle(false);
      selected_ = -1;
      run_.hints = 0;
      after(tc::showMove(run_));
      break;
    case ActionNext:
      if (view_ != View::Solve) return;
      if (daily_) {
        view_ = View::Home;
        cleanNext_ = true;
      } else {
        startTraining();
      }
      break;
    case ActionHome:
      view_ = View::Home;
      said_ = "";
      cleanNext_ = true;
      break;
    default:
      return;
  }
  requestUpdate();
}

void TacticsActivity::buildHome(toybox::Screen& screen) {
  lab::chrome(screen, "TACTICS");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  int y = toybox::kChromeHeight + 20;

  if (!packOpen_) {
    lab::label(screen, lab::rect(x, y + 30, inner, toybox::kDisplayCut.inkHeight), "NO PUZZLES YET",
               toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Left);
    lab::paragraph(screen, x, y + 30 + toybox::kDisplayCut.inkHeight + 24, inner,
                   "COPY A PACK TO THE CARD AS /TACTICS/PUZZLES.PACK AND COME BACK. THE STARTER PACK IS IN THE "
                   "REPOSITORY UNDER TOOLS_LOCAL/TACTICS, WITH THE SCRIPT THAT MAKES BIGGER ONES FROM THE LICHESS "
                   "PUZZLE DATABASE.",
                   6);
    return;
  }

  // The rating, as big as the page takes it.
  lab::caption(screen, x, y, inner, "YOUR RATING");
  char text[48];
  std::snprintf(text, sizeof(text), "PEAK %d", stats_.peak % 10000);
  lab::caption(screen, x, y, inner, text, fui::TextAlign::Right, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 14;
  std::snprintf(text, sizeof(text), "%d", stats_.rating % 10000);
  renderer.drawText(toybox::kHugeFontId, x, y, text, true);
  y += toybox::kHugeCut.ascender + 16;

  const int played = stats_.solved + stats_.missed;
  std::snprintf(text, sizeof(text), "%d SOLVED OF %d", stats_.solved % 100000, played % 100000);
  lab::caption(screen, x, y, inner, text);
  std::snprintf(text, sizeof(text), "STREAK %d", stats_.streak % 100000);
  lab::caption(screen, x, y, inner, text, fui::TextAlign::Right);
  y += toybox::kTileCut.inkHeight + 14;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 18;

  lab::button(screen, lab::rect(x, y, inner, 76), "TRAIN", ActionTrain, 0, true, false);
  y += 76 + 12;

  const int32_t day = today();
  char dailyLabel[40];
  if (day == 0) {
    std::snprintf(dailyLabel, sizeof(dailyLabel), "DAILY: SET THE CLOCK FIRST");
  } else if (tc::dailyDone(stats_, day)) {
    std::snprintf(dailyLabel, sizeof(dailyLabel), "DAILY %s. BACK TOMORROW", stats_.dailySolved ? "SOLVED" : "MISSED");
  } else {
    std::snprintf(dailyLabel, sizeof(dailyLabel), "TODAY'S PUZZLE");
  }
  fui::ButtonProps daily;
  daily.label = dailyLabel;
  daily.action = ActionDaily;
  daily.styles = toybox::rowStyles();
  daily.text = toybox::buttonText(screen.theme());
  daily.radius = lab::kRadius;
  daily.enabled = day != 0 && !tc::dailyDone(stats_, day);
  screen.button(daily, lab::rect(x, y, inner, 56));
  y += 56 + 8;
  if (stats_.dailyStreak > 1) {
    std::snprintf(text, sizeof(text), "%d DAYS RUNNING", stats_.dailyStreak % 100000);
    lab::caption(screen, x, y, inner, text, fui::TextAlign::Center, fui::Color::DarkGray);
  }
  y += toybox::kTileCut.inkHeight + 14;

  char focusLabel[40];
  std::snprintf(focusLabel, sizeof(focusLabel), "FOCUS: %s", tc::focusName(focus_));
  lab::button(screen, lab::rect(x, y, inner, 56), focusLabel, ActionFocus, 0);
  y += 56 + 12;
  lab::button(screen, lab::rect(x, y, inner, 56), "STATS", ActionStats, 0);
  y += 56 + 14;
  if (said_[0] != 0) lab::caption(screen, x, y, inner, said_, fui::TextAlign::Center);

  std::snprintf(text, sizeof(text), "%s: %d PUZZLES", pack_.name, static_cast<int>(pack_.count % 10000000));
  lab::caption(screen, x, screen.device().height - kBottomReserve - toybox::kTileCut.inkHeight - 8, inner, text,
               fui::TextAlign::Center, fui::Color::DarkGray);
}

void TacticsActivity::drawBoard(const int x, const int y, const int cell) const {
  const bool white = tc::whiteSolves(run_);
  chess::Move want;
  const bool hasWant = tc::wanted(run_, want);
  // Where the picked-up piece can go.
  bool target[64] = {};
  if (selected_ >= 0) {
    chess::MoveList list;
    chess::generateLegalMoves(run_.position, list);
    for (int i = 0; i < list.count; ++i) {
      if (list.moves[i].from == selected_) target[list.moves[i].to] = true;
    }
  }
  for (int square = 0; square < 64; ++square) {
    const int file = square & 7;
    const int rank = square >> 3;
    const int col = white ? file : 7 - file;
    const int row = white ? 7 - rank : rank;
    const int sx = x + col * cell;
    const int sy = y + row * cell;
    if (((file + rank) & 1) == 0) renderer.fillRectDither(sx, sy, cell, cell, Color::LightGray);
    // The move just played: a frame on both its squares.
    if (run_.hasLast && (square == run_.last.from || square == run_.last.to)) {
      renderer.drawRect(sx + 1, sy + 1, cell - 2, cell - 2, 2, true);
    }
    const uint8_t piece = run_.position.squares[square];
    if (square == selected_) {
      renderer.fillRect(sx, sy, cell, cell, false);
      renderer.drawRect(sx, sy, cell, cell, 4, true);
    }
    if (hasWant && ((run_.hints >= 1 && square == want.from) || (run_.hints >= 2 && square == want.to))) {
      // A hint: the square cleared and ringed twice.
      renderer.fillRect(sx + 3, sy + 3, cell - 6, cell - 6, false);
      renderer.drawRoundedRect(sx + 2, sy + 2, cell - 4, cell - 4, 2, 8, true);
      renderer.drawRoundedRect(sx + 7, sy + 7, cell - 14, cell - 14, 1, 6, true);
    }
    if (piece != chess::Empty) {
      const int inset = (cell - chessart::kPieceSize) / 2;
      drawPiece(renderer, piece, sx + inset, sy + inset);
    }
    if (target[square]) {
      // A dot on an empty square, corners on one that would be taken.
      if (piece == chess::Empty) {
        renderer.fillRoundedRect(sx + cell / 2 - 6, sy + cell / 2 - 6, 12, 12, 6, Color::Black);
      } else {
        renderer.fillRect(sx, sy, 12, 4, true);
        renderer.fillRect(sx, sy, 4, 12, true);
        renderer.fillRect(sx + cell - 12, sy + cell - 4, 12, 4, true);
        renderer.fillRect(sx + cell - 4, sy + cell - 12, 4, 12, true);
        renderer.fillRect(sx + cell - 12, sy, 12, 4, true);
        renderer.fillRect(sx + cell - 4, sy, 4, 12, true);
        renderer.fillRect(sx, sy + cell - 4, 12, 4, true);
        renderer.fillRect(sx, sy + cell - 12, 4, 12, true);
      }
    }
  }
  renderer.drawRect(x - 2, y - 2, cell * 8 + 4, cell * 8 + 4, 2, true);
}

void TacticsActivity::buildSolve(toybox::Screen& screen) {
  lab::chrome(screen, daily_ ? "TODAY'S PUZZLE" : "TACTICS");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  int y = toybox::kChromeHeight + 14;
  const bool white = tc::whiteSolves(run_);

  // Who is to move, with a counter of that colour, and your rating beside it.
  const int dot = toybox::kUiCut.inkHeight;
  if (white) {
    renderer.drawRoundedRect(x, y, dot, dot, 2, dot / 2, true);
  } else {
    renderer.fillRoundedRect(x, y, dot, dot, dot / 2, Color::Black);
  }
  lab::value(screen, x + dot + 10, y, inner - dot - 10, white ? "WHITE TO MOVE" : "BLACK TO MOVE");
  char text[64];
  std::snprintf(text, sizeof(text), "YOU %d", stats_.rating % 10000);
  lab::caption(screen, x, y + 6, inner, text, fui::TextAlign::Right, fui::Color::DarkGray);
  y += toybox::kUiCut.inkHeight + 14;

  cell_ = inner / 8;
  boardX_ = x + (inner - cell_ * 8) / 2;
  boardY_ = y + 2;
  drawBoard(boardX_, boardY_, cell_);
  // Files along the foot, from the solver's side.
  y = boardY_ + cell_ * 8 + 8;
  for (int col = 0; col < 8; ++col) {
    const char name[2] = {static_cast<char>('A' + (white ? col : 7 - col)), 0};
    lab::caption(screen, boardX_ + col * cell_, y, cell_, name, fui::TextAlign::Center, fui::Color::DarkGray);
  }
  y += toybox::kTileCut.inkHeight + 16;

  // What happened, and once it is over, what the puzzle was.
  lab::value(screen, x, y, inner, said_[0] != 0 ? said_ : "FIND THE BEST MOVE.");
  y += toybox::kUiCut.inkHeight + 12;
  if (settled_) {
    char change[toybox::kIntChars + 2];
    signedNumber(delta_, change, sizeof(change));
    std::snprintf(text, sizeof(text), "PUZZLE %d   YOU %s = %d", run_.puzzle.rating % 10000, change,
                  stats_.rating % 10000);
    lab::caption(screen, x, y, inner, text);
  }
  y += toybox::kTileCut.inkHeight + 10;
  if (run_.done) {
    // Up to three of its themes.
    char themes[72] = "";
    int named = 0;
    for (int t = 4; t < tc::kThemes + 4 && named < 3; ++t) {
      const int theme = t % tc::kThemes;
      if ((run_.puzzle.themes & (1u << theme)) == 0) continue;
      const size_t at = std::strlen(themes);
      std::snprintf(themes + at, sizeof(themes) - at, "%s%s", named ? ", " : "", tc::themeName(theme));
      ++named;
    }
    lab::caption(screen, x, y, inner, themes, fui::TextAlign::Left, fui::Color::DarkGray);
  }

  const int footH = 56;
  const int footY = screen.device().height - kBottomReserve - footH - 6;
  const int gap = 10;
  const int half = (inner - gap) / 2;
  if (run_.done) {
    lab::button(screen, lab::rect(x, footY, half, footH), "HOME", ActionHome, 0);
    lab::button(screen, lab::rect(x + half + gap, footY, half, footH), daily_ ? "DONE" : "NEXT", ActionNext, 0, true);
  } else {
    lab::button(screen, lab::rect(x, footY, half, footH), run_.hints == 0 ? "HINT" : "MORE HINT", ActionHint, 0);
    lab::button(screen, lab::rect(x + half + gap, footY, half, footH), "SHOW THE MOVE", ActionShow, 0);
  }
}

void TacticsActivity::buildStats(toybox::Screen& screen) {
  lab::chrome(screen, "STATS");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  int y = toybox::kChromeHeight + 18;
  char text[64];

  // Four numbers across.
  const int played = stats_.solved + stats_.missed;
  const int colW = inner / 4;
  const char* const names[4] = {"RATING", "PEAK", "SOLVED", "RIGHT"};
  char values[4][toybox::kIntChars + 2];
  std::snprintf(values[0], sizeof(values[0]), "%d", stats_.rating % 10000);
  std::snprintf(values[1], sizeof(values[1]), "%d", stats_.peak % 10000);
  std::snprintf(values[2], sizeof(values[2]), "%d", stats_.solved % 100000);
  std::snprintf(values[3], sizeof(values[3]), "%d%%", played > 0 ? stats_.solved * 100 / played : 0);
  for (int i = 0; i < 4; ++i) {
    lab::caption(screen, x + i * colW, y, colW, names[i], fui::TextAlign::Left, fui::Color::DarkGray);
    lab::value(screen, x + i * colW, y + toybox::kTileCut.inkHeight + 8, colW, values[i]);
  }
  y += toybox::kTileCut.inkHeight + 8 + toybox::kUiCut.inkHeight + 16;
  std::snprintf(text, sizeof(text), "STREAK %d   BEST %d   DAILY %d DAYS", stats_.streak % 100000,
                stats_.bestStreak % 100000, stats_.dailyStreak % 100000);
  lab::caption(screen, x, y, inner, text);
  y += toybox::kTileCut.inkHeight + 14;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 14;

  // The rating over the last puzzles.
  lab::caption(screen, x, y, inner, "RATING, LAST PUZZLES", fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 8;
  const int plotH = 110;
  if (stats_.historyCount >= 2) {
    int lo = stats_.history[0];
    int hi = lo;
    for (int i = 1; i < stats_.historyCount; ++i) {
      if (stats_.history[i] < lo) lo = stats_.history[i];
      if (stats_.history[i] > hi) hi = stats_.history[i];
    }
    const int span = hi - lo > 20 ? hi - lo : 20;
    auto px = [&](const int i) { return x + 4 + (inner - 8) * i / (stats_.historyCount - 1); };
    auto py = [&](const int i) { return y + plotH - 6 - (plotH - 12) * (stats_.history[i] - lo) / span; };
    for (int i = 0; i + 1 < stats_.historyCount; ++i) renderer.drawLine(px(i), py(i), px(i + 1), py(i + 1), 3, true);
    std::snprintf(text, sizeof(text), "%d", hi % 10000);
    lab::caption(screen, x, y, inner, text, fui::TextAlign::Right, fui::Color::DarkGray);
    std::snprintf(text, sizeof(text), "%d", lo % 10000);
    lab::caption(screen, x, y + plotH - toybox::kTileCut.inkHeight, inner, text, fui::TextAlign::Right,
                 fui::Color::DarkGray);
  } else {
    lab::caption(screen, x, y + plotH / 2, inner, "SOLVE A FEW AND IT SHOWS HERE.", fui::TextAlign::Center);
  }
  y += plotH + 8;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 14;

  // The themes met most, and how often each was got right.
  lab::caption(screen, x, y, inner, "BY THEME", fui::TextAlign::Left, fui::Color::DarkGray);
  lab::caption(screen, x, y, inner, "RIGHT OF SEEN", fui::TextAlign::Right, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 10;
  bool listed[tc::kThemes] = {};
  const int bottom = screen.device().height - kBottomReserve - 8;
  const int rowH = 34;
  int rows = 0;
  while (y + rowH <= bottom) {
    int best = -1;
    for (int t = 0; t < tc::kThemes; ++t) {
      // Where in the game it was says little; the motif is what is practised.
      if (listed[t] || stats_.themeSeen[t] == 0 || (t >= 17 && t <= 19)) continue;
      if (best < 0 || stats_.themeSeen[t] > stats_.themeSeen[best]) best = t;
    }
    if (best < 0) break;
    listed[best] = true;
    ++rows;
    const int seen = stats_.themeSeen[best];
    const int right = stats_.themeSolved[best];
    lab::caption(screen, x, y + (rowH - toybox::kTileCut.inkHeight) / 2 - 4, 170, tc::themeName(best));
    const int barX = x + 176;
    const int barW = inner - 176 - 86;
    renderer.drawRect(barX, y + 6, barW, 14, 1, true);
    renderer.fillRect(barX, y + 6, barW * right / seen, 14, true);
    std::snprintf(text, sizeof(text), "%d / %d", right % 100000, seen % 100000);
    lab::caption(screen, x, y + (rowH - toybox::kTileCut.inkHeight) / 2 - 4, inner, text, fui::TextAlign::Right);
    y += rowH;
  }
  if (rows == 0) lab::caption(screen, x, y + 10, inner, "NOTHING YET.", fui::TextAlign::Center);
}

void TacticsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::Home:
      buildHome(screen);
      break;
    case View::Solve:
      buildSolve(screen);
      break;
    case View::Stats:
      buildStats(screen);
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Tactics");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!cleanNext_ && ++fastPaints_ >= kCleanAfterPaints) cleanNext_ = true;
  if (cleanNext_) fastPaints_ = 0;
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}

#include "ReversiActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;

namespace {

enum : fui::ActionId {
  ActionOption = 1,  // + the option's index
  ActionPlay = 20,
  ActionNewGame,
  ActionOptions,
  ActionContinue,
  ActionUndo,
  ActionHint,
};

enum { OptOpponent, OptBoard, OptColour, OptHints };
struct Option {
  const char* name;
  const char* const* values;
  uint8_t count;
};
constexpr const char* kOpponents[] = {"DEVICE, EASY", "DEVICE, MEDIUM", "DEVICE, HARD", "DEVICE, EXPERT",
                                      "TWO PLAYERS"};
constexpr int kTwoPlayers = 4;
constexpr const char* kBoards[] = {"8 BY 8", "6 BY 6"};
constexpr const char* kColours[] = {"BLACK, MOVES FIRST", "WHITE"};
constexpr const char* kYesNo[] = {"YES", "NO"};
constexpr Option kOptionTable[] = {
    {"OPPONENT", kOpponents, 5},
    {"BOARD", kBoards, 2},
    {"YOU PLAY", kColours, 2},
    {"SHOW WHERE YOU CAN GO", kYesNo, 2},
};
constexpr char kOptionsPath[] = "/.crosspoint/reversi.cfg";
constexpr char kGamePath[] = "/.crosspoint/reversi-game.sav";
constexpr char kStatsPath[] = "/.crosspoint/reversi-levels.cfg";
constexpr uint8_t kCleanAfterPaints = 14;

void disc(const GfxRenderer& r, const int cx, const int cy, const int radius, const bool black) {
  for (int y = -radius; y <= radius; ++y) {
    const int half = static_cast<int>(std::lround(std::sqrt(static_cast<float>(radius * radius - y * y))));
    r.fillRect(cx - half, cy + y, half * 2 + 1, 1, black);
  }
}

}  // namespace

std::unique_ptr<Activity> ReversiActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ReversiActivity>(renderer, mappedInput);
}

void ReversiActivity::loadOptions() {
  uint8_t bytes[kOptions] = {};
  int got = 0;
  HalFile file;
  if (Storage.exists(kOptionsPath) && Storage.openFileForRead("RVS", kOptionsPath, file)) {
    got = file.read(bytes, sizeof(bytes));
  }
  for (int i = 0; i < kOptions; ++i) options_[i] = i < got && bytes[i] < kOptionTable[i].count ? bytes[i] : 0;
}

void ReversiActivity::saveOptions() {
  HalFile file;
  if (!Storage.openFileForWrite("RVS", kOptionsPath, file)) return;
  file.write(options_, sizeof(options_));
  file.flush();
}

void ReversiActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  rng_.state = static_cast<uint32_t>(millis()) * 2654435761u + 1u;
  loadOptions();
  sologame::loadStats("RVS", kStatsPath, stats_);
  hasSaved_ = Storage.exists(kGamePath);
  view_ = View::Setup;
  requestUpdate();
}

int ReversiActivity::pageBottom() const {
  return renderer.getScreenHeight() - (mappedInput.hasTouch() ? 8 : lab::kBottomReserve + 6);
}

void ReversiActivity::newGame() {
  reversi::start(board_, options_[OptBoard] == 0 ? 8 : 6);
  note_ = "";
  clean_ = true;
  history_.clear();
  hint_ = -1;
  recorded_ = false;
  sologame::drop(kGamePath);
  hasSaved_ = false;
}

void ReversiActivity::saveGame() {
  if (reversi::over(board_)) return;
  hasSaved_ = sologame::save("RVS", kGamePath, board_, history_, options_[OptOpponent], options_[OptColour]);
}

void ReversiActivity::continueGame() {
  uint8_t opponent = options_[OptOpponent];
  uint8_t colour = options_[OptColour];
  if (!sologame::load("RVS", kGamePath, board_, history_, opponent, colour) || (board_.side != 6 && board_.side != 8) ||
      reversi::over(board_)) {
    newGame();
    return;
  }
  // The game goes on against who, and as what, it was begun.
  if (opponent < kOptionTable[OptOpponent].count) options_[OptOpponent] = opponent;
  if (colour < kOptionTable[OptColour].count) options_[OptColour] = colour;
  options_[OptBoard] = board_.side == 8 ? 0 : 1;
  note_ = "";
  hint_ = -1;
  recorded_ = false;
  clean_ = true;
}

// Back to before the player's last move, the device's answer undone with it.
void ReversiActivity::takeBack() {
  if (deviceToMove() || !history_.pop(board_)) return;
  note_ = "";
  hint_ = -1;
  recorded_ = false;
  clean_ = true;
  saveGame();
}

// The square the expert would play for the side to move.
void ReversiActivity::showHint() {
  if (deviceToMove() || reversi::over(board_)) return;
  hint_ = reversi::choose(board_, reversi::kLevels - 1, rng_);
}

// A finished game against the device is counted against the level it was
// played at, once, and is no longer one to continue.
void ReversiActivity::recordIfOver() {
  if (recorded_ || !reversi::over(board_)) return;
  recorded_ = true;
  sologame::drop(kGamePath);
  hasSaved_ = false;
  history_.clear();
  if (options_[OptOpponent] == kTwoPlayers) return;
  const uint8_t mine = options_[OptColour] == 0 ? reversi::Black : reversi::White;
  const int me = board_.count(mine);
  const int them = board_.count(reversi::other(mine));
  stats_.count(options_[OptOpponent], me > them ? 0 : (me < them ? 1 : 2));
  sologame::saveStats("RVS", kStatsPath, stats_);
}

bool ReversiActivity::deviceToMove() const {
  if (options_[OptOpponent] == kTwoPlayers || reversi::over(board_)) return false;
  const uint8_t mine = options_[OptColour] == 0 ? reversi::Black : reversi::White;
  return board_.turn != mine;
}

void ReversiActivity::deviceMoves() {
  const int at = reversi::choose(board_, options_[OptOpponent], rng_);
  if (at < 0) return;
  const reversi::Played played = reversi::play(board_, at);
  note_ = played == reversi::Played::YesAndOtherPasses ? "YOU HAVE NO MOVE: THE DEVICE GOES AGAIN" : "";
  // Kept after every answer, so leaving at any point loses nothing.
  saveGame();
  recordIfOver();
}

void ReversiActivity::boardRect(int& x, int& y, int& cell) const {
  const int width = renderer.getScreenWidth();
  cell = (width - 2 * toybox::kMargin) / board_.side;
  x = (width - cell * board_.side) / 2;
  y = 96;
}

void ReversiActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::Play) {
      view_ = View::Setup;
      clean_ = true;
      requestUpdate();
      return;
    }
    shelf::leave(renderer, mappedInput);
    return;
  }

  // The device answers one pass after the move it answers was painted.
  if (view_ == View::Play && deviceToMove()) {
    deviceMoves();
    requestUpdateAndWait();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;

  if (view_ == View::Play && !reversi::over(board_)) {
    int bx = 0;
    int by = 0;
    int cell = 0;
    boardRect(bx, by, cell);
    if (tapX >= bx && tapY >= by && tapX < bx + cell * board_.side && tapY < by + cell * board_.side) {
      const int at = ((tapY - by) / cell) * board_.side + (tapX - bx) / cell;
      const reversi::Board before = board_;
      const reversi::Played played = reversi::play(board_, at);
      if (played == reversi::Played::No) {
        haptic_feedback::touchAction();
        return;
      }
      history_.push(before);
      hint_ = -1;
      saveGame();
      recordIfOver();
      note_ = "";
      if (played == reversi::Played::YesAndOtherPasses) {
        note_ = options_[OptOpponent] == kTwoPlayers ? "NO MOVE FOR THE OTHER SIDE: GO AGAIN"
                                                     : "THE DEVICE HAS NO MOVE: GO AGAIN";
      }
      // Painted before the device thinks, so the move is seen first.
      requestUpdateAndWait();
      return;
    }
  }

  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);
  if (event.action == ActionPlay || event.action == ActionNewGame) {
    newGame();
    view_ = View::Play;
  } else if (event.action == ActionContinue) {
    continueGame();
    view_ = View::Play;
  } else if (event.action == ActionUndo && view_ == View::Play) {
    takeBack();
  } else if (event.action == ActionHint && view_ == View::Play) {
    showHint();
  } else if (event.action == ActionOptions) {
    view_ = View::Setup;
    clean_ = true;
  } else if (event.action >= ActionOption && event.action < ActionOption + kOptions && view_ == View::Setup) {
    const int i = event.action - ActionOption;
    options_[i] = static_cast<uint8_t>((options_[i] + 1) % kOptionTable[i].count);
    saveOptions();
  } else {
    return;
  }
  requestUpdate();
}

void ReversiActivity::buildSetup(toybox::Screen& screen) {
  lab::chrome(screen, "REVERSI");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  int y = toybox::kChromeHeight + 14;
  const int rowH = 78;
  for (int i = 0; i < kOptions; ++i) {
    // Against the device the colour matters; with two players it does not.
    lab::button(screen, lab::rect(x, y, inner, rowH), "", static_cast<fui::ActionId>(ActionOption + i), 0, false,
                false);
    lab::caption(screen, x + 18, y + 12, inner - 36, kOptionTable[i].name);
    lab::value(screen, x + 18, y + 12 + toybox::kTileCut.inkHeight + 10, inner - 36,
               kOptionTable[i].values[options_[i]]);
    y += rowH + 10;
  }
  lab::paragraph(
      screen, x, y + 8, inner,
      "PUT A STONE WHERE IT TRAPS THE OTHER COLOUR BETWEEN TWO OF YOURS. EVERYTHING TRAPPED TURNS OVER. MOST "
      "STONES AT THE END WINS.",
      4);
  // How it stands against the opponent chosen.
  if (options_[OptOpponent] != kTwoPlayers) {
    char record[64];
    std::snprintf(record, sizeof(record), "AGAINST %s: %d WON, %d LOST, %d DRAWN", kOpponents[options_[OptOpponent]],
                  stats_.won[options_[OptOpponent]] % 10000, stats_.lost[options_[OptOpponent]] % 10000,
                  stats_.drawn[options_[OptOpponent]] % 10000);
    lab::caption(screen, x, pageBottom() - 90 - toybox::kTileCut.inkHeight - 12, inner, record);
  }
  if (hasSaved_) {
    const int half = (inner - 8) / 2;
    lab::button(screen, lab::rect(x, pageBottom() - 90, half, 90), "CONTINUE", ActionContinue, 0, true, false);
    lab::button(screen, lab::rect(x + half + 8, pageBottom() - 90, half, 90), "NEW GAME", ActionPlay, 0, false, false);
  } else {
    lab::button(screen, lab::rect(x, pageBottom() - 90, inner, 90), "PLAY", ActionPlay, 0, true, false);
  }
}

void ReversiActivity::buildPlay(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - 2 * toybox::kMargin;
  const bool over = reversi::over(board_);
  const bool two = options_[OptOpponent] == kTwoPlayers;
  const uint8_t mine = options_[OptColour] == 0 ? reversi::Black : reversi::White;
  const int black = board_.count(reversi::Black);
  const int white = board_.count(reversi::White);

  // The score: a stone and a count each side, the side to move underlined.
  const int half = inner / 2;
  for (int side = 0; side < 2; ++side) {
    const uint8_t colour = side == 0 ? reversi::Black : reversi::White;
    const int sx = x + side * half;
    disc(renderer, sx + 28, 44, 22, true);
    if (colour == reversi::White) disc(renderer, sx + 28, 44, 18, false);
    char count[toybox::kIntChars + 1];
    std::snprintf(count, sizeof(count), "%d", (colour == reversi::Black ? black : white) % 1000);
    lab::label(screen, lab::rect(sx + 62, 44 - toybox::kDisplayCut.inkHeight / 2, 90, toybox::kDisplayCut.inkHeight),
               count, toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Left);
    const char* who = two ? (side == 0 ? "BLACK" : "WHITE") : (colour == mine ? "YOU" : "DEVICE");
    lab::caption(screen, sx + 150, 38, half - 150, who);
    if (!over && board_.turn == colour) renderer.fillRect(sx, 76, half - 12, 5, true);
  }

  int bx = 0;
  int by = 0;
  int cell = 0;
  boardRect(bx, by, cell);
  const int side = board_.side;
  const int span = cell * side;
  for (int i = 0; i <= side; ++i) {
    const int weight = i == 0 || i == side ? 4 : 2;
    renderer.fillRect(bx + i * cell - weight / 2, by - 2, weight, span + 4, true);
    renderer.fillRect(bx - 2, by + i * cell - weight / 2, span + 4, weight, true);
  }
  const bool hints = options_[OptHints] == 0 && !over && !deviceToMove();
  const int radius = cell / 2 - 6;
  for (int at = 0; at < side * side; ++at) {
    const int cx = bx + (at % side) * cell + cell / 2;
    const int cy = by + (at / side) * cell + cell / 2;
    const uint8_t stone = board_.cell[at];
    if (stone == reversi::Black) {
      disc(renderer, cx, cy, radius, true);
      if (at == board_.last) disc(renderer, cx, cy, 4, false);
    } else if (stone == reversi::White) {
      disc(renderer, cx, cy, radius, true);
      disc(renderer, cx, cy, radius - 4, false);
      if (at == board_.last) disc(renderer, cx, cy, 4, true);
    } else if (at == hint_ && !over) {
      // The hinted square: a ring the size of a stone.
      disc(renderer, cx, cy, radius, true);
      disc(renderer, cx, cy, radius - 3, false);
      disc(renderer, cx, cy, 6, true);
    } else if (hints && reversi::legal(board_, at, board_.turn)) {
      disc(renderer, cx, cy, 5, true);
    }
  }

  const int below = by + span + 18;
  char line[64];
  const char* status = note_;
  if (over) {
    if (black == white) {
      std::snprintf(line, sizeof(line), "A DRAW, %d ALL", black % 100);
    } else {
      const uint8_t winner = black > white ? reversi::Black : reversi::White;
      const char* name = two ? (winner == reversi::Black ? "BLACK WINS" : "WHITE WINS")
                             : (winner == mine ? "YOU WIN" : "THE DEVICE WINS");
      std::snprintf(line, sizeof(line), "%s, %d TO %d", name, (black > white ? black : white) % 100,
                    (black > white ? white : black) % 100);
    }
    status = line;
  } else if (status[0] == 0) {
    status = deviceToMove() ? "THE DEVICE IS THINKING"
             : two          ? (board_.turn == reversi::Black ? "BLACK TO MOVE" : "WHITE TO MOVE")
                            : "YOUR MOVE";
  }
  lab::label(screen, lab::rect(x, below, inner, toybox::kUiCut.inkHeight), status, toybox::kUiFont, toybox::kUiCut);

  const int buttonH = 60;
  if (!two && !over) {
    // Against the device: take a move back and ask for one, beside the two that were here.
    const int quarter = (inner - 8 * 3) / 4;
    const int by2 = pageBottom() - buttonH;
    lab::button(screen, lab::rect(x, by2, quarter, buttonH), "UNDO", ActionUndo, 0, false);
    lab::button(screen, lab::rect(x + (quarter + 8), by2, quarter, buttonH), "HINT", ActionHint, 0, false);
    lab::button(screen, lab::rect(x + (quarter + 8) * 2, by2, quarter, buttonH), "NEW", ActionNewGame, 0, false);
    lab::button(screen, lab::rect(x + (quarter + 8) * 3, by2, inner - (quarter + 8) * 3, buttonH), "OPTIONS",
                ActionOptions, 0, false);
  } else {
    const int half2 = (inner - 8) / 2;
    lab::button(screen, lab::rect(x, pageBottom() - buttonH, half2, buttonH), "NEW GAME", ActionNewGame, 0, over,
                false);
    lab::button(screen, lab::rect(x + half2 + 8, pageBottom() - buttonH, half2, buttonH), "OPTIONS", ActionOptions, 0,
                false, false);
  }
}

void ReversiActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  if (view_ == View::Setup) {
    buildSetup(screen);
  } else {
    buildPlay(screen);
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Reversi");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  // Stones turning leave ghosts on a fast paint; every so often a clean one.
  if (!clean_ && ++paints_ >= kCleanAfterPaints) clean_ = true;
  if (clean_) paints_ = 0;
  renderer.displayBuffer(clean_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  clean_ = false;
}

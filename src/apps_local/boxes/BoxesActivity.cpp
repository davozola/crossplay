#include "BoxesActivity.h"

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

enum { OptOpponent, OptBoard, OptFirst };
struct Option {
  const char* name;
  const char* const* values;
  uint8_t count;
};
constexpr const char* kOpponents[] = {"DEVICE, EASY", "DEVICE, MEDIUM", "DEVICE, HARD", "TWO PLAYERS"};
constexpr const char* kBoards[] = {"4 BY 4 SQUARES", "5 BY 5", "6 BY 6", "3 BY 3"};
constexpr int kBoardSides[] = {4, 5, 6, 3};
constexpr const char* kFirst[] = {"YOU GO FIRST", "THE OTHER SIDE GOES FIRST"};
constexpr Option kOptionTable[] = {
    {"OPPONENT", kOpponents, 4},
    {"BOARD", kBoards, 4},
    {"WHO STARTS", kFirst, 2},
};
constexpr char kOptionsPath[] = "/.crosspoint/boxes.cfg";
constexpr char kGamePath[] = "/.crosspoint/boxes-game.sav";
constexpr char kStatsPath[] = "/.crosspoint/boxes-levels.cfg";
constexpr int kTwoPlayers = 3;
constexpr uint8_t kCleanAfterPaints = 16;
// Long enough to see the line the device drew before it draws another.
constexpr uint32_t kDeviceBeatMs = 500;

void disc(const GfxRenderer& r, const int cx, const int cy, const int radius) {
  for (int y = -radius; y <= radius; ++y) {
    const int half = static_cast<int>(std::lround(std::sqrt(static_cast<float>(radius * radius - y * y))));
    r.fillRect(cx - half, cy + y, half * 2 + 1, 1, true);
  }
}

}  // namespace

std::unique_ptr<Activity> BoxesActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<BoxesActivity>(renderer, mappedInput);
}

void BoxesActivity::loadOptions() {
  uint8_t bytes[kOptions] = {};
  int got = 0;
  HalFile file;
  if (Storage.exists(kOptionsPath) && Storage.openFileForRead("BOX", kOptionsPath, file)) {
    got = file.read(bytes, sizeof(bytes));
  }
  for (int i = 0; i < kOptions; ++i) options_[i] = i < got && bytes[i] < kOptionTable[i].count ? bytes[i] : 0;
}

void BoxesActivity::saveOptions() {
  HalFile file;
  if (!Storage.openFileForWrite("BOX", kOptionsPath, file)) return;
  file.write(options_, sizeof(options_));
  file.flush();
}

void BoxesActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  rng_.state = static_cast<uint32_t>(millis()) * 2654435761u + 1u;
  loadOptions();
  sologame::loadStats("BOX", kStatsPath, stats_);
  hasSaved_ = Storage.exists(kGamePath);
  view_ = View::Setup;
  requestUpdate();
}

int BoxesActivity::pageBottom() const {
  return renderer.getScreenHeight() - (mappedInput.hasTouch() ? 8 : lab::kBottomReserve + 6);
}

void BoxesActivity::newGame() {
  const int side = kBoardSides[options_[OptBoard] % 4];
  boxes::start(board_, side, side);
  // Player one is always "you" (or the first of two); the other side may start.
  board_.turn = options_[OptFirst] == 0 ? 1 : 2;
  note_ = "";
  clean_ = true;
  history_.clear();
  hint_ = -1;
  recorded_ = false;
  sologame::drop(kGamePath);
  hasSaved_ = false;
}

void BoxesActivity::saveGame() {
  if (board_.over()) return;
  hasSaved_ = sologame::save("BOX", kGamePath, board_, history_, options_[OptOpponent], options_[OptBoard]);
}

void BoxesActivity::continueGame() {
  uint8_t opponent = options_[OptOpponent];
  uint8_t boardChoice = options_[OptBoard];
  if (!sologame::load("BOX", kGamePath, board_, history_, opponent, boardChoice) || board_.w < 3 || board_.w > 6 ||
      board_.h != board_.w || board_.over()) {
    newGame();
    return;
  }
  // The game goes on against who it was begun against.
  if (opponent < kOptionTable[OptOpponent].count) options_[OptOpponent] = opponent;
  if (boardChoice < kOptionTable[OptBoard].count) options_[OptBoard] = boardChoice;
  note_ = "";
  hint_ = -1;
  recorded_ = false;
  clean_ = true;
}

// Back to before the player's last line, the device's answer undone with it.
void BoxesActivity::takeBack() {
  if (deviceToMove() || !history_.pop(board_)) return;
  note_ = "";
  hint_ = -1;
  recorded_ = false;
  clean_ = true;
  saveGame();
}

// The line the hard level would draw for the side to move.
void BoxesActivity::showHint() {
  if (deviceToMove() || board_.over()) return;
  hint_ = boxes::choose(board_, boxes::kLevels - 1, rng_);
}

// A finished game against the device is counted against the level it was
// played at, once, and is no longer one to continue.
void BoxesActivity::recordIfOver() {
  if (recorded_ || !board_.over()) return;
  recorded_ = true;
  sologame::drop(kGamePath);
  hasSaved_ = false;
  history_.clear();
  if (options_[OptOpponent] == kTwoPlayers) return;
  const int me = board_.score[1];
  const int them = board_.score[2];
  stats_.count(options_[OptOpponent], me > them ? 0 : (me < them ? 1 : 2));
  sologame::saveStats("BOX", kStatsPath, stats_);
}

bool BoxesActivity::deviceToMove() const { return options_[OptOpponent] != 3 && !board_.over() && board_.turn == 2; }

void BoxesActivity::deviceMoves() {
  const int line = boxes::choose(board_, options_[OptOpponent], rng_);
  if (line >= 0) boxes::draw(board_, line);
  note_ = "";
  // Kept after every answer, so leaving at any point loses nothing.
  saveGame();
  recordIfOver();
}

void BoxesActivity::boardRect(int& x, int& y, int& cell) const {
  const int width = renderer.getScreenWidth();
  const int room = width - 2 * toybox::kMargin - 24;
  cell = room / board_.w;
  x = (width - cell * board_.w) / 2;
  y = 112;
}

// The undrawn line nearest the finger, when the finger is near one.
int BoxesActivity::lineAt(const int tapX, const int tapY) const {
  int bx = 0;
  int by = 0;
  int cell = 0;
  boardRect(bx, by, cell);
  int best = -1;
  long bestD = static_cast<long>(cell) * cell * 3 / 10;  // a little over half a cell away, squared
  for (int line = 0; line < board_.lines(); ++line) {
    int mx = 0;
    int my = 0;
    if (line < board_.across()) {
      mx = bx + (line % board_.w) * cell + cell / 2;
      my = by + (line / board_.w) * cell;
    } else {
      const int k = line - board_.across();
      mx = bx + (k % (board_.w + 1)) * cell;
      my = by + (k / (board_.w + 1)) * cell + cell / 2;
    }
    const long d = static_cast<long>(tapX - mx) * (tapX - mx) + static_cast<long>(tapY - my) * (tapY - my);
    if (d < bestD) {
      bestD = d;
      best = line;
    }
  }
  return best;
}

void BoxesActivity::loop() {
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

  if (view_ == View::Play && deviceToMove()) {
    delay(kDeviceBeatMs);
    deviceMoves();
    requestUpdateAndWait();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;

  if (view_ == View::Play && !board_.over()) {
    const int line = lineAt(tapX, tapY);
    if (line >= 0) {
      const boxes::Board before = board_;
      const int closed = boxes::draw(board_, line);
      if (closed < 0) {
        haptic_feedback::touchAction();
        return;
      }
      history_.push(before);
      hint_ = -1;
      saveGame();
      recordIfOver();
      note_ = closed > 0 && !board_.over() ? "A SQUARE! GO AGAIN" : "";
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

void BoxesActivity::buildSetup(toybox::Screen& screen) {
  lab::chrome(screen, "DOTS AND BOXES");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  int y = toybox::kChromeHeight + 14;
  const int rowH = 78;
  for (int i = 0; i < kOptions; ++i) {
    lab::button(screen, lab::rect(x, y, inner, rowH), "", static_cast<fui::ActionId>(ActionOption + i), 0, false,
                false);
    lab::caption(screen, x + 18, y + 12, inner - 36, kOptionTable[i].name);
    lab::value(screen, x + 18, y + 12 + toybox::kTileCut.inkHeight + 10, inner - 36,
               kOptionTable[i].values[options_[i]]);
    y += rowH + 10;
  }
  lab::paragraph(
      screen, x, y + 8, inner,
      "TAKE TURNS DRAWING ONE LINE BETWEEN TWO DOTS. DRAW THE FOURTH SIDE OF A SQUARE AND IT IS YOURS, AND YOU "
      "GO AGAIN. MOST SQUARES WINS.",
      5);
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

void BoxesActivity::buildPlay(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - 2 * toybox::kMargin;
  const bool over = board_.over();
  const bool two = options_[OptOpponent] == 3;

  // The score: each side's mark and count, the side to move underlined.
  const int half = inner / 2;
  for (int p = 1; p <= 2; ++p) {
    const int sx = x + (p - 1) * half;
    renderer.fillRect(sx, 22, 44, 44, true);
    if (p == 2) {
      renderer.fillRect(sx + 4, 26, 36, 36, false);
      renderer.fillRectDither(sx + 4, 26, 36, 36, Color::LightGray);
    }
    char count[toybox::kIntChars + 1];
    std::snprintf(count, sizeof(count), "%d", board_.score[p] % 1000);
    lab::label(screen, lab::rect(sx + 58, 44 - toybox::kDisplayCut.inkHeight / 2, 80, toybox::kDisplayCut.inkHeight),
               count, toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Left);
    lab::caption(screen, sx + 130, 38, half - 130,
                 two ? (p == 1 ? "PLAYER 1" : "PLAYER 2") : (p == 1 ? "YOU" : "DEVICE"));
    if (!over && board_.turn == p) renderer.fillRect(sx, 76, half - 12, 5, true);
  }

  int bx = 0;
  int by = 0;
  int cell = 0;
  boardRect(bx, by, cell);
  // Owned squares first, so the lines sit on top of them.
  for (int box = 0; box < board_.w * board_.h; ++box) {
    if (board_.owner[box] == 0) continue;
    const int qx = bx + (box % board_.w) * cell + 8;
    const int qy = by + (box / board_.w) * cell + 8;
    if (board_.owner[box] == 1) {
      renderer.fillRect(qx, qy, cell - 16, cell - 16, true);
    } else {
      renderer.fillRectDither(qx, qy, cell - 16, cell - 16, Color::LightGray);
    }
  }
  for (int line = 0; line < board_.lines(); ++line) {
    const bool acrossLine = line < board_.across();
    const int k = acrossLine ? line : line - board_.across();
    const int lx = bx + (acrossLine ? k % board_.w : k % (board_.w + 1)) * cell;
    const int ly = by + (acrossLine ? k / board_.w : k / (board_.w + 1)) * cell;
    if (board_.line[line]) {
      const int weight = line == board_.last ? 10 : 6;
      if (acrossLine) {
        renderer.fillRect(lx, ly - weight / 2, cell, weight, true);
      } else {
        renderer.fillRect(lx - weight / 2, ly, weight, cell, true);
      }
    } else if (line == hint_) {
      // The hinted line: drawn in grey, the weight of a real one.
      if (acrossLine) {
        renderer.fillRectDither(lx, ly - 4, cell, 8, Color::DarkGray);
      } else {
        renderer.fillRectDither(lx - 4, ly, 8, cell, Color::DarkGray);
      }
    } else {
      // Where a line could go: a row of specks.
      for (int s = cell / 6; s < cell; s += cell / 6) {
        renderer.fillRect(acrossLine ? lx + s : lx - 1, acrossLine ? ly - 1 : ly + s, 2, 2, true);
      }
    }
  }
  for (int row = 0; row <= board_.h; ++row) {
    for (int col = 0; col <= board_.w; ++col) disc(renderer, bx + col * cell, by + row * cell, 8);
  }

  const int below = by + board_.h * cell + 34;
  char line[64];
  const char* status = note_;
  if (over) {
    const int a = board_.score[1];
    const int b = board_.score[2];
    if (a == b) {
      std::snprintf(line, sizeof(line), "A DRAW, %d ALL", a % 100);
    } else {
      const char* name = two ? (a > b ? "PLAYER 1 WINS" : "PLAYER 2 WINS") : (a > b ? "YOU WIN" : "THE DEVICE WINS");
      std::snprintf(line, sizeof(line), "%s, %d TO %d", name, (a > b ? a : b) % 100, (a > b ? b : a) % 100);
    }
    status = line;
  } else if (status[0] == 0) {
    status = deviceToMove() ? "THE DEVICE IS DRAWING"
             : two          ? (board_.turn == 1 ? "PLAYER 1" : "PLAYER 2")
                            : "YOUR LINE";
  }
  lab::label(screen, lab::rect(x, below, inner, toybox::kUiCut.inkHeight), status, toybox::kUiFont, toybox::kUiCut);

  const int buttonH = 60;
  if (options_[OptOpponent] != kTwoPlayers && !over) {
    // Against the device: take a line back and ask for one, beside the two that were here.
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

void BoxesActivity::render(RenderLock&&) {
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
  toybox::reportOverflow(interactions_, "Boxes");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!clean_ && ++paints_ >= kCleanAfterPaints) clean_ = true;
  if (clean_) paints_ = 0;
  renderer.displayBuffer(clean_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  clean_ = false;
}

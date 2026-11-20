#include "MancalaActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Memory.h>

#include <cstdio>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"
// After the theme: see ToyboxHowTo.h.
#include "../ui/ToyboxHowTo.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace mc = mancala;

namespace {

enum : fui::ActionId {
  ActionOpponent = 1,
  ActionLevel,
  ActionSeeds,
  ActionFirst,
  ActionNew,
  ActionContinue,
  ActionPit,  // value: the slot
  ActionAgain,
  ActionMenu,
  ActionUndo,
  ActionHint,
};

constexpr char kSavePath[] = "/.crosspoint/mancala.sav";
constexpr char kStatsPath[] = "/.crosspoint/mancala-levels.cfg";
constexpr char kViewPath[] = "/.crosspoint/mancala-view.cfg";
constexpr uint32_t kMachineThinksMs = 900;  // long enough to see what you did first
constexpr uint8_t kCleanAfterPaints = 10;
constexpr int kBottomReserve = 40;
constexpr int kRadius = 18;

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

constexpr const char* kHowToPages[] = {
    "KALAH IS PLAYED WITH TWO ROWS OF SIX PITS AND TWO STORES. HERE THE BOARD STANDS UPRIGHT: YOUR PITS ARE THE LEFT "
    "COLUMN AND YOUR STORE IS THE LONG ONE AT THE FOOT. THE RIGHT COLUMN AND THE STORE AT THE TOP ARE THE OTHER "
    "PLAYER'S. WHOEVER ENDS WITH MORE SEEDS IN THEIR STORE WINS.",
    "ON YOUR TURN, TAP ONE OF YOUR PITS. ALL ITS SEEDS ARE PICKED UP AND SOWN ONE TO A PIT: DOWN YOUR COLUMN, ONE "
    "INTO YOUR STORE, THEN UP THE OTHER COLUMN, AND ROUND AGAIN IF THERE ARE ENOUGH. THE OTHER PLAYER'S STORE IS "
    "ALWAYS SKIPPED.",
    "IF YOUR LAST SEED FALLS IN YOUR OWN STORE, YOU MOVE AGAIN. IF IT FALLS IN AN EMPTY PIT OF YOURS AND THE PIT "
    "BESIDE IT HAS SEEDS, YOU CAPTURE: THAT SEED AND EVERYTHING ACROSS FROM IT GO TO YOUR STORE.",
    "THE GAME ENDS WHEN ONE COLUMN IS EMPTY. WHAT IS LEFT IN THE OTHER GOES TO ITS OWNER'S STORE, SO EMPTYING YOUR "
    "SIDE FIRST IS NOT ALWAYS GOOD. ON THE BOARD, THE GREY PIT IS THE ONE JUST PLAYED AND THE DOUBLE RING IS WHERE "
    "ITS LAST SEED FELL. THICK RINGS ARE THE PITS YOU CAN PLAY. SWIPE UP OR DOWN TO LAY THE BOARD ON ITS SIDE.",
};
constexpr toyboxhowto::Guide kHowTo{kHowToPages, 4};

// The same rules for the board on its side, in pages short enough for it.
constexpr const char* kHowToWidePages[] = {
    "KALAH: TWO ROWS OF SIX PITS AND TWO STORES. YOUR PITS ARE THE ROW ALONG THE FOOT AND YOUR STORE IS THE TALL ONE "
    "AT THE RIGHT. WHOEVER ENDS WITH MORE SEEDS IN THEIR STORE WINS.",
    "TAP ONE OF YOUR PITS. ITS SEEDS ARE SOWN ONE TO A PIT: RIGHTWARD ALONG YOUR ROW, ONE INTO YOUR STORE, THEN "
    "LEFTWARD ALONG THE TOP ROW. THE OTHER STORE IS ALWAYS SKIPPED.",
    "IF YOUR LAST SEED FALLS IN YOUR OWN STORE, YOU MOVE AGAIN. IF IT FALLS IN AN EMPTY PIT OF YOURS AND THE PIT "
    "ABOVE HAS SEEDS, YOU CAPTURE BOTH INTO YOUR STORE.",
    "THE GAME ENDS WHEN ONE ROW IS EMPTY. WHAT IS LEFT IN THE OTHER GOES TO ITS OWNER'S STORE, SO EMPTYING YOUR SIDE "
    "FIRST IS NOT ALWAYS GOOD.",
    "THICK RINGS ARE THE PITS YOU CAN PLAY. THE GREY PIT IS THE ONE JUST PLAYED; THE DOUBLE RING IS WHERE ITS LAST "
    "SEED FELL. SWIPE UP OR DOWN TO STAND THE BOARD UPRIGHT.",
};
constexpr toyboxhowto::Guide kHowToWide{kHowToWidePages, 5};

// One choice on the menu: its name on the left, what it is set to on the right.
void choice(toybox::Screen& screen, const fui::Rect& r, const char* name, const char* value, const fui::ActionId action,
            const bool enabled = true) {
  fui::ButtonProps b;
  b.label = "";
  b.action = action;
  b.enabled = enabled;
  b.styles = toybox::rowStyles();
  b.styles.normal.borderWidth = 0;
  b.styles.disabled = b.styles.normal;
  b.minTouchSize = 0;
  screen.button(b, r);
  screen.target().stroke(r, enabled ? kInk : fui::Paint::dither(fui::Color::DarkGray), 3, kRadius);
  lab::value(screen, r.x + 20, r.y + (r.height - toybox::kUiCut.inkHeight) / 2, 200, name, fui::TextAlign::Left,
             fui::Color::DarkGray);
  lab::label(screen, lab::rect(r.x + 150, r.y, r.width - 172, r.height), value, toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Right, enabled ? fui::Color::Black : fui::Color::DarkGray);
}

// A seed: a stone with a glint on it.
void seed(toybox::Screen& screen, const int cx, const int cy, const int r) {
  toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r), fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(cx - r / 3), static_cast<int16_t>(cy - r / 3),
               static_cast<int16_t>(r / 4 + 1), fui::Color::White);
}

// Up to `cols` x `rows` seeds laid in a block centred on (cx, cy); a pit
// holding more shows a full block, and its number says the rest.
void seeds(toybox::Screen& screen, const int count, const int cx, const int cy, const int cols, const int rows,
           const int r) {
  const int shown = count > cols * rows ? cols * rows : count;
  if (shown <= 0) return;
  const int step = r * 2 + 4;
  const int usedRows = (shown + cols - 1) / cols;
  for (int i = 0; i < shown; ++i) {
    const int row = i / cols;
    const int inRow = row == usedRows - 1 ? shown - row * cols : cols;
    const int x = cx - (inRow - 1) * step / 2 + (i % cols) * step;
    const int y = cy - (usedRows - 1) * step / 2 + row * step;
    seed(screen, x, y, r);
  }
}

}  // namespace

std::unique_ptr<Activity> MancalaActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MancalaActivity>(renderer, mappedInput);
}

void MancalaActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  rng_ = static_cast<uint32_t>(millis()) * 2654435761u + 1u;
  load();
  sologame::loadStats("MANC", kStatsPath, stats_);
  view_ = View::Menu;
  cleanNext_ = true;
  loadView();
  requestUpdate();
}

void MancalaActivity::onExit() {
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  Activity::onExit();
}

// Which way up the board was left: one byte, 1 on its side.
void MancalaActivity::loadView() {
  wide_ = true;
  HalFile file;
  uint8_t byte = 1;
  if (Storage.exists(kViewPath) && Storage.openFileForRead("MANC", kViewPath, file) && file.read(&byte, 1) == 1) {
    wide_ = byte != 0;
  }
}

void MancalaActivity::saveView() {
  HalFile file;
  if (!Storage.openFileForWrite("MANC", kViewPath, file)) return;
  const uint8_t byte = wide_ ? 1 : 0;
  file.write(&byte, 1);
  file.flush();
}

void MancalaActivity::load() {
  config_ = mc::Config{};
  mc::setup(board_, mc::seedsOf(config_), mc::South);
  inProgress_ = false;
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  uint8_t bytes[mc::kSaveBytes];
  if (!Storage.openFileForRead("MANC", kSavePath, file)) return;
  const int got = file.read(bytes, sizeof(bytes));
  mc::unpack(bytes, got, config_, board_, inProgress_);
}

void MancalaActivity::save() {
  HalFile file;
  if (!Storage.openFileForWrite("MANC", kSavePath, file)) return;
  uint8_t bytes[mc::kSaveBytes];
  mc::pack(config_, board_, inProgress_, bytes);
  file.write(bytes, sizeof(bytes));
  file.flush();
}

bool MancalaActivity::machineToMove() const {
  return !config_.friendly && board_.turn == mc::North && !mc::over(board_);
}

void MancalaActivity::newGame() {
  mc::setup(board_, mc::seedsOf(config_), config_.first ? mc::North : mc::South);
  inProgress_ = true;
  played_ = -1;
  last_ = -1;
  captured_ = 0;
  movedAgain_ = false;
  machineAtMs_ = 0;
  history_.clear();
  hint_ = -1;
  recorded_ = false;
  save();
}

// Back to before the player's last move, the machine's answer undone with it.
void MancalaActivity::takeBack() {
  if (config_.friendly || machineToMove() || !history_.pop(board_)) return;
  played_ = -1;
  last_ = -1;
  captured_ = 0;
  movedAgain_ = false;
  hint_ = -1;
  inProgress_ = true;
  recorded_ = false;
  cleanNext_ = true;
  save();
}

// The pit the hard level would play for the side to move.
void MancalaActivity::showHint() {
  if (machineToMove() || mc::over(board_)) return;
  mc::Config hard = config_;
  hard.level = mc::kLevels - 1;
  hint_ = static_cast<int8_t>(mc::bestMove(board_, mc::depthOf(hard), rng_));
}

// A finished game against the machine is counted against its level, once.
void MancalaActivity::recordIfOver() {
  if (recorded_ || !mc::over(board_)) return;
  recorded_ = true;
  history_.clear();
  if (config_.friendly) return;
  const int me = board_.slot[mc::kSouthStore];
  const int them = board_.slot[mc::kNorthStore];
  stats_.count(config_.level, me > them ? 0 : (me < them ? 1 : 2));
  sologame::saveStats("MANC", kStatsPath, stats_);
}

void MancalaActivity::move(const int slot) {
  if (!mc::legal(board_, slot)) return;
  // The player's own moves are what can be taken back.
  if (!config_.friendly && board_.turn == mc::South) history_.push(board_);
  hint_ = -1;
  const mc::Outcome out = mc::play(board_, slot);
  played_ = static_cast<int8_t>(slot);
  last_ = out.last;
  captured_ = out.captured;
  movedAgain_ = out.again;
  inProgress_ = !out.ended;
  if (out.ended) cleanNext_ = true;
  save();
  recordIfOver();
}

void MancalaActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (howTo_.open) {
      howTo_.back();
    } else if (view_ == View::Play) {
      view_ = View::Menu;
      machineAtMs_ = 0;
      cleanNext_ = true;
    } else {
      shelf::leave(renderer, mappedInput);
      return;
    }
    requestUpdate();
    return;
  }

  // A swipe up or down stands the board upright, and lays it down again.
  if (!howTo_.open) {
    const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
      wide_ = !wide_;
      saveView();
      cleanNext_ = true;
      requestUpdate();
      return;
    }
  }

  // The machine, after a pause in which the last move can be seen.
  if (view_ == View::Play && !howTo_.open && machineToMove()) {
    const uint32_t now = static_cast<uint32_t>(millis());
    if (machineAtMs_ == 0) {
      machineAtMs_ = now + kMachineThinksMs;
      if (machineAtMs_ == 0) machineAtMs_ = 1;
    } else if (now - machineAtMs_ < 0x80000000u) {
      machineAtMs_ = 0;
      move(mc::bestMove(board_, mc::depthOf(config_), rng_));
      requestUpdate();
    }
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  route(interactions_.route(input));
}

void MancalaActivity::route(const fui::ActionEvent& event) {
  if (howTo_.open) {
    if (howTo_.tap(event, wide_ ? kHowToWide : kHowTo)) requestUpdate();
    return;
  }
  switch (event.action) {
    case toyboxhowto::kOpen:
      howTo_.start();
      break;
    case ActionOpponent:
      config_.friendly = config_.friendly ? 0 : 1;
      inProgress_ = false;
      save();
      break;
    case ActionLevel:
      config_.level = static_cast<uint8_t>((config_.level + 1) % mc::kLevels);
      save();
      break;
    case ActionSeeds:
      config_.seeds = static_cast<uint8_t>((config_.seeds + 1) % mc::kSeedChoices);
      inProgress_ = false;
      save();
      break;
    case ActionFirst:
      config_.first = config_.first ? 0 : 1;
      save();
      break;
    case ActionNew:
    case ActionAgain:
      newGame();
      view_ = View::Play;
      cleanNext_ = true;
      break;
    case ActionContinue:
      if (!inProgress_) return;
      view_ = View::Play;
      cleanNext_ = true;
      break;
    case ActionMenu:
      view_ = View::Menu;
      cleanNext_ = true;
      break;
    case ActionUndo:
      if (view_ != View::Play) return;
      takeBack();
      break;
    case ActionHint:
      if (view_ != View::Play) return;
      showHint();
      break;
    case ActionPit:
      if (view_ != View::Play || machineToMove() || !mc::legal(board_, event.value)) return;
      haptic_feedback::touchAction(true);
      move(event.value);
      break;
    default:
      return;
  }
  requestUpdate();
}

void MancalaActivity::buildMenu(toybox::Screen& screen) {
  lab::chrome(screen, "MANCALA");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = screen.device().height - kBottomReserve - 6;

  // A strip of the board for a title: six pits with the opening seeds.
  int y = toybox::kChromeHeight + 14;
  {
    const int pitW = (inner - 5 * 8) / 6;
    for (int i = 0; i < 6; ++i) {
      const fui::Rect r = lab::rect(x + i * (pitW + 8), y, pitW, 70);
      screen.target().stroke(r, kInk, 3, kRadius);
      seeds(screen, mc::seedsOf(config_), r.x + pitW / 2, y + 35, 2, 3, 8);
    }
  }
  y += 70 + 16;

  const int rowH = 58;
  const int gap = 8;
  choice(screen, lab::rect(x, y, inner, rowH), "AGAINST", config_.friendly ? "A FRIEND" : "MACHINE", ActionOpponent);
  y += rowH + gap;
  choice(screen, lab::rect(x, y, inner, rowH), "LEVEL", mc::levelName(config_), ActionLevel, !config_.friendly);
  y += rowH + gap;
  char seedsText[toybox::kIntChars + 8];
  std::snprintf(seedsText, sizeof(seedsText), "%d A PIT", mc::seedsOf(config_));
  choice(screen, lab::rect(x, y, inner, rowH), "SEEDS", seedsText, ActionSeeds);
  y += rowH + gap;
  choice(screen, lab::rect(x, y, inner, rowH), "FIRST",
         config_.first ? (config_.friendly ? "TOP" : "MACHINE") : (config_.friendly ? "BOTTOM" : "YOU"), ActionFirst);
  y += rowH + gap + 6;
  // How it stands against the machine at that level.
  if (!config_.friendly) {
    char record[64];
    std::snprintf(record, sizeof(record), "AGAINST %s: %d WON, %d LOST, %d DRAWN", mc::levelName(config_),
                  stats_.won[config_.level % sologame::kStatLevels] % 10000,
                  stats_.lost[config_.level % sologame::kStatLevels] % 10000,
                  stats_.drawn[config_.level % sologame::kStatLevels] % 10000);
    lab::caption(screen, x + 4, y, inner - 8, record);
    y += toybox::kTileCut.inkHeight + 10;
  }

  // New game, large; continue beside it when there is one to continue.
  const int howH = 48;
  const int playH = bottom - howH - 10 - y;
  if (inProgress_) {
    const int half = (inner - gap) / 2;
    lab::button(screen, lab::rect(x, y, half, playH), "CONTINUE", ActionContinue, 0, true, false);
    lab::button(screen, lab::rect(x + half + gap, y, half, playH), "NEW GAME", ActionNew, 0, false, false);
  } else {
    lab::button(screen, lab::rect(x, y, inner, playH), "", ActionNew, 0, true, false);
    // The one big word on the page, in a cut the frame has no slot for.
    const int w = renderer.getTextWidth(toybox::kLargeFontId, "PLAY");
    renderer.drawText(
        toybox::kLargeFontId, x + (inner - w) / 2,
        y + (playH - toybox::kLargeCut.inkHeight) / 2 - (toybox::kLargeCut.ascender - toybox::kLargeCut.inkHeight),
        "PLAY", false);
  }
  toyboxhowto::button(screen, lab::rect(x, bottom - howH, inner, howH));
}

void MancalaActivity::buildBoard(toybox::Screen& screen) {
  const bool finished = mc::over(board_);

  const char* status = statusText();
  fui::HeaderProps header;
  header.title = "MANCALA";
  header.rightLabel = status;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  header.sidePadding = screen.theme().headerSidePadding;
  toybox::headerBand(screen, header);

  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int top = toybox::kChromeHeight + 8;
  // Against the machine a row under the board takes a move back or asks for one.
  const bool helpRow = !config_.friendly && !finished;
  const int helpH = 46;
  const int pageBottom = screen.device().height - kBottomReserve - 4;
  const int bottom = pageBottom - (helpRow ? helpH + 8 : 0);
  if (helpRow) {
    const int half = (screen.device().width - toybox::kMargin * 2 - 8) / 2;
    fui::ButtonProps side;
    side.styles = toybox::rowStyles();
    side.text = toybox::buttonText(screen.theme());
    side.radius = kRadius;
    side.label = "UNDO";
    side.action = ActionUndo;
    side.enabled = history_.count > 0 && !machineToMove();
    screen.button(side, lab::rect(toybox::kMargin, pageBottom - helpH, half, helpH));
    side.label = "HINT";
    side.action = ActionHint;
    side.enabled = !machineToMove();
    screen.button(side, lab::rect(toybox::kMargin + half + 8, pageBottom - helpH, half, helpH));
  }
  const int storeH = 80;
  const int gap = 8;
  const int pitsTop = top + storeH + gap;
  const int pitsBottom = bottom - storeH - gap;
  const int pitH = (pitsBottom - pitsTop - gap * (mc::kPits - 1)) / mc::kPits;
  const int pitW = (inner - gap) / 2;
  char number[toybox::kIntChars + 1];

  // The stores: a row of seeds, and the count large at the end.
  auto store = [&](const int slot, const int y, const char* name) {
    const fui::Rect r = lab::rect(x, y, inner, storeH);
    const bool marked = slot == last_;
    screen.target().stroke(r, kInk, 4, static_cast<uint8_t>(storeH / 2));
    if (marked) {
      screen.target().stroke(lab::rect(r.x + 8, r.y + 8, r.width - 16, r.height - 16), kInk, 2,
                             static_cast<uint8_t>(storeH / 2 - 8));
    }
    std::snprintf(number, sizeof(number), "%d", board_.slot[slot]);
    lab::label(screen, lab::rect(r.x + r.width - 110, r.y, 80, storeH), number, toybox::kDisplayFont,
               toybox::kDisplayCut, fui::TextAlign::Right);
    lab::caption(screen, r.x + 34, r.y + 14, 160, name, fui::TextAlign::Left, fui::Color::DarkGray);
    seeds(screen, board_.slot[slot], r.x + 34 + 12 * 9, r.y + storeH / 2 + 12, 12, 2, 7);
  };
  store(mc::kNorthStore, top, config_.friendly ? "TOP" : "MACHINE");
  store(mc::kSouthStore, bottom - storeH, config_.friendly ? "BOTTOM" : "YOU");

  // The pits: South's down the left, North's up the right, so that the seeds
  // go round the board anticlockwise as they do on a table.
  for (int row = 0; row < mc::kPits; ++row) {
    for (int col = 0; col < 2; ++col) {
      const int slot = col == 0 ? row : 12 - row;
      const fui::Rect r = lab::rect(x + col * (pitW + gap), pitsTop + row * (pitH + gap), pitW, pitH);
      const bool playable = mc::legal(board_, slot) && !machineToMove();
      fui::ButtonProps b;
      b.label = "";
      b.action = ActionPit;
      b.value = static_cast<int16_t>(slot);
      b.enabled = playable;
      b.styles = toybox::rowStyles();
      b.styles.normal.borderWidth = 0;
      b.styles.disabled = b.styles.normal;
      b.minTouchSize = 0;
      screen.button(b, r);
      const uint8_t radius = static_cast<uint8_t>(pitH / 2);
      if (slot == played_)
        lab::box(screen, r.x, r.y, r.width, r.height, fui::Paint::dither(fui::Color::LightGray), radius);
      screen.target().stroke(r, kInk, playable ? 5 : 2, radius);
      if (slot == last_) {
        screen.target().stroke(lab::rect(r.x + 8, r.y + 8, r.width - 16, r.height - 16), kInk, 2,
                               static_cast<uint8_t>(radius - 8));
      }
      // The hinted pit: a second, heavy frame inside its own.
      if (slot == hint_) {
        screen.target().stroke(lab::rect(r.x + 10, r.y + 10, r.width - 20, r.height - 20), kInk, 5,
                               static_cast<uint8_t>(radius > 10 ? radius - 10 : 0));
      }
      // The count on the outer end, the seeds toward the middle of the board.
      std::snprintf(number, sizeof(number), "%d", board_.slot[slot]);
      const int numberW = 56;
      const int nx = col == 0 ? r.x + 14 : r.x + r.width - 14 - numberW;
      lab::label(screen, lab::rect(nx, r.y, numberW, pitH), number, toybox::kUiFont, toybox::kUiCut,
                 fui::TextAlign::Center, board_.slot[slot] ? fui::Color::Black : fui::Color::DarkGray);
      const int cx =
          col == 0 ? r.x + numberW + 14 + (r.width - numberW - 30) / 2 : r.x + 16 + (r.width - numberW - 30) / 2;
      seeds(screen, board_.slot[slot], cx, r.y + pitH / 2, 6, 2, 8);
    }
  }

  if (finished) buildVerdict(screen, status, top, bottom);
}

// Whose move it is, or how it ended.
const char* MancalaActivity::statusText() const {
  const int south = board_.slot[mc::kSouthStore];
  const int north = board_.slot[mc::kNorthStore];
  if (mc::over(board_)) {
    if (south == north) return "A DRAW";
    if (config_.friendly) return south > north ? "BOTTOM WINS" : "TOP WINS";
    return south > north ? "YOU WIN" : "MACHINE WINS";
  }
  if (config_.friendly) return board_.turn == mc::South ? "BOTTOM TO PLAY" : "TOP TO PLAY";
  if (board_.turn == mc::North) return "THINKING";
  return movedAgain_ && played_ >= 0 && mc::sideOf(played_) == mc::South ? "GO AGAIN" : "YOUR MOVE";
}

// A game that is over: the verdict over the board, and what next.
void MancalaActivity::buildVerdict(toybox::Screen& screen, const char* status, const int top, const int bottom) {
  const int south = board_.slot[mc::kSouthStore];
  const int north = board_.slot[mc::kNorthStore];
  const int width = screen.device().width;
  const int boxW = (width > 520 ? 480 : width - toybox::kMargin * 2) - 40;
  const int boxH = 250;
  const int bx = (width - boxW) / 2;
  const int by = (top + bottom) / 2 - boxH / 2;
  lab::box(screen, bx - 6, by - 6, boxW + 12, boxH + 12, kPaper, kRadius + 6);
  lab::box(screen, bx, by, boxW, boxH, kInk, kRadius);
  lab::box(screen, bx + 4, by + 4, boxW - 8, boxH - 8, kPaper, kRadius - 4);
  lab::label(screen, lab::rect(bx, by + 28, boxW, toybox::kDisplayCut.inkHeight), status, toybox::kDisplayFont,
             toybox::kDisplayCut);
  char score[2 * toybox::kIntChars + 8];
  std::snprintf(score, sizeof(score), "%d TO %d", south > north ? south : north, south > north ? north : south);
  lab::value(screen, bx, by + 28 + toybox::kDisplayCut.inkHeight + 18, boxW, score, fui::TextAlign::Center);
  const int half = (boxW - 48 - 10) / 2;
  lab::button(screen, lab::rect(bx + 24, by + boxH - 24 - 64, half, 64), "MENU", ActionMenu, 0);
  lab::button(screen, lab::rect(bx + 24 + half + 10, by + boxH - 24 - 64, half, 64), "AGAIN", ActionAgain, 0, true);
}

// The menu on its side: the choices down the left, the way in at the right.
void MancalaActivity::buildMenuWide(toybox::Screen& screen) {
  lab::chrome(screen, "MANCALA");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int gap = 8;
  const int top = toybox::kChromeHeight + 12;
  const int bottom = screen.device().height - kBottomReserve - 6;
  const int leftW = 440;
  const int rightX = x + leftW + 16;
  const int rightW = width - x - rightX;

  const int rowH = 62;
  int y = top;
  choice(screen, lab::rect(x, y, leftW, rowH), "AGAINST", config_.friendly ? "A FRIEND" : "MACHINE", ActionOpponent);
  y += rowH + gap;
  choice(screen, lab::rect(x, y, leftW, rowH), "LEVEL", mc::levelName(config_), ActionLevel, !config_.friendly);
  y += rowH + gap;
  char seedsText[toybox::kIntChars + 8];
  std::snprintf(seedsText, sizeof(seedsText), "%d A PIT", mc::seedsOf(config_));
  choice(screen, lab::rect(x, y, leftW, rowH), "SEEDS", seedsText, ActionSeeds);
  y += rowH + gap;
  choice(screen, lab::rect(x, y, leftW, rowH), "FIRST",
         config_.first ? (config_.friendly ? "TOP" : "MACHINE") : (config_.friendly ? "BOTTOM" : "YOU"), ActionFirst);
  y += rowH + gap + 8;
  if (!config_.friendly) {
    char record[64];
    std::snprintf(record, sizeof(record), "AGAINST %s: %d WON, %d LOST, %d DRAWN", mc::levelName(config_),
                  stats_.won[config_.level % sologame::kStatLevels] % 10000,
                  stats_.lost[config_.level % sologame::kStatLevels] % 10000,
                  stats_.drawn[config_.level % sologame::kStatLevels] % 10000);
    lab::caption(screen, x + 4, y, leftW - 8, record);
  }

  const int howH = 52;
  const int playH = bottom - howH - gap - top;
  if (inProgress_) {
    const int half = (playH - gap) / 2;
    lab::button(screen, lab::rect(rightX, top, rightW, half), "CONTINUE", ActionContinue, 0, true, false);
    lab::button(screen, lab::rect(rightX, top + half + gap, rightW, half), "NEW GAME", ActionNew, 0, false, false);
  } else {
    lab::button(screen, lab::rect(rightX, top, rightW, playH), "", ActionNew, 0, true, false);
    const int w = renderer.getTextWidth(toybox::kLargeFontId, "PLAY");
    renderer.drawText(
        toybox::kLargeFontId, rightX + (rightW - w) / 2,
        top + (playH - toybox::kLargeCut.inkHeight) / 2 - (toybox::kLargeCut.ascender - toybox::kLargeCut.inkHeight),
        "PLAY", false);
  }
  toyboxhowto::button(screen, lab::rect(rightX, bottom - howH, rightW, howH));
}

// The board on its side, as it lies on a table: your row along the foot, your
// store at the right-hand end, and the seeds going round anticlockwise. No
// band: the board takes the page, and whose move it is sits between the rows.
void MancalaActivity::buildBoardWide(toybox::Screen& screen) {
  const bool finished = mc::over(board_);
  const char* status = statusText();
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int gap = 8;
  const bool help = !config_.friendly && !finished;
  const int top = 12;
  const int bottom = screen.device().height - kBottomReserve - 4;
  const int storeW = 104;
  const int helpH = 46;
  const int storeH = bottom - top - (help ? helpH + gap : 0);
  const int pitsX = x + storeW + gap;
  const int pitsW = width - pitsX * 2;
  const int pitW = (pitsW - gap * 5) / mc::kPits;
  const int statusH = toybox::kUiCut.inkHeight + 16;
  const int pitH = (bottom - top - statusH) / 2;
  const int radius = pitW / 2 < 40 ? pitW / 2 : 40;
  const int numH = 40;
  char number[toybox::kIntChars + 1];

  auto count = [&](const fui::Rect& r, const int slot, const fui::FontId font, const toybox::CutMetrics& cut) {
    std::snprintf(number, sizeof(number), "%d", board_.slot[slot]);
    lab::label(screen, r, number, font, cut, fui::TextAlign::Center,
               board_.slot[slot] ? fui::Color::Black : fui::Color::DarkGray);
  };
  // A store standing at one end: whose it is, the count, and the seeds heaped under it.
  auto store = [&](const fui::Rect& r, const int slot, const char* name) {
    screen.target().stroke(r, kInk, 4, static_cast<uint8_t>(radius));
    if (slot == last_) {
      screen.target().stroke(lab::rect(r.x + 8, r.y + 8, r.width - 16, r.height - 16), kInk, 2,
                             static_cast<uint8_t>(radius - 8));
    }
    lab::caption(screen, r.x, r.y + 20, r.width, name, fui::TextAlign::Center, fui::Color::DarkGray);
    count(lab::rect(r.x, r.y + 44, r.width, toybox::kDisplayCut.inkHeight + 8), slot, toybox::kDisplayFont,
          toybox::kDisplayCut);
    const int heapTop = r.y + 44 + toybox::kDisplayCut.inkHeight + 24;
    const int rows = (r.y + r.height - 22 - heapTop) / 20;
    seeds(screen, board_.slot[slot], r.x + r.width / 2, heapTop + rows * 10, 4, rows, 8);
  };
  store(lab::rect(x, top, storeW, storeH), mc::kNorthStore, config_.friendly ? "TOP" : "MACHINE");
  store(lab::rect(width - x - storeW, top, storeW, storeH), mc::kSouthStore, config_.friendly ? "BOTTOM" : "YOU");

  for (int col = 0; col < mc::kPits; ++col) {
    for (int row = 0; row < 2; ++row) {
      const int slot = row == 1 ? col : 12 - col;
      const fui::Rect r = lab::rect(pitsX + col * (pitW + gap), row == 0 ? top : bottom - pitH, pitW, pitH);
      const bool playable = mc::legal(board_, slot) && !machineToMove();
      fui::ButtonProps b;
      b.label = "";
      b.action = ActionPit;
      b.value = static_cast<int16_t>(slot);
      b.enabled = playable;
      b.styles = toybox::rowStyles();
      b.styles.normal.borderWidth = 0;
      b.styles.disabled = b.styles.normal;
      b.minTouchSize = 0;
      screen.button(b, r);
      if (slot == played_)
        lab::box(screen, r.x, r.y, r.width, r.height, fui::Paint::dither(fui::Color::LightGray), radius);
      screen.target().stroke(r, kInk, playable ? 5 : 2, static_cast<uint8_t>(radius));
      if (slot == last_) {
        screen.target().stroke(lab::rect(r.x + 8, r.y + 8, r.width - 16, r.height - 16), kInk, 2,
                               static_cast<uint8_t>(radius - 8));
      }
      if (slot == hint_) {
        screen.target().stroke(lab::rect(r.x + 10, r.y + 10, r.width - 20, r.height - 20), kInk, 5,
                               static_cast<uint8_t>(radius - 10));
      }
      // The count at the outer end, the seeds toward the middle of the board.
      count(lab::rect(r.x, row == 0 ? r.y + 8 : r.y + pitH - numH - 8, pitW, numH), slot, toybox::kUiFont,
            toybox::kUiCut);
      const int room = pitH - numH - 28;
      const int cy = row == 0 ? r.y + numH + 8 + room / 2 : r.y + 20 + room / 2;
      seeds(screen, board_.slot[slot], r.x + pitW / 2, cy, 3, room / 24, 10);
    }
  }
  lab::label(screen, lab::rect(pitsX, top + pitH, pitsW, statusH), status, toybox::kUiFont, toybox::kUiCut);

  if (help) {
    fui::ButtonProps side;
    side.styles = toybox::rowStyles();
    side.text = toybox::buttonText(screen.theme());
    side.radius = kRadius;
    side.label = "UNDO";
    side.action = ActionUndo;
    side.enabled = history_.count > 0 && !machineToMove();
    screen.button(side, lab::rect(x, bottom - helpH, storeW, helpH));
    side.label = "HINT";
    side.action = ActionHint;
    side.enabled = !machineToMove();
    screen.button(side, lab::rect(width - x - storeW, bottom - helpH, storeW, helpH));
  }
  if (finished) buildVerdict(screen, status, top, bottom);
}

void MancalaActivity::render(RenderLock&&) {
  renderer.setOrientation(wide_ ? GfxRenderer::Orientation::LandscapeCounterClockwise
                                : GfxRenderer::Orientation::Portrait);
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  if (howTo_.open) {
    toyboxhowto::build(screen, wide_ ? kHowToWide : kHowTo, howTo_);
  } else if (view_ == View::Menu) {
    if (wide_) {
      buildMenuWide(screen);
    } else {
      buildMenu(screen);
    }
  } else if (wide_) {
    buildBoardWide(screen);
  } else {
    buildBoard(screen);
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Mancala");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!cleanNext_ && ++fastPaints_ >= kCleanAfterPaints) cleanNext_ = true;
  if (cleanNext_) fastPaints_ = 0;
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}

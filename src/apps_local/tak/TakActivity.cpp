#include "TakActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"

namespace tk = tak;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/tak.sav";
// Bumped whenever SaveState or tak::Game changes shape: an old file is
// discarded rather than misread.
constexpr uint8_t kSaveVersion = 1;

struct SaveState {
  uint8_t inProgress;
  uint8_t vsComputer;
  uint8_t human;
  uint8_t menuSize;
  uint8_t level;
  uint8_t pad[3];
  uint32_t played;
  uint32_t won;
  uint64_t lastMove;
  char lastPtn[12];
  uint32_t gameBytes;
  tk::Game game;
};

uint64_t touched(const tk::Game& before, const tk::Move& m) {
  uint64_t mask = 1ull << m.square;
  if (m.kind == tk::MoveKind::Slide) {
    int at = m.square;
    const int count = tk::dropCount(m);
    for (int i = 0; i < count; ++i) {
      at = tk::step(before, at, static_cast<tk::Direction>(m.what));
      if (at < 0) break;
      mask |= 1ull << at;
    }
  }
  return mask;
}

}  // namespace

std::unique_ptr<Activity> TakActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TakActivity>(renderer, mappedInput);
}

void TakActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  screen = tk::Screen::Menu;
  hand = tk::Hand{};
  thinking = false;
  requestUpdate();
}

void TakActivity::onExit() {
  saveState();
  buffers.reset();
  Activity::onExit();
}

void TakActivity::goTo(const tk::Screen next) {
  screen = next;
  hand = tk::Hand{};
  requestUpdate();
}

bool TakActivity::computerToMove() const {
  return vsComputer && inProgress && !tk::over(game.result) && game.turn != human;
}

void TakActivity::newGame(const bool computer) {
  tk::reset(game, menuSize);
  vsComputer = computer;
  // Colours alternate against the computer, so the first move is not always
  // yours.
  if (computer) human = static_cast<uint8_t>(played % 2 == 0 ? tk::kWhite : tk::kBlack);
  inProgress = true;
  resultRecorded = false;
  lastMove = 0;
  lastPtn[0] = '\0';
  tray = tk::Stone::Flat;
  thinking = false;
  thinkingShown = false;
  saveState();
  goTo(tk::Screen::Board);
}

void TakActivity::playMove(const tk::Move& move) {
  // Both read only the board's size and the move, so the position before the
  // move does not need copying.
  tk::toPtn(game, move, lastPtn, sizeof(lastPtn));
  lastMove = touched(game, move);
  tk::apply(game, move);
  hand = tk::Hand{};
  tray = tk::defaultTray(game);
  saveState();
  finishIfOver();
  requestUpdate();
}

void TakActivity::finishIfOver() {
  if (!tk::over(game.result)) return;
  inProgress = false;
  if (!resultRecorded) {
    resultRecorded = true;
    if (vsComputer) {
      ++played;
      if (tk::winner(game.result) == human) ++won;
    }
  }
  saveState();
  screen = tk::Screen::Result;
}

void TakActivity::computerTurn() {
  if (!buffers) {
    buffers = makeUniqueNoThrow<tk::SearchBuffers>();
    if (!buffers) {
      LOG_ERR("TAK", "OOM: search buffers (%u bytes)", static_cast<unsigned>(sizeof(tk::SearchBuffers)));
      thinking = false;
      return;
    }
  }
  const uint32_t started = millis();
  const tk::SearchResult r = tk::search(game, *buffers, tk::limitsFor(level, toybox::seed()));
  LOG_INF("TAK", "searched %u nodes to depth %d in %u ms", static_cast<unsigned>(r.nodes), r.depth,
          static_cast<unsigned>(millis() - started));
  thinking = false;
  thinkingShown = false;
  if (!r.found) return;
  playMove(r.best);
}

void TakActivity::handleBoardTap(const int x, const int y) {
  const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
  const int square = takui::squareAt(device, game.size, x, y);
  if (square < 0) return;
  if (tk::over(game.result) || computerToMove() || thinking) return;
  tk::Move move;
  switch (tk::tapSquare(game, hand, square, tray, move)) {
    case tk::TapResult::Ignored:
      return;
    case tk::TapResult::Changed:
      requestUpdate();
      return;
    case tk::TapResult::Played:
      playMove(move);
      return;
  }
}

void TakActivity::loop() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (tk::leavesApp(screen)) {
      saveState();
      shelf::leave(renderer, mappedInput);
      return;
    }
    // A held stack goes back first; a second Back leaves the board.
    if (screen == tk::Screen::Board && hand.holding) {
      hand = tk::Hand{};
      requestUpdate();
      return;
    }
    saveState();
    goTo(tk::back(screen));
    return;
  }

  if (screen == tk::Screen::HowTo) {
    const bool forward = mappedInput.wasReleased(MappedInputManager::Button::Down);
    const bool backward = mappedInput.wasReleased(MappedInputManager::Button::Up);
    if (forward || backward) {
      const int pages = takui::howToPages();
      howToPage = (howToPage + (forward ? 1 : pages - 1)) % pages;
      requestUpdate();
      return;
    }
  }

  if (screen == tk::Screen::Board && computerToMove()) {
    if (!thinking) {
      // Paint the human's move and "THINKING" first; search next pass.
      thinking = true;
      thinkingShown = false;
      requestUpdate();
      return;
    }
    if (!thinkingShown) {
      thinkingShown = true;
      return;
    }
    computerTurn();
    return;
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  if (screen == tk::Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    if (takui::squareAt(device, game.size, tapX, tapY) >= 0) {
      handleBoardTap(tapX, tapY);
      return;
    }
  }

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case takui::ActionMenuRow:
      switch (static_cast<takui::MenuRow>(event.value)) {
        case takui::MenuRow::Resume:
          if (!inProgress) return;
          tray = tk::defaultTray(game);
          goTo(tk::Screen::Board);
          return;
        case takui::MenuRow::PlayComputer:
          newGame(true);
          return;
        case takui::MenuRow::PlayTwo:
          newGame(false);
          return;
        case takui::MenuRow::Size:
          menuSize = menuSize >= tk::kMaxSize ? tk::kMinSize : menuSize + 1;
          saveState();
          requestUpdate();
          return;
        case takui::MenuRow::Level:
          level = static_cast<tk::Level>((static_cast<int>(level) + 1) % 3);
          saveState();
          requestUpdate();
          return;
        case takui::MenuRow::HowTo:
          howToPage = 0;
          goTo(tk::Screen::HowTo);
          return;
        case takui::MenuRow::Count:
          return;
      }
      return;
    case takui::ActionHowToNext:
      if (howToPage + 1 < takui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(tk::Screen::Menu);
      return;
    case takui::ActionTray: {
      const tk::Stone choice = static_cast<tk::Stone>(event.value);
      if (choice == tray || !tk::trayAvailable(game, choice)) return;
      tray = choice;
      requestUpdate();
      return;
    }
    case takui::ActionCancel:
      hand = tk::Hand{};
      requestUpdate();
      return;
    case takui::ActionAgain:
      newGame(vsComputer);
      return;
    case takui::ActionDone:
      goTo(tk::Screen::Menu);
      return;
    default:
      return;
  }
}

void TakActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case tk::Screen::Menu: {
      takui::MenuModel model;
      model.canResume = inProgress;
      model.current = inProgress ? &game : nullptr;
      model.vsComputer = vsComputer;
      model.human = human;
      model.size = menuSize;
      model.level = level;
      model.played = played;
      model.won = won;
      takui::buildMenu(surface, model);
      break;
    }
    case tk::Screen::HowTo: {
      takui::HowToModel model;
      model.page = howToPage;
      takui::buildHowTo(surface, model);
      break;
    }
    case tk::Screen::Board: {
      // A member: two Games are past what a render stack frame should hold.
      // Every field is assigned below.
      takui::BoardModel& model = boardModel;
      model.game = game;
      tk::preview(game, hand, model.shown);
      model.vsComputer = vsComputer;
      model.human = human;
      model.thinking = thinking;
      model.tray = tray;
      model.holding = hand.holding;
      model.from = hand.from;
      model.at = hand.at;
      model.handCount = tk::inHand(hand);
      model.handColors = tk::handColors(game, hand);
      model.handTop = tk::handTop(game, hand);
      model.lastMove = hand.holding ? 0 : lastMove;
      std::memcpy(model.lastPtn, lastPtn, sizeof(lastPtn));
      takui::buildBoard(surface, model);
      break;
    }
    case tk::Screen::Result: {
      takui::ResultModel& model = resultModel;
      model.game = game;
      model.vsComputer = vsComputer;
      model.human = human;
      const uint8_t w = tk::winner(game.result);
      model.road = tk::roadWin(game.result) ? tk::roadSquares(game, w) : 0;
      takui::buildResult(surface, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Tak");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void TakActivity::saveState() {
  // Heap, not stack: a Game is ~600 bytes, past the local-variable budget.
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) {
    LOG_ERR("TAK", "OOM: save state");
    return;
  }
  s->inProgress = inProgress ? 1 : 0;
  s->vsComputer = vsComputer ? 1 : 0;
  s->human = human;
  s->menuSize = static_cast<uint8_t>(menuSize);
  s->level = static_cast<uint8_t>(level);
  s->played = static_cast<uint32_t>(played);
  s->won = static_cast<uint32_t>(won);
  s->lastMove = lastMove;
  std::memcpy(s->lastPtn, lastPtn, sizeof(lastPtn));
  s->gameBytes = sizeof(tk::Game);
  s->game = game;
  HalFile file;
  if (!Storage.openFileForWrite("TAK", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(s.get()), sizeof(SaveState));
  file.flush();
}

void TakActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("TAK", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  if (file.read(reinterpret_cast<uint8_t*>(s.get()), sizeof(SaveState)) != sizeof(SaveState)) return;
  if (s->gameBytes != sizeof(tk::Game)) return;
  played = static_cast<int>(s->played);
  won = static_cast<int>(s->won);
  menuSize = s->menuSize >= tk::kMinSize && s->menuSize <= tk::kMaxSize ? s->menuSize : 5;
  level = s->level <= 2 ? static_cast<tk::Level>(s->level) : tk::Level::Medium;
  vsComputer = s->vsComputer != 0;
  human = s->human == tk::kBlack ? tk::kBlack : tk::kWhite;
  const tk::Game& g = s->game;
  // Only a game whose shape still makes sense is resumed.
  const bool sane = g.size >= tk::kMinSize && g.size <= tk::kMaxSize && g.turn <= 1 && !tk::over(g.result);
  inProgress = s->inProgress != 0 && sane;
  if (inProgress) {
    game = g;
    lastMove = s->lastMove;
    std::memcpy(lastPtn, s->lastPtn, sizeof(lastPtn));
    lastPtn[sizeof(lastPtn) - 1] = '\0';
  }
}

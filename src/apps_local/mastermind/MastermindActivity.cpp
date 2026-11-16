#include "MastermindActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"

namespace mm = mastermind;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/mastermind.sav";
constexpr uint8_t kSaveVersion = 1;

struct SaveState {
  uint8_t inProgress;
  uint8_t recorded;
  mastermindui::Stats stats[mm::kModeCount];
  mm::Game game;
};

}  // namespace

std::unique_ptr<Activity> MastermindActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MastermindActivity>(renderer, mappedInput);
}

void MastermindActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  screen = Screen::Menu;
  requestUpdate();
}

void MastermindActivity::onExit() {
  saveState();
  Activity::onExit();
}

void MastermindActivity::goTo(const Screen next) {
  screen = next;
  requestUpdate();
}

void MastermindActivity::newGame(const mm::Mode mode) {
  mm::start(game, mode, toybox::seed());
  inProgress = true;
  recorded = false;
  saveState();
  goTo(Screen::Board);
}

void MastermindActivity::finishIfOver() {
  if (game.state == mm::State::Playing || recorded) return;
  recorded = true;
  inProgress = false;
  mastermindui::Stats& s = stats[static_cast<int>(game.mode)];
  ++s.played;
  if (game.state == mm::State::Won) {
    ++s.won;
    s.guessesInWins = static_cast<uint16_t>(s.guessesInWins + game.rowsUsed);
  }
  saveState();
}

void MastermindActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (screen == Screen::Menu) {
      saveState();
      shelf::leave(renderer, mappedInput);
      return;
    }
    saveState();
    goTo(Screen::Menu);
    return;
  }

  if (screen == Screen::HowTo) {
    const bool forward = mappedInput.wasReleased(MappedInputManager::Button::Down);
    const bool backward = mappedInput.wasReleased(MappedInputManager::Button::Up);
    if (forward || backward) {
      const int pages = mastermindui::howToPages();
      howToPage = (howToPage + (forward ? 1 : pages - 1)) % pages;
      requestUpdate();
      return;
    }
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

  if (screen == Screen::Board && game.state == mm::State::Playing) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    const int symbol = mastermindui::paletteAt(device, game, tapX, tapY);
    if (symbol >= 0) {
      if (mm::add(game, symbol)) requestUpdate();
      return;
    }
    const int slot = mastermindui::slotAt(device, game, tapX, tapY);
    if (slot >= 0) {
      if (mm::clearSlot(game, slot)) requestUpdate();
      return;
    }
  }

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case mastermindui::ActionMenuRow:
      switch (static_cast<mastermindui::MenuRow>(event.value)) {
        case mastermindui::MenuRow::Continue:
          if (inProgress) goTo(Screen::Board);
          return;
        case mastermindui::MenuRow::Easy:
          newGame(mm::Mode::Easy);
          return;
        case mastermindui::MenuRow::Classic:
          newGame(mm::Mode::Classic);
          return;
        case mastermindui::MenuRow::Hard:
          newGame(mm::Mode::Hard);
          return;
        case mastermindui::MenuRow::HowTo:
          howToPage = 0;
          goTo(Screen::HowTo);
          return;
        case mastermindui::MenuRow::Count:
          return;
      }
      return;
    case mastermindui::ActionHowToNext:
      if (howToPage + 1 < mastermindui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(Screen::Menu);
      return;
    case mastermindui::ActionGuess:
      if (mm::submit(game)) {
        finishIfOver();
        if (game.state == mm::State::Playing) saveState();
        requestUpdate();
      }
      return;
    case mastermindui::ActionClear:
      mm::clearRow(game);
      requestUpdate();
      return;
    case mastermindui::ActionAgain:
      newGame(game.mode);
      return;
    case mastermindui::ActionMenu:
      goTo(Screen::Menu);
      return;
    default:
      return;
  }
}

void MastermindActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case Screen::Menu: {
      mastermindui::MenuModel model;
      model.canContinue = inProgress;
      model.continueMode = game.mode;
      for (int i = 0; i < mm::kModeCount; ++i) model.stats[i] = stats[i];
      mastermindui::buildMenu(surface, model);
      break;
    }
    case Screen::HowTo: {
      mastermindui::HowToModel model;
      model.page = howToPage;
      mastermindui::buildHowTo(surface, model);
      break;
    }
    case Screen::Board: {
      mastermindui::BoardModel model;
      model.game = &game;
      mastermindui::buildBoard(surface, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Mastermind");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void MastermindActivity::saveState() {
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  s->inProgress = inProgress ? 1 : 0;
  s->recorded = recorded ? 1 : 0;
  for (int i = 0; i < mm::kModeCount; ++i) s->stats[i] = stats[i];
  s->game = game;
  HalFile file;
  if (!Storage.openFileForWrite("MMND", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(s.get()), sizeof(SaveState));
  file.flush();
}

void MastermindActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("MMND", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  if (file.read(reinterpret_cast<uint8_t*>(s.get()), sizeof(SaveState)) != sizeof(SaveState)) return;
  for (int i = 0; i < mm::kModeCount; ++i) stats[i] = s->stats[i];
  const mm::Game& g = s->game;
  const bool sane = static_cast<int>(g.mode) < mm::kModeCount && g.rowsUsed <= mm::kMaxRows;
  if (sane) {
    game = g;
    inProgress = s->inProgress != 0 && g.state == mm::State::Playing;
    recorded = s->recorded != 0;
  }
}

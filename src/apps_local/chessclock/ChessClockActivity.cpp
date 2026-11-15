#include "ChessClockActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "ClockScreens.h"

// The Buzzer library is in the device builds' lib_deps; the browser build does
// not have it, and a board without a buzzer links stubs whose begin() is false.
#if __has_include(<Buzzer.h>)
#include <Buzzer.h>
#define CHESSCLOCK_HAS_BUZZER 1
#else
#define CHESSCLOCK_HAS_BUZZER 0
#endif

namespace cc = chessclock;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/chessclock.sav";
constexpr uint8_t kSaveVersion = 1;

struct SaveState {
  uint8_t preset;
  uint8_t alwaysSeconds;
  uint8_t state;
  int8_t active;
  uint32_t remaining[2];
  uint16_t moves[2];
};

// Presses that start this close to the left edge are the start of the back
// swipe, not a hand on the clock.
constexpr int kBackSwipeEdge = 24;

}  // namespace

std::unique_ptr<Activity> ChessClockActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ChessClockActivity>(renderer, mappedInput);
}

void ChessClockActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  clock.reset(cc::kDefaultPreset);
  loadState();
  screen = Screen::Menu;
  requestUpdate();
}

void ChessClockActivity::onExit() {
  pauseIfRunning();
  saveState();
  Activity::onExit();
}

void ChessClockActivity::goTo(const Screen next) {
  screen = next;
  fullOnNextPaint = true;
  requestUpdate();
}

void ChessClockActivity::newGame() {
  clock.reset(clock.preset());
  saveState();
  goTo(Screen::Face);
}

void ChessClockActivity::pauseIfRunning() {
  if (clock.state() == cc::State::Running) clock.togglePause(static_cast<uint32_t>(millis()));
}

void ChessClockActivity::ring() {
  fullOnNextPaint = true;
  haptic_feedback::touchAction(true);
#if CHESSCLOCK_HAS_BUZZER
  Buzzer buzzer;
  if (buzzer.begin()) {
    // One long low tone: the flag has fallen, and the game is over.
    buzzer.tone(1319, 700);
    buzzer.end();
  }
#endif
}

bool ChessClockActivity::shownChanged() {
  const uint32_t now = static_cast<uint32_t>(millis());
  bool changed = false;
  for (int side = 0; side < 2; ++side) {
    const cc::Shown s = cc::shown(clock.remainingMs(side, now), alwaysSeconds);
    if (s != lastShown[side]) changed = true;
  }
  return changed;
}

void ChessClockActivity::loop() {
  const uint32_t now = static_cast<uint32_t>(millis());

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (screen == Screen::Menu) {
      saveState();
      shelf::leave(renderer, mappedInput);
      return;
    }
    pauseIfRunning();
    saveState();
    goTo(Screen::Menu);
    return;
  }

  if (screen == Screen::Face) {
    if (clock.tick(now)) {
      ring();
      saveState();
      requestUpdate();
      return;
    }

    // Confirm (the Sticky's third key) pauses and resumes.
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (clock.togglePause(now)) {
        if (clock.state() == cc::State::Paused) {
          fullOnNextPaint = true;
          saveState();
        }
        requestUpdate();
      }
      return;
    }

    int px = 0;
    int py = 0;
    if (mappedInput.wasScreenTouchPressed(px, py) && px >= kBackSwipeEdge) {
      const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
      const int side = clockui::sideAt(device, px, py);
      if (side >= 0 && clock.press(side, now)) {
        haptic_feedback::touchAction();
        requestUpdate();
        return;
      }
    }

    if (clock.state() == cc::State::Running && shownChanged()) requestUpdate();
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

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case clockui::ActionMenuRow:
      switch (static_cast<clockui::MenuRow>(event.value)) {
        case clockui::MenuRow::Start:
          newGame();
          return;
        case clockui::MenuRow::Resume:
          goTo(Screen::Face);
          return;
        case clockui::MenuRow::Time:
          // A new time control is a new game: a paused one is set aside, which
          // is why RESUME disappears from the menu at the same moment.
          clock.reset((clock.preset() + 1) % cc::kPresetCount);
          saveState();
          requestUpdate();
          return;
        case clockui::MenuRow::Seconds:
          alwaysSeconds = !alwaysSeconds;
          saveState();
          requestUpdate();
          return;
        case clockui::MenuRow::Count:
          return;
      }
      return;
    case clockui::ActionPause:
      if (clock.togglePause(now)) {
        if (clock.state() == cc::State::Paused) {
          fullOnNextPaint = true;
          saveState();
        }
        requestUpdate();
      }
      return;
    case clockui::ActionNew:
      newGame();
      return;
    default:
      return;
  }
}

void ChessClockActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  if (screen == Screen::Menu) {
    clockui::MenuModel model;
    model.preset = clock.preset();
    model.alwaysSeconds = alwaysSeconds;
    model.canResume = clock.state() == cc::State::Paused;
    clockui::buildMenu(surface, model);
  } else {
    const uint32_t now = static_cast<uint32_t>(millis());
    clockui::FaceModel model;
    model.state = clock.state();
    model.preset = clock.preset();
    model.active = clock.active();
    model.flagged = clock.flagged();
    for (int side = 0; side < 2; ++side) {
      model.shown[side] = cc::shown(clock.remainingMs(side, now), alwaysSeconds);
      lastShown[side] = model.shown[side];
      model.moves[side] = clock.moves(side);
    }
    clockui::buildFace(surface, model);
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "ChessClock");

  if (screen == Screen::Menu) {
    const auto labels = mappedInput.mapLabels("Back", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer(fullOnNextPaint ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  fullOnNextPaint = false;
}

void ChessClockActivity::saveState() {
  SaveState s{};
  s.preset = static_cast<uint8_t>(clock.preset());
  s.alwaysSeconds = alwaysSeconds ? 1 : 0;
  s.state = static_cast<uint8_t>(clock.state());
  s.active = static_cast<int8_t>(clock.active());
  for (int side = 0; side < 2; ++side) {
    s.remaining[side] = clock.storedMs(side);
    s.moves[side] = static_cast<uint16_t>(clock.moves(side));
  }
  HalFile file;
  if (!Storage.openFileForWrite("CLOK", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(&s), sizeof(s));
  file.flush();
}

void ChessClockActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("CLOK", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  SaveState s{};
  if (file.read(reinterpret_cast<uint8_t*>(&s), sizeof(s)) != sizeof(s)) return;
  alwaysSeconds = s.alwaysSeconds != 0;
  const int preset = s.preset < cc::kPresetCount ? s.preset : cc::kDefaultPreset;
  const cc::State state =
      s.state <= static_cast<uint8_t>(cc::State::Flagged) ? static_cast<cc::State>(s.state) : cc::State::Ready;
  if (state == cc::State::Paused || state == cc::State::Running) {
    clock.restore(preset, s.active, s.remaining, s.moves, state);
  } else {
    // A finished or unstarted game comes back as a fresh one at the same time.
    clock.reset(preset);
  }
}

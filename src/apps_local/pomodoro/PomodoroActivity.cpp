#include "PomodoroActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"

// The Buzzer library is in the device builds' lib_deps; the browser build does
// not have it, and a board without a buzzer links stubs whose begin() is false.
#if __has_include(<Buzzer.h>)
#include <Buzzer.h>
#define POMODORO_HAS_BUZZER 1
#else
#define POMODORO_HAS_BUZZER 0
#endif

namespace {

namespace pd = pomodoro;
namespace fui = freeink::ui;

constexpr char kSavePath[] = "/.crosspoint/pomodoro.sav";
constexpr uint8_t kSaveVersion = 1;

struct SaveState {
  uint8_t focusMinutes;
  uint8_t breakMinutes;
  uint8_t phase;
  uint8_t doneInCycle;
  uint32_t totalDone;
} __attribute__((packed));

}  // namespace

std::unique_ptr<Activity> PomodoroActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<PomodoroActivity>(renderer, mappedInput);
}

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  view = View::Timer;
  shownMinutes = -1;
  justFinished = false;
  flashOnNextPaint = false;
  requestUpdate();
}

void PomodoroActivity::onExit() {
  saveState();
  Activity::onExit();
}

void PomodoroActivity::togglePrimary() {
  justFinished = false;
  timer.toggle(static_cast<uint32_t>(millis()));
  shownMinutes = -1;
  requestUpdate();
}

void PomodoroActivity::skip() {
  justFinished = false;
  timer.skip();
  saveState();
  requestUpdate();
}

void PomodoroActivity::route(const fui::ActionEvent& event) {
  namespace ui = pomodoroui;
  const bool ready = timer.state() == pd::State::Ready;
  switch (event.action) {
    case ui::ActionPrimary:
      haptic_feedback::touchAction();
      togglePrimary();
      return;
    case ui::ActionSkip:
      haptic_feedback::touchAction();
      skip();
      return;
    case ui::ActionReset:
      haptic_feedback::touchAction();
      justFinished = false;
      timer.reset();
      saveState();
      break;
    case ui::ActionPickFocus:
      if (!ready) return;
      view = View::PickFocus;
      break;
    case ui::ActionPickBreak:
      if (!ready) return;
      view = View::PickBreak;
      break;
    case ui::ActionChoose:
      if (view == View::PickFocus) timer.setFocus(event.value);
      if (view == View::PickBreak) timer.setBreak(event.value);
      haptic_feedback::touchAction();
      saveState();
      view = View::Timer;
      break;
    default:
      return;
  }
  requestUpdate();
}

void PomodoroActivity::ring() {
  flashOnNextPaint = true;
  justFinished = true;
  haptic_feedback::touchAction(true);
#if POMODORO_HAS_BUZZER
  Buzzer buzzer;
  if (buzzer.begin()) {
    // Three short rising beeps. tone() blocks, which is fine here: the whole
    // thing is under a second and there is nothing else to do at the bell.
    const uint32_t notes[] = {1760, 2093, 2637};
    for (int round = 0; round < 2; ++round) {
      for (const uint32_t f : notes) {
        buzzer.tone(f, 120);
        delay(60);
      }
      delay(250);
    }
    buzzer.end();
  }
#endif
}

void PomodoroActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view != View::Timer) {
      view = View::Timer;
      requestUpdate();
      return;
    }
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }

  const uint32_t now = static_cast<uint32_t>(millis());
  if (timer.tick(now)) {
    ring();
    saveState();
    view = View::Timer;
    shownMinutes = -1;
    requestUpdate();
    return;
  }
  if (view == View::Timer && timer.state() == pd::State::Running && timer.remainingMinutes(now) != shownMinutes) {
    requestUpdate();
  }

  // Two keys and touch is the floor (docs/buttons.md). Next/Down and the
  // Sticky's Confirm are the big button; Previous/Up skips, but only while
  // stopped, so a key brushed in a bag cannot throw away twenty minutes.
  if (view == View::Timer) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      togglePrimary();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) && timer.state() != pd::State::Running) {
      skip();
      return;
    }
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  route(interactions.route(input));
}

void PomodoroActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  const uint32_t now = static_cast<uint32_t>(millis());
  const pd::State state = timer.state();
  const char* primary = state == pd::State::Running ? "Pause" : (state == pd::State::Paused ? "Resume" : "Start");

  if (view == View::Timer) {
    pomodoroui::TimerModel m;
    m.phase = timer.phase();
    m.state = state;
    m.minutes = timer.remainingMinutes(now);
    m.progressPermille = timer.progressPermille(now);
    m.doneInCycle = timer.doneInCycle();
    m.totalDone = timer.totalDone();
    m.focusMinutes = timer.focusMinutes;
    m.breakMinutes = timer.breakMinutes;
    m.justFinished = justFinished;
    m.faces.ctx = &target;
    m.faces.bindTitle = [](void* ctx, const bool huge) {
      static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE,
                                                         huge ? toybox::kHugeFontId : toybox::kDisplayFontId);
    };
    shownMinutes = m.minutes;
    pomodoroui::buildTimer(surface, m);
  } else {
    pomodoroui::PickerModel m;
    m.focus = view == View::PickFocus;
    m.current = m.focus ? timer.focusMinutes : timer.breakMinutes;
    pomodoroui::buildPicker(surface, m);
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Pomodoro");
  const auto labels = view == View::Timer
                          ? mappedInput.mapLabels("Back", primary, state == pd::State::Running ? "" : "Skip", primary)
                          : mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(flashOnNextPaint ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  flashOnNextPaint = false;
}

void PomodoroActivity::saveState() {
  SaveState s{};
  s.focusMinutes = static_cast<uint8_t>(timer.focusMinutes);
  s.breakMinutes = static_cast<uint8_t>(timer.breakMinutes);
  s.phase = static_cast<uint8_t>(timer.phase());
  s.doneInCycle = static_cast<uint8_t>(timer.doneInCycle());
  s.totalDone = timer.totalDone();
  HalFile file;
  if (!Storage.openFileForWrite("POMO", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(&s), sizeof(s));
  file.flush();
}

void PomodoroActivity::loadState() {
  timer = pd::Timer{};
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("POMO", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  SaveState s{};
  if (file.read(reinterpret_cast<uint8_t*>(&s), sizeof(s)) != sizeof(s)) return;
  // Through the pickers' own check, so an edited or old file cannot set a
  // length they would never offer.
  timer.setFocus(s.focusMinutes);
  timer.setBreak(s.breakMinutes);
  timer.restore(static_cast<pd::Phase>(s.phase <= 2 ? s.phase : 0), s.doneInCycle);
  timer.setTotalDone(s.totalDone);
}

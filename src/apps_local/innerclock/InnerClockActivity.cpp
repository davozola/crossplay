#include "InnerClockActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "InnerScreens.h"

namespace ic = innerclock;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/innerclock.sav";
constexpr uint8_t kSaveVersion = 1;

struct SaveState {
  uint8_t targetIndex;
  ic::Record record;
};

// Two presses closer than this are one tap bouncing.
constexpr uint32_t kDebounceMs = 250;
// Presses starting this close to the left edge are the back swipe.
constexpr int kBackSwipeEdge = 24;

}  // namespace

std::unique_ptr<Activity> InnerClockActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<InnerClockActivity>(renderer, mappedInput);
}

void InnerClockActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  round.clear();
  requestUpdate();
}

void InnerClockActivity::onExit() {
  saveState();
  Activity::onExit();
}

void InnerClockActivity::loop() {
  // Stamp first, before any other work this pass.
  const uint32_t now = static_cast<uint32_t>(millis());

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // Back during a round abandons it; on the main screen it leaves.
    if (round.state() == ic::State::Running) {
      round.clear();
      requestUpdate();
      return;
    }
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }

  int px = 0;
  int py = 0;
  if (mappedInput.wasScreenTouchPressed(px, py) && px >= kBackSwipeEdge && now - lastPressMs >= kDebounceMs) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    const fui::Rect pad = innerui::padRect(device);
    if (px >= pad.x && px < pad.right() && py >= pad.y && py < pad.bottom()) {
      lastPressMs = now;
      if (round.state() == ic::State::Running) {
        round.stop(now, record);
        saveState();
      } else {
        round.start(now);
      }
      haptic_feedback::touchAction();
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
  const fui::ActionEvent event = interactions.route(input);
  if (event.action == innerui::ActionTarget && round.state() != ic::State::Running &&
      event.value != round.targetIndex()) {
    round.setTarget(event.value);
    round.clear();
    saveState();
    requestUpdate();
  }
}

void InnerClockActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  innerui::Model model;
  model.state = round.state();
  model.targetIndex = round.targetIndex();
  model.elapsedMs = round.elapsedMs();
  model.diffMs = round.diffMs();
  model.newBest = round.newBest();
  model.record = &record;
  innerui::build(surface, model);

  interactionsReady = true;
  toybox::reportOverflow(interactions, "InnerClock");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void InnerClockActivity::saveState() {
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  s->targetIndex = static_cast<uint8_t>(round.targetIndex());
  s->record = record;
  HalFile file;
  if (!Storage.openFileForWrite("INNR", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(s.get()), sizeof(SaveState));
  file.flush();
}

void InnerClockActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("INNR", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  if (file.read(reinterpret_cast<uint8_t*>(s.get()), sizeof(SaveState)) != sizeof(SaveState)) return;
  record = s->record;
  if (record.recentCount < 0 || record.recentCount > ic::kRecent) record.recentCount = 0;
  round.setTarget(s->targetIndex);
}

#include "ClockActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <ctime>

#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"

namespace {

constexpr char kSavePath[] = "/.crosspoint/clock.sav";
constexpr uint8_t kSaveVersion = 1;
constexpr uint32_t kHintMs = 12000;
constexpr int kBackSwipeEdge = 28;
// Before this the device's clock has plainly never been set.
constexpr time_t kClockSetAfter = 1704067200;  // 2024-01-01

}  // namespace

std::unique_ptr<Activity> ClockActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ClockActivity>(renderer, mappedInput);
}

void ClockActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  applyOrientation();
  shownMinute_ = -1;
  cleanNext_ = true;
  hintUntilMs_ = static_cast<uint32_t>(millis()) + kHintMs;
  requestUpdate();
}

void ClockActivity::onExit() {
  saveState();
  // The orientation is the renderer's, not this app's: hand it back upright.
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  Activity::onExit();
}

void ClockActivity::applyOrientation() {
  renderer.setOrientation(wide_ ? GfxRenderer::Orientation::LandscapeCounterClockwise
                                : GfxRenderer::Orientation::Portrait);
}

clockfaces::Moment ClockActivity::now() const {
  clockfaces::Moment m;
  const time_t t = time(nullptr);
  if (t < kClockSetAfter) return m;
  struct tm parts;
  localtime_r(&t, &parts);
  m.valid = true;
  m.hour = parts.tm_hour;
  m.minute = parts.tm_min;
  m.weekday = parts.tm_wday;
  m.day = parts.tm_mday;
  m.month = parts.tm_mon + 1;
  return m;
}

void ClockActivity::changeFace(const int by) {
  face_ = ((face_ + by) % clockcore::kFaces + clockcore::kFaces) % clockcore::kFaces;
  cleanNext_ = true;
  saveState();
  requestUpdate();
}

void ClockActivity::loop() {
  int px = 0;
  int py = 0;
  if (mappedInput.wasScreenTouchPressed(px, py)) pressX_ = px;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // A swipe to the right from the left quarter is the system's Back. Here it
    // is the face before, unless it began at the very edge.
    if (mappedInput.wasBackGesture() && pressX_ >= kBackSwipeEdge) {
      pressX_ = -1;
      changeFace(-1);
      return;
    }
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }

  switch (mappedInput.wasSwipe()) {
    case MappedInputManager::SwipeDir::Left:
      changeFace(1);
      return;
    case MappedInputManager::SwipeDir::Right:
      changeFace(-1);
      return;
    case MappedInputManager::SwipeDir::Up:
    case MappedInputManager::SwipeDir::Down:
      wide_ = !wide_;
      applyOrientation();
      cleanNext_ = true;
      saveState();
      requestUpdate();
      return;
    case MappedInputManager::SwipeDir::None:
      break;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    changeFace(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    changeFace(1);
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    dark_ = !dark_;
    cleanNext_ = true;
    saveState();
    requestUpdate();
    return;
  }

  // The minute turning over, or the hint running out.
  const clockfaces::Moment m = now();
  const int minute = m.valid ? m.hour * 60 + m.minute : -2;
  const uint32_t nowMs = static_cast<uint32_t>(millis());
  if (minute != shownMinute_) {
    if (m.valid && m.minute == 0) cleanNext_ = true;  // once an hour, wipe the ghosts
    requestUpdate();
  } else if (hintUntilMs_ != 0 && static_cast<int32_t>(nowMs - hintUntilMs_) >= 0) {
    hintUntilMs_ = 0;
    requestUpdate();
  }
}

void ClockActivity::render(RenderLock&&) {
  const clockfaces::Moment m = now();
  shownMinute_ = m.valid ? m.hour * 60 + m.minute : -2;
  // The clip is in logical coordinates and whoever drew last left theirs:
  // open it to the whole panel as it is turned now.
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  renderer.clearScreen();
  clockfaces::draw(renderer, face_, m);
  if (hintUntilMs_ != 0 && static_cast<int32_t>(static_cast<uint32_t>(millis()) - hintUntilMs_) < 0) {
    clockfaces::drawHint(renderer, m.valid ? "DESLIZA: ESTILO    ARRIBA: GIRAR    TOCA: INVERTIR"
                                           : "EL RELOJ NO TIENE HORA: PONLA EN AJUSTES");
  } else {
    hintUntilMs_ = 0;
  }
  if (dark_) renderer.invertScreen();
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}

void ClockActivity::saveState() {
  HalFile file;
  if (!Storage.openFileForWrite("CLCK", kSavePath, file)) return;
  const uint8_t data[4] = {kSaveVersion, static_cast<uint8_t>(face_), static_cast<uint8_t>(wide_ ? 1 : 0),
                           static_cast<uint8_t>(dark_ ? 1 : 0)};
  file.write(data, sizeof(data));
  file.flush();
}

void ClockActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("CLCK", kSavePath, file)) return;
  uint8_t data[4] = {};
  if (file.read(data, sizeof(data)) != sizeof(data) || data[0] != kSaveVersion) return;
  face_ = data[1] < clockcore::kFaces ? data[1] : 0;
  wide_ = data[2] == 1;
  dark_ = data[3] == 1;
}

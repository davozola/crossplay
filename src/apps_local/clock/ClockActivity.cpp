#include "ClockActivity.h"

#include <HalStorage.h>
#include <HalTiltSensor.h>
#include <Memory.h>

#include <ctime>

#include "../Shelf.h"
#include "../power/LightNap.h"
#include "../ui/ToyboxFonts.h"

namespace {

constexpr char kSavePath[] = "/.crosspoint/clock.sav";
constexpr uint8_t kSaveVersion = 2;
constexpr uint32_t kHintMs = 12000;
constexpr int kBackSwipeEdge = 28;
// How long after the last touch the clock stays fully awake, so a run of
// swipes through the faces is not interrupted by naps.
constexpr uint32_t kAwakeAfterInputMs = 20000;
// How often the device's sense of down is read, and how far over it has to
// lean (in g along one edge) before that counts as standing on that edge.
constexpr uint32_t kGravityEveryMs = 700;
constexpr float kLeanG = 0.6f;
constexpr float kFlatG = 0.85f;
constexpr uint8_t kLeanReads = 2;
// A nap no longer than this while the face follows the device, so that laying
// it down is noticed in a few seconds and not at the next minute.
constexpr uint64_t kFollowNapMicros = 2500000ULL;
// Before this the device's clock has plainly never been set.
constexpr time_t kClockSetAfter = 1704067200;  // 2024-01-01

// The simulator's sensor is its own class and has no gravity to read.
#if defined(SIMULATOR)
bool senseThere() { return false; }
void senseHold(bool) {}
bool senseRead(float&, float&, float&) { return false; }
#else
bool senseThere() { return halTiltSensor.isAvailable(); }
void senseHold(const bool on) { halTiltSensor.hold(on); }
bool senseRead(float& ax, float& ay, float& az) { return halTiltSensor.readGravity(ax, ay, az); }
#endif

}  // namespace

std::unique_ptr<Activity> ClockActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ClockActivity>(renderer, mappedInput);
}

void ClockActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  if (!senseThere() && turn_ == Turn::Auto) turn_ = wide_ ? Turn::Wide : Turn::Tall;
  senseHold(turn_ == Turn::Auto);
  applyOrientation();
  shownMinute_ = -1;
  cleanNext_ = true;
  hintUntilMs_ = static_cast<uint32_t>(millis()) + kHintMs;
  lastInputMs_ = static_cast<uint32_t>(millis());
  ask();
}

void ClockActivity::onExit() {
  saveState();
  senseHold(false);
  // The orientation is the renderer's, not this app's: hand it back upright.
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  Activity::onExit();
}

void ClockActivity::ask() {
  paintPending_ = true;
  requestUpdate();
}

void ClockActivity::applyOrientation() {
  renderer.setOrientation(!wide_     ? GfxRenderer::Orientation::Portrait
                          : flipped_ ? GfxRenderer::Orientation::LandscapeClockwise
                                     : GfxRenderer::Orientation::LandscapeCounterClockwise);
}

bool ClockActivity::followGravity(const uint32_t nowMs) {
  if (turn_ != Turn::Auto || nowMs - gravityAtMs_ < kGravityEveryMs) return false;
  gravityAtMs_ = nowMs;
  float ax = 0;
  float ay = 0;
  float az = 0;
  if (!senseRead(ax, ay, az)) return false;
  // Lying on its back it has no way up; it keeps the one it had.
  if (az > kFlatG || az < -kFlatG) return false;
  // The frame has X along the long edge, which is the one that is vertical
  // when the device stands upright.
  uint8_t pose = 0;
  if (ax > kLeanG || ax < -kLeanG) {
    pose = 1;
  } else if (ay > kLeanG) {
    pose = 2;
  } else if (ay < -kLeanG) {
    pose = 3;
  }
  if (pose == 0) return false;
  if (pose != leaning_) {
    leaning_ = pose;
    leaningFor_ = 1;
    return false;
  }
  if (leaningFor_ < kLeanReads) ++leaningFor_;
  if (leaningFor_ < kLeanReads) return false;
  const bool wide = pose != 1;
  const bool flipped = pose == 3;
  if (wide == wide_ && (!wide || flipped == flipped_)) return false;
  wide_ = wide;
  flipped_ = flipped;
  applyOrientation();
  cleanNext_ = true;
  return true;
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
  ask();
}

void ClockActivity::loop() {
  int px = 0;
  int py = 0;
  if (mappedInput.wasScreenTouchPressed(px, py)) {
    pressX_ = px;
    lastInputMs_ = static_cast<uint32_t>(millis());
  }

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
      // Round the three: following the device, held upright, held on its side.
      // Without a sensor there are only the two.
      if (turn_ == Turn::Auto) {
        turn_ = Turn::Tall;
      } else if (turn_ == Turn::Tall) {
        turn_ = Turn::Wide;
      } else {
        turn_ = senseThere() ? Turn::Auto : Turn::Tall;
      }
      if (turn_ != Turn::Auto) {
        wide_ = turn_ == Turn::Wide;
        flipped_ = false;
      }
      leaning_ = 0;
      senseHold(turn_ == Turn::Auto);
      note_ = turn_ == Turn::Auto ? "GIRO: SOLO, SEGUN LA POSTURA" : (wide_ ? "GIRO: APAISADO" : "GIRO: VERTICAL");
      hintUntilMs_ = static_cast<uint32_t>(millis()) + 4000;
      applyOrientation();
      cleanNext_ = true;
      saveState();
      ask();
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
    ask();
    return;
  }

  // The minute turning over, or the hint running out.
  const clockfaces::Moment m = now();
  const int minute = m.valid ? m.hour * 60 + m.minute : -2;
  const uint32_t nowMs = static_cast<uint32_t>(millis());
  if (followGravity(nowMs)) {
    saveState();
    ask();
    return;
  }
  if (minute != shownMinute_) {
    if (m.valid && m.minute == 0) cleanNext_ = true;  // once an hour, wipe the ghosts
    ask();
    return;
  }
  if (hintUntilMs_ != 0) {
    if (static_cast<int32_t>(nowMs - hintUntilMs_) >= 0) {
      hintUntilMs_ = 0;
      ask();
    }
    return;
  }

  // Nothing to paint and nobody here: nap until the minute turns. The paint
  // that put this minute up must have reached the panel first.
  if (!m.valid || paintPending_ || nowMs - lastInputMs_ < kAwakeAfterInputMs) return;
  const time_t t = time(nullptr);
  struct tm parts;
  localtime_r(&t, &parts);
  uint64_t micros = static_cast<uint64_t>(60 - parts.tm_sec) * 1000000ULL + 250000ULL;
  if (turn_ == Turn::Auto && micros > kFollowNapMicros) micros = kFollowNapMicros;
  if (lightnap::nap(micros) == lightnap::Woke::Input) lastInputMs_ = static_cast<uint32_t>(millis());
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
    clockfaces::drawHint(renderer, note_ != nullptr ? note_
                                   : m.valid        ? "DESLIZA: ESTILO    ARRIBA: GIRO    TOCA: INVERTIR"
                                                    : "EL RELOJ NO TIENE HORA: PONLA EN AJUSTES");
  } else {
    hintUntilMs_ = 0;
    note_ = nullptr;
  }
  if (dark_) renderer.invertScreen();
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
  paintPending_ = false;
}

void ClockActivity::saveState() {
  HalFile file;
  if (!Storage.openFileForWrite("CLCK", kSavePath, file)) return;
  const uint8_t data[6] = {kSaveVersion,
                           static_cast<uint8_t>(face_),
                           static_cast<uint8_t>(wide_ ? 1 : 0),
                           static_cast<uint8_t>(dark_ ? 1 : 0),
                           static_cast<uint8_t>(turn_),
                           static_cast<uint8_t>(flipped_ ? 1 : 0)};
  file.write(data, sizeof(data));
  file.flush();
}

void ClockActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("CLCK", kSavePath, file)) return;
  uint8_t data[6] = {};
  const int got = file.read(data, sizeof(data));
  // Version 1 had the first four bytes and no way of turning by itself.
  if (got < 4 || (data[0] != kSaveVersion && data[0] != 1)) return;
  face_ = data[1] < clockcore::kFaces ? data[1] : 0;
  wide_ = data[2] == 1;
  dark_ = data[3] == 1;
  if (data[0] == kSaveVersion && got == 6) {
    turn_ = data[4] <= static_cast<uint8_t>(Turn::Wide) ? static_cast<Turn>(data[4]) : Turn::Auto;
    flipped_ = data[5] == 1;
  }
}

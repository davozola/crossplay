#pragma once

// PRESENTER, the part with no hardware in it: the session clock, what it shows,
// and the keyboard reports that turn a tap into a slide.

#include <cstdint>
#include <cstdio>

namespace presenter {

// --- the keys -----------------------------------------------------------

enum class Key : uint8_t { Next, Previous, Blank, Count };
enum class KeySet : uint8_t { Arrows, Pages, Count };

// USB HID keyboard usages.
constexpr uint8_t kUsageRight = 0x4F;
constexpr uint8_t kUsageLeft = 0x50;
constexpr uint8_t kUsagePageDown = 0x4E;
constexpr uint8_t kUsagePageUp = 0x4B;
constexpr uint8_t kUsageB = 0x05;

constexpr uint8_t usageFor(const Key key, const KeySet set) {
  if (key == Key::Blank) return kUsageB;
  if (set == KeySet::Pages) return key == Key::Next ? kUsagePageDown : kUsagePageUp;
  return key == Key::Next ? kUsageRight : kUsageLeft;
}

constexpr uint8_t kReportId = 1;
constexpr int kReportBytes = 8;  // modifiers, reserved, six keys

// One key down, or all keys up when usage is 0.
inline void fillReport(uint8_t report[kReportBytes], const uint8_t usage) {
  for (int i = 0; i < kReportBytes; ++i) report[i] = 0;
  report[2] = usage;
}

// A plain keyboard, the shape every host already has a driver for.
constexpr uint8_t kReportMap[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x85, kReportId,  //   Report ID
    0x05, 0x07,       //   Usage Page (Keyboard)
    0x19, 0xE0,       //   Usage Minimum (Left Control)
    0x29, 0xE7,       //   Usage Maximum (Right GUI)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute): modifiers
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x01,       //   Input (Constant): reserved
    0x95, 0x05,       //   Report Count (5)
    0x75, 0x01,       //   Report Size (1)
    0x05, 0x08,       //   Usage Page (LEDs)
    0x19, 0x01,       //   Usage Minimum (Num Lock)
    0x29, 0x05,       //   Usage Maximum (Kana)
    0x91, 0x02,       //   Output (Data, Variable, Absolute): LEDs
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x03,       //   Report Size (3)
    0x91, 0x01,       //   Output (Constant): padding
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x65,       //   Logical Maximum (101)
    0x05, 0x07,       //   Usage Page (Keyboard)
    0x19, 0x00,       //   Usage Minimum (0)
    0x29, 0x65,       //   Usage Maximum (101)
    0x81, 0x00,       //   Input (Data, Array): keys
    0xC0,             // End Collection
};

// --- the clock ----------------------------------------------------------

// Wraps with millis() and stays right, since only differences are taken.
struct Clock {
  uint32_t bankedMs = 0;
  uint32_t since = 0;
  bool running = false;

  uint32_t elapsedMs(const uint32_t now) const { return bankedMs + (running ? now - since : 0); }
  bool untouched() const { return !running && bankedMs == 0; }
  void start(const uint32_t now) {
    if (running) return;
    since = now;
    running = true;
  }
  void pause(const uint32_t now) {
    if (!running) return;
    bankedMs += now - since;
    running = false;
  }
  void reset() {
    bankedMs = 0;
    running = false;
  }
};

enum class Phase : uint8_t { Open, Plenty, Five, Two, Over };
enum class Warn : uint8_t { FiveAndTwo, TwoOnly, None, Count };

constexpr int kLimits[] = {0, 5, 10, 15, 20, 30, 45, 60, 90, 120};  // minutes; 0 is no limit
constexpr int kLimitCount = sizeof(kLimits) / sizeof(kLimits[0]);

// What the big figure says and how loudly.
struct Face {
  Phase phase = Phase::Open;
  int minutes = 0;       // left, used or over, by phase and by what was asked for
  bool counted = false;  // true when `minutes` is time left

  bool operator==(const Face& o) const { return phase == o.phase && minutes == o.minutes && counted == o.counted; }
  bool operator!=(const Face& o) const { return !(*this == o); }
};

inline Face faceFor(const uint32_t elapsedMs, const int limitMinutes, const bool showLeft, const Warn warn) {
  Face face;
  const uint32_t usedSec = elapsedMs / 1000;
  if (limitMinutes <= 0) {
    face.minutes = static_cast<int>(usedSec / 60);
    return face;
  }
  const uint32_t limitSec = static_cast<uint32_t>(limitMinutes) * 60;
  if (usedSec >= limitSec) {
    face.phase = Phase::Over;
    face.minutes = static_cast<int>((usedSec - limitSec) / 60);
    return face;
  }
  const uint32_t leftSec = limitSec - usedSec;
  face.phase = Phase::Plenty;
  if (warn != Warn::None && leftSec <= 120) {
    face.phase = Phase::Two;
  } else if (warn == Warn::FiveAndTwo && leftSec <= 300 && limitMinutes > 5) {
    face.phase = Phase::Five;
  }
  face.counted = showLeft || face.phase != Phase::Plenty;
  // Time left rounds up, so "5" is on screen for the whole of the fifth minute
  // from the end; time used rounds down, like a watch.
  face.minutes = face.counted ? static_cast<int>((leftSec + 59) / 60) : static_cast<int>(usedSec / 60);
  return face;
}

inline const char* captionFor(const Face& face) {
  switch (face.phase) {
    case Phase::Open:
      return "MINUTES SO FAR";
    case Phase::Plenty:
      return face.counted ? "MINUTES LEFT" : "MINUTES SO FAR";
    case Phase::Five:
      return face.minutes == 1 ? "MINUTE LEFT" : "MINUTES LEFT";
    case Phase::Two:
      return face.minutes == 1 ? "MINUTE LEFT: WRAP UP" : "MINUTES LEFT: WRAP UP";
    case Phase::Over:
      return face.minutes == 1 ? "MINUTE OVER TIME" : "MINUTES OVER TIME";
  }
  return "";
}

}  // namespace presenter

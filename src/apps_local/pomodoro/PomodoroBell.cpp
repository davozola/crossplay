#include "PomodoroBell.h"

#include <Arduino.h>
#include <HalStorage.h>

#include <ctime>

// The Buzzer library is in the device builds' lib_deps; the browser build does
// not have it, and a board without a buzzer links stubs whose begin() is false.
#if __has_include(<Buzzer.h>)
#include <Buzzer.h>
#define POMODORO_BELL_HAS_BUZZER 1
#else
#define POMODORO_BELL_HAS_BUZZER 0
#endif

namespace pomodorobell {

namespace {

constexpr char kPath[] = "/.crosspoint/pomodoro.bell";
// A clock that was never set reads as the seventies.
constexpr uint32_t kClockIsSet = 1700000000u;
// In the file in place of a time: the bell has rung and the app has not been told.
constexpr uint32_t kRang = 1;

bool loaded = false;
uint32_t due = 0;
uint32_t lastPollMs = 0;

uint32_t epochNow() {
  const time_t now = time(nullptr);
  return now >= static_cast<time_t>(kClockIsSet) ? static_cast<uint32_t>(now) : 0;
}

void store(const uint32_t value) {
  due = value;
  loaded = true;
  if (value == 0) {
    if (Storage.exists(kPath)) Storage.remove(kPath);
    return;
  }
  HalFile file;
  if (!Storage.openFileForWrite("POMO", kPath, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
  file.flush();
}

void load() {
  if (loaded) return;
  loaded = true;
  due = 0;
  if (!Storage.exists(kPath)) return;
  HalFile file;
  uint32_t value = 0;
  if (Storage.openFileForRead("POMO", kPath, file) &&
      file.read(reinterpret_cast<uint8_t*>(&value), sizeof(value)) == static_cast<int>(sizeof(value))) {
    due = value;
  }
}

}  // namespace

void chime() {
#if POMODORO_BELL_HAS_BUZZER
  Buzzer buzzer;
  if (!buzzer.begin()) return;
  // tone() blocks, which is fine here: the whole thing is under a second and
  // a half and there is nothing else to do at the bell.
  const uint32_t notes[] = {1760, 2093, 2637};
  for (int round = 0; round < 2; ++round) {
    for (const uint32_t f : notes) {
      buzzer.tone(f, 120);
      delay(60);
    }
    delay(250);
  }
  buzzer.end();
#endif
}

void set(const uint32_t dueEpoch) { store(dueEpoch > kRang ? dueEpoch : 0); }

void clear() { store(0); }

void cancelPending() {
  load();
  if (due > kRang) store(0);
}

uint32_t secondsUntil() {
  load();
  if (due <= kRang) return 0;
  const uint32_t now = epochNow();
  if (now == 0) return 0;
  return due > now ? due - now : 1;
}

void poll() {
  const uint32_t nowMs = static_cast<uint32_t>(millis());
  if (nowMs - lastPollMs < 1000u) return;
  lastPollMs = nowMs;
  load();
  if (due <= kRang) return;
  const uint32_t now = epochNow();
  if (now == 0 || now < due) return;
  store(kRang);
  chime();
}

void ringFromSleep() {
  load();
  if (due <= kRang) return;
  store(kRang);
  chime();
}

bool takeRang() {
  load();
  if (due != kRang) return false;
  store(0);
  return true;
}

}  // namespace pomodorobell

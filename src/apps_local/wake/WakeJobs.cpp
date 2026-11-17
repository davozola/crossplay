#include "WakeJobs.h"

#include <HalStorage.h>
#include <Logging.h>

#include "../../CrossPointSettings.h"
#include "../casa/CasaStore.h"
#include "WakePlan.h"

#if !defined(SIMULATOR)
#include <esp_attr.h>
#endif

namespace wakejobs {

namespace {

constexpr char kFramePath[] = "/.crosspoint/frame.cfg";
constexpr char kPinnedSleep[] = "/sleep.bmp";
constexpr char kFrameCountPath[] = "/.crosspoint/frame.count";

constexpr uint32_t kFrameSeconds[kFrameChoices] = {0, 900, 1800, 3600, 10800, 43200, 86400};
constexpr const char* kFrameLabels[kFrameChoices] = {"OFF", "15M", "30M", "1H", "3H", "12H", "24H"};

// Survives deep sleep, not a power cut; wakeplan checks its magic.
#if defined(SIMULATOR)
wakeplan::State state;
#else
RTC_NOINIT_ATTR wakeplan::State state;
#endif

// The frame only has something to do when the sleep screen draws from a set:
// one pinned picture, or any other kind of sleep screen, would be repainted
// the same.
uint32_t frameEveryNow() {
  const uint32_t every = frameSeconds(frameChoice());
  if (every == 0) return 0;
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM) return 0;
  if (Storage.exists(kPinnedSleep)) return 0;
  return every;
}

}  // namespace

uint32_t armSeconds(const uint32_t liveSeconds, const bool attended) {
  const bool casaOn = casastore::sleepLogging();
  if (attended && casaOn) casastore::note();
  const uint32_t seconds =
      wakeplan::arm(state, frameEveryNow(), casaOn ? casastore::kSleepEverySeconds : 0, liveSeconds, attended);
  LOG_DBG("WAKE", "armed for %u s (live %u)", static_cast<unsigned>(seconds), static_cast<unsigned>(liveSeconds));
  return seconds;
}

uint32_t rearmSeconds(const uint32_t liveSeconds) { return wakeplan::rearm(state, liveSeconds); }

bool onTimerWake() {
  const wakeplan::Due due = wakeplan::onTimer(state);
  if (due.casa) casastore::note();
  if (due.frame) setFrameChanges(frameChanges() + 1);
  return due.frame;
}

uint32_t frameChanges() {
  if (!Storage.exists(kFrameCountPath)) return 0;
  HalFile file;
  uint32_t count = 0;
  if (!Storage.openFileForRead("WAKE", kFrameCountPath, file) ||
      file.read(reinterpret_cast<uint8_t*>(&count), sizeof(count)) != static_cast<int>(sizeof(count))) {
    return 0;
  }
  return count;
}

void setFrameChanges(const uint32_t count) {
  HalFile file;
  if (!Storage.openFileForWrite("WAKE", kFrameCountPath, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(&count), sizeof(count));
  file.flush();
}

int frameChoice() {
  if (!Storage.exists(kFramePath)) return 0;
  HalFile file;
  uint8_t value = 0;
  if (!Storage.openFileForRead("WAKE", kFramePath, file) || file.read(&value, 1) != 1) return 0;
  return value < kFrameChoices ? value : 0;
}

void setFrameChoice(const int choice) {
  HalFile file;
  if (!Storage.openFileForWrite("WAKE", kFramePath, file)) return;
  const uint8_t value = static_cast<uint8_t>(choice >= 0 && choice < kFrameChoices ? choice : 0);
  file.write(&value, 1);
  file.flush();
  // A new interval counts its own pictures.
  setFrameChanges(0);
}

const char* frameLabel(const int choice) { return kFrameLabels[choice >= 0 && choice < kFrameChoices ? choice : 0]; }

uint32_t frameSeconds(const int choice) { return kFrameSeconds[choice >= 0 && choice < kFrameChoices ? choice : 0]; }

}  // namespace wakejobs

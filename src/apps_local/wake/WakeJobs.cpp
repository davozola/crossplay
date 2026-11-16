#include "WakeJobs.h"

#include <HalStorage.h>
#include <Logging.h>

#include <ctime>

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
  const uint32_t seconds = wakeplan::arm(state, frameEveryNow(), casaOn ? casastore::kSleepEverySeconds : 0,
                                         static_cast<int64_t>(time(nullptr)), liveSeconds);
  LOG_DBG("WAKE", "armed for %u s (live %u)", static_cast<unsigned>(seconds), static_cast<unsigned>(liveSeconds));
  return seconds;
}

uint32_t rearmSeconds(const uint32_t liveSeconds) {
  return wakeplan::rearm(state, static_cast<int64_t>(time(nullptr)), liveSeconds);
}

bool onTimerWake() {
  const int64_t now = static_cast<int64_t>(time(nullptr));
  const wakeplan::Due due = wakeplan::onTimer(state, now);
  if (due.casa) casastore::note();
  // A job just done is unscheduled; give it its next time from the intervals
  // the last sleep was armed with, since this wake may go straight back down
  // without passing armSeconds.
  if (state.magic == wakeplan::kMagic) wakeplan::arm(state, state.frameEvery, state.casaEvery, now, 0);
  return due.frame;
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
}

const char* frameLabel(const int choice) { return kFrameLabels[choice >= 0 && choice < kFrameChoices ? choice : 0]; }

uint32_t frameSeconds(const int choice) { return kFrameSeconds[choice >= 0 && choice < kFrameChoices ? choice : 0]; }

}  // namespace wakejobs

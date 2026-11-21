#include "VozAudio.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>

#include <atomic>
#include <cstdlib>

#include "VozCore.h"

#if !defined(SIMULATOR)
#include <Microphone.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace voz::audio {

namespace {

int16_t* pcm = nullptr;
std::atomic<size_t> taken{0};
std::atomic<bool> stopAsked{false};
std::atomic<bool> done{true};

#if !defined(SIMULATOR)
freeink::Microphone mic;
TaskHandle_t task = nullptr;

void run(void*) {
  // The first few milliseconds after the clock starts are the mic settling:
  // a thump that would sit at the front of every note.
  int16_t settle[256];
  for (int i = 0; i < 4; ++i) mic.read(settle, 256, 20);
  while (!stopAsked && taken < kMaxSamples) {
    size_t want = kMaxSamples - taken;
    if (want > 512) want = 512;
    const int got = mic.read(pcm + taken, want, 60);
    if (got < 0) {
      LOG_ERR("VOZ", "microphone read failed");
      break;
    }
    taken += static_cast<size_t>(got);
  }
  mic.end();
  done = true;
  task = nullptr;
  vTaskDelete(nullptr);
}
#else
uint32_t startedMs = 0;
#endif

}  // namespace

void* acquire(const size_t bytes) {
#if !defined(SIMULATOR)
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = malloc(bytes);
  return p;
#else
  return malloc(bytes);
#endif
}

void release(void* p) { free(p); }

bool start() {
  discard();
  pcm = static_cast<int16_t*>(acquire(kMaxSamples * sizeof(int16_t)));
  if (!pcm) {
    LOG_ERR("VOZ", "OOM: %u bytes for a take", static_cast<unsigned>(kMaxSamples * sizeof(int16_t)));
    return false;
  }
  taken = 0;
  stopAsked = false;
#if !defined(SIMULATOR)
  if (!mic.begin(kSampleRate)) {
    LOG_ERR("VOZ", "no microphone on this device");
    release(pcm);
    pcm = nullptr;
    return false;
  }
  done = false;
  // Above the UI's priority, on the core the radio is not on.
  if (xTaskCreatePinnedToCore(&run, "voz_mic", 4096, nullptr, 5, &task, 1) != pdPASS) {
    LOG_ERR("VOZ", "no task for the microphone");
    mic.end();
    done = true;
    release(pcm);
    pcm = nullptr;
    return false;
  }
#else
  // The simulator has no microphone: a take of silence, so the screens run.
  done = false;
  startedMs = static_cast<uint32_t>(millis());
#endif
  return true;
}

void stop() { stopAsked = true; }

bool finished() {
#if defined(SIMULATOR)
  if (!done) {
    size_t n = static_cast<size_t>(static_cast<uint32_t>(millis()) - startedMs) * (kSampleRate / 1000);
    if (n > kMaxSamples) n = kMaxSamples;
    for (size_t i = taken; i < n; ++i) pcm[i] = 0;
    taken = n;
    if (stopAsked || n >= kMaxSamples) done = true;
  }
#endif
  return done;
}

size_t samples() {
  finished();
  return taken;
}

bool save(const char* path) {
  if (!pcm || !done) return false;
  const size_t count = taken;
  normalise(pcm, count);
  uint8_t header[kWavHeaderBytes];
  wavHeader(header, static_cast<uint32_t>(count * sizeof(int16_t)));
  bool ok = false;
  {
    HalFile file;
    if (Storage.openFileForWrite("VOZ", path, file)) {
      ok = file.write(header, sizeof(header)) == sizeof(header);
      // In pieces: one write of two megabytes holds the card's lock for seconds.
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(pcm);
      const size_t total = count * sizeof(int16_t);
      for (size_t at = 0; ok && at < total; at += 16384) {
        const size_t n = total - at < 16384 ? total - at : 16384;
        ok = file.write(bytes + at, n) == n;
      }
      file.flush();
    }
  }
  if (!ok) LOG_ERR("VOZ", "could not write %s", path);
  release(pcm);
  pcm = nullptr;
  taken = 0;
  return ok;
}

void discard() {
#if !defined(SIMULATOR)
  if (!done) {
    stopAsked = true;
    for (int i = 0; i < 100 && !done; ++i) delay(10);
  }
#endif
  done = true;
  if (pcm) release(pcm);
  pcm = nullptr;
  taken = 0;
}

}  // namespace voz::audio

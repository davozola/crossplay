#include "VozAudio.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>

#include "VozCore.h"

#if !defined(SIMULATOR)
#include <BoardConfig.h>
#include <driver/gpio.h>
#include <driver/i2s_pdm.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/soc_caps.h>
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include <soc/usb_serial_jtag_struct.h>
#endif
#endif

namespace voz::audio {

namespace {

int16_t* pcm = nullptr;
std::atomic<size_t> taken{0};
std::atomic<bool> stopAsked{false};
std::atomic<bool> done{true};

std::atomic<int> heard{0};
Level taken_level;
int setup = -1;  // -1 until asked for: the board's own

#if !defined(SIMULATOR)
// The PDM microphone, opened here rather than through the SDK's Microphone so
// the setup can be chosen: the SDK fixes all three of MicSetup's choices from
// notes that were never checked on a Sticky.
i2s_chan_handle_t rx = nullptr;
TaskHandle_t task = nullptr;

void micPower(const bool on, const bool enableHigh) {
  const int8_t pin = BoardConfig::ACTIVE.mic.enable;
  if (pin == BoardConfig::PIN_UNASSIGNED) return;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, on == enableHigh ? HIGH : LOW);
}

void micClose(const bool enableHigh) {
  if (rx) {
    i2s_channel_disable(rx);
    i2s_del_channel(rx);
    rx = nullptr;
  }
  micPower(false, enableHigh);
}

bool micOpen(const MicSetup& with) {
  const BoardConfig::MicConfig& mic = BoardConfig::ACTIVE.mic;
  if (mic.input != BoardConfig::MicInput::Pdm || mic.clk == BoardConfig::PIN_UNASSIGNED ||
      mic.data == BoardConfig::PIN_UNASSIGNED) {
    return false;
  }
#if SOC_USB_SERIAL_JTAG_SUPPORTED
  // GPIO19 and 20 are the chip's own USB pads, and while the USB-Serial/JTAG
  // PHY holds them nothing else reads them. This board's USB port goes through
  // a bridge on the UART, so nothing is lost by letting go.
  if (mic.clk == 19 || mic.clk == 20 || mic.data == 19 || mic.data == 20) {
    USB_SERIAL_JTAG.conf0.usb_pad_enable = 0;
    USB_SERIAL_JTAG.conf0.dp_pullup = 0;
  }
#endif
  micPower(true, with.enableHigh);
  delay(50);  // the rail, then the microphone waking

  i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  if (i2s_new_channel(&channel, nullptr, &rx) != ESP_OK) {
    rx = nullptr;
    micPower(false, with.enableHigh);
    return false;
  }
  i2s_pdm_rx_config_t pdm = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(kSampleRate),
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .clk = static_cast<gpio_num_t>(mic.clk),
              .din = static_cast<gpio_num_t>(mic.data),
              .invert_flags = {.clk_inv = false},
          },
  };
  // 64 clocks a sample is 1.024 MHz at 16 kHz; 128 is 2.048 MHz.
  pdm.clk_cfg.dn_sample_mode = with.fastClock ? I2S_PDM_DSR_16S : I2S_PDM_DSR_8S;
  pdm.slot_cfg.slot_mask = with.rightSlot ? I2S_PDM_SLOT_RIGHT : I2S_PDM_SLOT_LEFT;
  if (i2s_channel_init_pdm_rx_mode(rx, &pdm) != ESP_OK || i2s_channel_enable(rx) != ESP_OK) {
    i2s_del_channel(rx);
    rx = nullptr;
    micPower(false, with.enableHigh);
    return false;
  }
  return true;
}

// Samples read, 0 on a timeout, below zero on an error.
int micRead(int16_t* into, const size_t maxSamples, const uint32_t timeoutMs) {
  size_t bytes = 0;
  const esp_err_t err = i2s_channel_read(rx, into, maxSamples * sizeof(int16_t), &bytes, pdMS_TO_TICKS(timeoutMs));
  if (err == ESP_ERR_TIMEOUT) return 0;
  if (err != ESP_OK) return -1;
  return static_cast<int>(bytes / sizeof(int16_t));
}

void run(void*) {
  // The first few milliseconds after the clock starts are the mic settling:
  // a thump that would sit at the front of every note.
  int16_t settle[256];
  for (int i = 0; i < 4; ++i) micRead(settle, 256, 20);
  while (!stopAsked && taken < kMaxSamples) {
    size_t want = kMaxSamples - taken;
    if (want > 512) want = 512;
    const int got = micRead(pcm + taken, want, 60);
    if (got < 0) {
      LOG_ERR("VOZ", "microphone read failed");
      break;
    }
    if (got > 0) {
      // The loudest block since the screen last asked: it redraws once a
      // second, and the block in hand at that instant may fall between words.
      const int peak = levelOf(pcm + taken, static_cast<size_t>(got)).peak;
      if (peak > heard) heard = peak;
    }
    taken += static_cast<size_t>(got);
  }
  micClose(micSetup(setupInUse()).enableHigh);
  heard = 0;
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
  if (!micOpen(micSetup(setupInUse()))) {
    LOG_ERR("VOZ", "no microphone on this device");
    release(pcm);
    pcm = nullptr;
    return false;
  }
  done = false;
  // Above the UI's priority, on the core the radio is not on.
  if (xTaskCreatePinnedToCore(&run, "voz_mic", 4096, nullptr, 5, &task, 1) != pdPASS) {
    LOG_ERR("VOZ", "no task for the microphone");
    micClose(micSetup(setupInUse()).enableHigh);
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
  taken_level = levelOf(pcm, count);
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

int level() { return heard.exchange(0); }

Level lastTake() { return taken_level; }

void useSetup(const int index) { setup = index >= 0 && index < kMicSetups ? index : -1; }

int setupInUse() { return setup >= 0 ? setup : boardSetup(); }

int boardSetup() {
#if !defined(SIMULATOR)
  MicSetup notes;
  notes.enableHigh = BoardConfig::ACTIVE.mic.enableActiveHigh;
  return micIndex(notes);
#else
  return 0;
#endif
}

bool probe(Level levels[kMicSetups], bool opened[kMicSetups]) {
  for (int i = 0; i < kMicSetups; ++i) {
    levels[i] = Level{};
    opened[i] = false;
  }
#if !defined(SIMULATOR)
  if (!done || BoardConfig::ACTIVE.mic.input != BoardConfig::MicInput::Pdm) return false;
  constexpr size_t kListen = kSampleRate / 2;  // half a second each
  int16_t* buffer = static_cast<int16_t*>(acquire(kListen * sizeof(int16_t)));
  if (!buffer) {
    LOG_ERR("VOZ", "OOM: %u bytes for the microphone test", static_cast<unsigned>(kListen * sizeof(int16_t)));
    return false;
  }
  for (int i = 0; i < kMicSetups; ++i) {
    const MicSetup with = micSetup(i);
    if (!micOpen(with)) continue;
    opened[i] = true;
    // A fifth of a second thrown away: the microphone waking and the filter
    // settling would otherwise read as the loudest thing it heard.
    size_t skipped = 0;
    for (int tries = 0; skipped < kSampleRate / 5 && tries < 40; ++tries) {
      const int got = micRead(buffer, 512, 60);
      if (got < 0) break;
      skipped += static_cast<size_t>(got);
    }
    size_t have = 0;
    for (int tries = 0; have < kListen && tries < 80; ++tries) {
      const int got = micRead(buffer + have, std::min<size_t>(512, kListen - have), 60);
      if (got < 0) break;
      have += static_cast<size_t>(got);
    }
    levels[i] = levelOf(buffer, have);
    LOG_INF("VOZ", "mic setup %d: peak %d rms %d", i, levels[i].peak, levels[i].rms);
    micClose(with.enableHigh);
    delay(60);  // the rail falls before the next setup raises it its own way
  }
  release(buffer);
  return true;
#else
  return false;
#endif
}

}  // namespace voz::audio

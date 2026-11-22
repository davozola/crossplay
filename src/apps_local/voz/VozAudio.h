#pragma once

// VOZ's recorder: the Sticky's PDM microphone, read on a task of its own into
// one buffer in PSRAM, so a slow paint or a card write never drops a moment of
// the voice. One take at a time.

#include <cstddef>
#include <cstdint>

#include "VozCore.h"

namespace voz::audio {

// Large buffers, in PSRAM where there is any. Free with release().
void* acquire(size_t bytes);
void release(void* p);

// Starts the microphone and the task. False when this device has no
// microphone, or there is no room for a minute of sound.
bool start();
// Asks the take to end; finished() turns true once the task has stopped.
void stop();
bool finished();
// Samples so far; the clock on the screen is this over the rate.
size_t samples();
// The take, raised to a readable level, written as a WAV file at `path`.
// Frees the buffer whether it was written or not.
bool save(const char* path);
// Ends any take and frees everything. Safe to call twice.
void discard();

// The loudest the microphone has heard since this was last asked: the bar on
// the recording screen. Zero when nothing is being recorded.
int level();
// How loud the take save() last wrote was, measured before it was raised.
Level lastTake();

// The microphone setup takes are recorded with (voz::micSetup's index), and
// the one this board's notes give.
void useSetup(int index);
int setupInUse();
int boardSetup();
// Listens for about half a second through every setup and reports what each
// heard. Blocks for some seconds; not while a take is running. False when this
// device has no microphone.
bool probe(Level levels[kMicSetups], bool opened[kMicSetups]);

// What the microphone's pins do when they are driven by hand, with no I2S in
// the way: the evidence for a test in which no setup hears anything.
struct MicFacts {
  bool ran = false;
  // The enable pin read back while driven high, then low: a pad that does not
  // follow is held by something else.
  int enableHigh = -1;
  int enableLow = -1;
  uint32_t usbConf = 0;  // USB_SERIAL_JTAG.conf0, which owns the pads at reset
  // Four runs: enable high or low, by clock on the first pin or on the second.
  // In each, how often in a hundred the data pin read 1 on the clock's high
  // half and on its low half, with the pin pulled down and then pulled up. A
  // microphone that is talking reads near fifty on one half whatever the pull;
  // a line nobody drives reads 0 pulled down and 100 pulled up.
  struct Run {
    int downHigh = 0;
    int downLow = 0;
    int upHigh = 0;
    int upLow = 0;
  };
  Run runs[4];
  int clk = -1;
  int data = -1;
};
void diagnose(MicFacts& facts);

}  // namespace voz::audio

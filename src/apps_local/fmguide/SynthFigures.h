#pragma once

// Teaching figures for synthesis: the signal path, waveforms, a wavetable,
// filters, envelopes, LFOs, modulation, the arp, the sequencer. Drawn in black
// on paper from lines, boxes and capitals, so they read on e-ink and on any
// firmware's guide: nothing here knows which firmware it explains.
//
// They draw on a Canvas, not on the renderer, so the same figure is painted
// on the device and rendered on a laptop to be looked at before it ships
// (host-tests/fmguide/figures.sh).

#include <cstdint>

namespace synthfig {

enum Fig : uint8_t {
  SignalFlow,   // SHRUB's voice: oscillators, noise and sub into one filter, then ENV 1 and the effects
  Waveforms,    // sine, triangle, saw, square, pulse
  Wavetable,    // frames in a row, and WT POS choosing one
  Harmonics,    // a saw's and a square's harmonics
  Unison,       // VOICES, DETUNE, BLEND and WIDTH
  Warp,         // one cycle bent four ways
  FilterTypes,  // LP, HP, BP, NOTCH
  CutoffRes,    // a low-pass with and without resonance
  Slopes,       // 6, 12 and 24 dB per octave
  Adsr,         // attack, decay, sustain, release against the key
  LfoShapes,    // SKEW's three shapes and sample and hold
  Bipolar,      // unipolar and bipolar modulation around a knob
  Matrix,       // SOURCE, AMOUNT, DEST and the AUX that scales it
  Glide,        // notes stepping, and sliding
  Arp,          // a held chord played one note at a time
  StepSeq,      // notes put in step by step, with their gate
  Automation,   // a knob's movement recorded in a lane
  Polyrhythm,   // lanes of 3 and 4 bars meeting at 12
  FxChain,      // seven slots in order
  VoicePool,    // notes and unison sharing the voices
  Kick,         // a drum from a falling pitch and a fast decay
  kFigureCount,
};
constexpr int kFigures = static_cast<int>(kFigureCount);

enum class Align : uint8_t { Left, Centre, Right };

// What a figure draws with. Coordinates in pixels; text is ASCII capitals in
// a small pixel face, placed by the vertical centre of its capitals.
class Canvas {
 public:
  virtual ~Canvas() = default;
  virtual void line(int x0, int y0, int x1, int y1, int width) = 0;
  virtual void fill(int x, int y, int w, int h, bool ink) = 0;
  virtual void gray(int x, int y, int w, int h) = 0;
  virtual void frame(int x, int y, int w, int h, int width) = 0;
  virtual void text(int x, int cy, const char* text, Align align, bool ink) = 0;
  virtual int textWidth(const char* text) = 0;
};

// How tall a figure is drawn in a column `width` wide.
int height(int figure, int width);
void draw(Canvas& canvas, int figure, int x, int y, int width);

// Its name, for the tests and the preview sheet.
const char* name(int figure);

}  // namespace synthfig

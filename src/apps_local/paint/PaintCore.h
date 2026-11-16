#pragma once

// PINTA's model: a one-bit sheet a small child paints on with a finger. A
// round brush, a rubber that is the same brush in white, and pictures stamped
// on whole. No strokes are kept and nothing is undone: the sheet is its pixels.
// Freestanding.

#include <cstdint>

namespace paint {

class Sheet {
 public:
  // `bits` is rowBytes() * height bytes, row-major, MSB first, 1 = ink. The
  // sheet does not own it.
  Sheet(uint8_t* bits, int width, int height) : bits_(bits), w_(width), h_(height) {}

  int width() const { return w_; }
  int height() const { return h_; }
  int rowBytes() const { return (w_ + 7) / 8; }
  const uint8_t* bits() const { return bits_; }

  void clear();
  bool empty() const;
  bool at(int x, int y) const;
  // A filled disc of `radius` at (x, y), ink or paper; clipped to the sheet.
  void dot(int x, int y, int radius, bool ink);
  // A stroke of that brush between two points.
  void line(int x0, int y0, int x1, int y1, int radius, bool ink);
  // A picture centred on (cx, cy), every source pixel `scale` wide. `src` is
  // `sw` x `sh`, rows of (sw + 7) / 8 bytes, MSB first, where a CLEAR bit is
  // ink: the layout the picture tables use. Only its ink is laid down.
  void stamp(const uint8_t* src, int sw, int sh, int cx, int cy, int scale);

 private:
  void span(int x0, int x1, int y, bool ink);

  uint8_t* bits_;
  int w_;
  int h_;
};

// The brushes, by the radius they paint with.
constexpr int kThinRadius = 7;
constexpr int kFatRadius = 17;
constexpr int kRubberRadius = 30;

}  // namespace paint

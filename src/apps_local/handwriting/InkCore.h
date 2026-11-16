#pragma once

// HANDWRITING: pages you write and draw on with a finger. Freestanding: the
// page model, undo, the rasteriser that turns strokes into a 1-bit image, and
// the BMP the image is exported as. host-tests/handwriting checks all four.
//
// A page is kept as its strokes, not its pixels. That makes undo exact (take
// the last stroke away), keeps a page to a few kilobytes on the card, and lets
// the eraser be a stroke too: drawn in white, in order, it rubs out exactly
// what was under it when it was drawn and nothing drawn after.

#include <cstdint>

namespace ink {

enum class Tool : uint8_t { Pen = 0, Eraser = 1 };

constexpr int kPenWidth = 4;
constexpr int kEraserWidth = 26;
inline int widthOf(const Tool tool) { return tool == Tool::Pen ? kPenWidth : kEraserWidth; }

// Points of every stroke on a page, end to end. A stroke is a header entry
// (x = kStrokeMark, y = tool) followed by its points.
constexpr int16_t kStrokeMark = -1;
constexpr int kMaxPoints = 12000;

struct Point {
  int16_t x;
  int16_t y;
};

class Page {
 public:
  int16_t width = 480;
  int16_t height = 640;

  void clear() { count_ = 0; }
  int count() const { return count_; }
  const Point* points() const { return points_; }
  bool empty() const { return count_ == 0; }
  int strokes() const;

  // False when the page is full; the caller says so instead of drawing.
  bool begin(Tool tool, int16_t x, int16_t y);
  // Extends the stroke begun last. Points closer than `minStep` to the last
  // one are dropped: the finger jitters, and a page is a fixed budget.
  bool extend(int16_t x, int16_t y, int minStep = 2);
  // Takes the last stroke away. False when there is none.
  bool undo();

  // For loading a saved page.
  bool assign(const Point* points, int count);

 private:
  Point points_[kMaxPoints];
  int count_ = 0;
  int lastStroke_ = -1;
};

// Walks the page stroke by stroke. `segment` is called for each line piece and
// for each lone point (as a zero-length piece), with the stroke's tool.
template <typename F>
void forEachSegment(const Page& page, F&& segment) {
  const Point* p = page.points();
  Tool tool = Tool::Pen;
  for (int i = 0; i < page.count(); ++i) {
    if (p[i].x == kStrokeMark) {
      tool = static_cast<Tool>(p[i].y);
      if (i + 1 < page.count() && p[i + 1].x != kStrokeMark && (i + 2 >= page.count() || p[i + 2].x == kStrokeMark)) {
        segment(tool, p[i + 1], p[i + 1]);
      }
      continue;
    }
    if (i + 1 < page.count() && p[i + 1].x != kStrokeMark) segment(tool, p[i], p[i + 1]);
  }
}

// Draws the page into a 1-bit buffer, row-major, MSB first, 1 = ink.
// `rowBytes` is (width + 7) / 8; the buffer is cleared first.
void rasterise(const Page& page, uint8_t* bits, int rowBytes);

// A 1-bit BMP (white background, black ink) of `bits`: the 62-byte header the
// file starts with, for a `width` x `height` image. Rows follow bottom-up,
// each padded to four bytes -- `bmpRow` produces one.
constexpr int kBmpHeaderSize = 62;
int bmpRowBytes(int width);
void bmpHeader(int width, int height, uint8_t* out);
// Writes row `y` of the image (top-down numbering) in BMP order into `out`.
void bmpRow(const uint8_t* bits, int rowBytes, int width, int y, uint8_t* out);

}  // namespace ink

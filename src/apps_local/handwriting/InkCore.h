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

// The id is what a stroke's header stores, so the two original tools keep
// theirs and pages written before the other nibs existed still read.
enum class Tool : uint8_t { Pen = 0, Eraser = 1, Fine = 2, Marker = 3, Pencil = 4 };
constexpr int kToolCount = 5;

constexpr int kFineWidth = 2;
constexpr int kPenWidth = 4;
constexpr int kMarkerWidth = 10;
constexpr int kPencilWidth = 8;
constexpr int kEraserWidth = 26;
inline int widthOf(const Tool tool) {
  switch (tool) {
    case Tool::Fine:
      return kFineWidth;
    case Tool::Marker:
      return kMarkerWidth;
    case Tool::Pencil:
      return kPencilWidth;
    case Tool::Eraser:
      return kEraserWidth;
    case Tool::Pen:
    default:
      return kPenWidth;
  }
}
inline bool isNib(const Tool tool) { return tool != Tool::Eraser; }
const char* toolName(Tool tool);

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
// Adds to a buffer that already holds the page's first `from` points only
// what came after them, up to `upTo`. The pencil is a half-tone and the eraser
// clears, both exactly as rasterise() would have left them, so drawing a page
// in steps and drawing it at once give the same bits.
void rasteriseMore(const Page& page, uint8_t* bits, int rowBytes, int from, int upTo);
// A `tw` x `th` thumbnail of a rasterised page: a thumbnail pixel is ink when
// any pixel under it is, except that a half-inked block keeps its half-tone. Same bit layout, rows of (tw + 7) / 8
// bytes.
void thumbnail(const uint8_t* bits, int rowBytes, int width, int height, uint8_t* out, int tw, int th);

// A 1-bit BMP (white background, black ink) of `bits`: the 62-byte header the
// file starts with, for a `width` x `height` image. Rows follow bottom-up,
// each padded to four bytes -- `bmpRow` produces one.
constexpr int kBmpHeaderSize = 62;
int bmpRowBytes(int width);
void bmpHeader(int width, int height, uint8_t* out);
// Writes row `y` of the image (top-down numbering) in BMP order into `out`.
void bmpRow(const uint8_t* bits, int rowBytes, int width, int y, uint8_t* out);

}  // namespace ink

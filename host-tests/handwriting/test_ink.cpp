#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "InkCore.h"

using namespace ink;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                 \
    }                                                             \
  } while (0)

static Page page;
static uint8_t bits[640 * 60];

static bool inkAt(const int x, const int y) { return (bits[y * 60 + x / 8] >> (7 - x % 8)) & 1; }

static void strokesAndUndo() {
  page.clear();
  CHECK(page.empty());
  CHECK(page.begin(Tool::Pen, 10, 10));
  CHECK(page.extend(11, 10));  // too close, dropped
  CHECK(page.count() == 2);
  CHECK(page.extend(100, 10));
  CHECK(page.count() == 3);
  CHECK(page.begin(Tool::Pen, 50, 50));
  CHECK(page.extend(50, 150));
  CHECK(page.strokes() == 2);
  CHECK(page.undo());
  CHECK(page.strokes() == 1);
  CHECK(page.count() == 3);
  // A stroke that was undone cannot be extended.
  CHECK(!page.extend(60, 60));
  CHECK(page.undo());
  CHECK(page.empty());
  CHECK(!page.undo());
}

static void rasterAndEraser() {
  page.clear();
  page.begin(Tool::Pen, 20, 100);
  page.extend(200, 100);
  rasterise(page, bits, 60);
  CHECK(inkAt(20, 100) && inkAt(110, 100) && inkAt(200, 100));
  CHECK(inkAt(110, 101) && !inkAt(110, 110));
  CHECK(!inkAt(300, 100));
  // The eraser rubs out what is under it...
  page.begin(Tool::Eraser, 110, 80);
  page.extend(110, 120);
  // ...and a pen stroke drawn after it is not rubbed out.
  page.begin(Tool::Pen, 100, 300);
  page.extend(120, 300);
  rasterise(page, bits, 60);
  CHECK(!inkAt(110, 100));
  CHECK(inkAt(20, 100));
  CHECK(inkAt(110, 300));
  // A lone tap leaves a dot.
  page.begin(Tool::Pen, 300, 400);
  rasterise(page, bits, 60);
  CHECK(inkAt(300, 400));
  // Strokes off the edge are clipped, not wrapped.
  page.begin(Tool::Pen, 470, 10);
  page.extend(479, 10);
  rasterise(page, bits, 60);
  CHECK(inkAt(479, 10));
  CHECK(!inkAt(0, 11));
}

static void bmp() {
  uint8_t header[kBmpHeaderSize];
  bmpHeader(480, 640, header);
  CHECK(header[0] == 'B' && header[1] == 'M');
  CHECK(header[28] == 1);
  CHECK(bmpRowBytes(480) == 60 && bmpRowBytes(481) == 64);
  const uint32_t size = header[2] | (header[3] << 8) | (header[4] << 16) | (header[5] << 24);
  CHECK(size == 62u + 60u * 640u);
  uint8_t row[64];
  page.clear();
  page.begin(Tool::Pen, 0, 0);
  rasterise(page, bits, 60);
  bmpRow(bits, 60, 480, 0, row);
  CHECK(row[0] & 0x80);
}

static void assignGuards() {
  Point bad[2] = {{5, 5}, {6, 6}};
  CHECK(!page.assign(bad, 2));
  Point good[3] = {{kStrokeMark, 0}, {5, 5}, {9, 9}};
  CHECK(page.assign(good, 3));
  CHECK(page.strokes() == 1);
}

int main() {
  strokesAndUndo();
  rasterAndEraser();
  bmp();
  assignGuards();
  if (failures) {
    std::printf("%d handwriting check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("handwriting: all checks passed\n");
  return EXIT_SUCCESS;
}

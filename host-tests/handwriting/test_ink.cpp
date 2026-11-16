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

static uint8_t stepped[640 * 60];

// Drawing a page a few points at a time must leave the same bits as drawing it
// at once: the device only ever adds the new piece of a stroke to its buffer.
static void incrementalMatchesFull() {
  page.clear();
  const Tool tools[5] = {Tool::Pen, Tool::Marker, Tool::Pencil, Tool::Eraser, Tool::Fine};
  for (int s = 0; s < 5; ++s) {
    page.begin(tools[s], static_cast<int16_t>(40 + s * 30), static_cast<int16_t>(100 + s * 50));
    for (int k = 1; k < 12; ++k) {
      page.extend(static_cast<int16_t>(40 + s * 30 + k * 25), static_cast<int16_t>(100 + s * 50 + (k % 3) * 40));
    }
  }
  page.begin(Tool::Pen, 300, 600);  // a lone tap
  rasterise(page, bits, 60);
  std::memset(stepped, 0, sizeof(stepped));
  int done = 0;
  for (int upTo = 3; done < page.count(); upTo += 5) {
    const int to = upTo < page.count() ? upTo : page.count();
    rasteriseMore(page, stepped, 60, done, to);
    done = to;
  }
  CHECK(std::memcmp(bits, stepped, sizeof(stepped)) == 0);
  CHECK(inkAt(300, 600));
}

static void nibs() {
  page.clear();
  page.begin(Tool::Fine, 100, 100);
  page.extend(200, 100);
  page.begin(Tool::Marker, 100, 200);
  page.extend(200, 200);
  page.begin(Tool::Pencil, 100, 300);
  page.extend(200, 300);
  rasterise(page, bits, 60);
  // Fine is two pixels tall, the marker about ten.
  CHECK(inkAt(150, 100) && inkAt(150, 101) && !inkAt(150, 99) && !inkAt(150, 102));
  CHECK(inkAt(150, 196) && inkAt(150, 204) && !inkAt(150, 206));
  // The pencil is a half-tone: every other pixel along its middle row.
  int on = 0;
  for (int x = 120; x < 180; ++x) on += inkAt(x, 300) ? 1 : 0;
  CHECK(on == 30);
  CHECK(std::strcmp(toolName(Tool::Marker), "MARKER") == 0);
  // An unknown tool id in a file reads as the pen rather than as nothing.
  Point odd[3] = {{kStrokeMark, 77}, {50, 50}, {60, 50}};
  CHECK(page.assign(odd, 3));
  rasterise(page, bits, 60);
  CHECK(inkAt(55, 50));
}

static void thumbnails() {
  page.clear();
  page.begin(Tool::Pen, 240, 320);
  rasterise(page, bits, 60);
  uint8_t thumb[15 * 160];
  thumbnail(bits, 60, 480, 640, thumb, 120, 160);
  int set = 0;
  for (unsigned i = 0; i < sizeof(thumb); ++i) {
    for (int b = 0; b < 8; ++b) set += (thumb[i] >> b) & 1;
  }
  CHECK(set >= 1 && set <= 4);
  CHECK((thumb[80 * 15 + 60 / 8] >> (7 - 60 % 8)) & 1);
}

int main() {
  incrementalMatchesFull();
  nibs();
  thumbnails();
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

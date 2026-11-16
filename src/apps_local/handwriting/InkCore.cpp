#include "InkCore.h"

#include <cstring>

namespace ink {

int Page::strokes() const {
  int n = 0;
  for (int i = 0; i < count_; ++i)
    if (points_[i].x == kStrokeMark) ++n;
  return n;
}

bool Page::begin(const Tool tool, const int16_t x, const int16_t y) {
  if (count_ + 2 > kMaxPoints) return false;
  lastStroke_ = count_;
  points_[count_++] = Point{kStrokeMark, static_cast<int16_t>(tool)};
  points_[count_++] = Point{x, y};
  return true;
}

bool Page::extend(const int16_t x, const int16_t y, const int minStep) {
  if (lastStroke_ < 0 || count_ >= kMaxPoints) return false;
  const Point& last = points_[count_ - 1];
  const int dx = x - last.x;
  const int dy = y - last.y;
  if (dx * dx + dy * dy < minStep * minStep) return true;
  points_[count_++] = Point{x, y};
  return true;
}

bool Page::undo() {
  if (count_ == 0) return false;
  int at = count_ - 1;
  while (at > 0 && points_[at].x != kStrokeMark) --at;
  count_ = at;
  // The stroke before is no longer extendable.
  lastStroke_ = -1;
  return true;
}

bool Page::assign(const Point* points, const int count) {
  if (count < 0 || count > kMaxPoints) return false;
  // A saved page always starts with a stroke mark; anything else is damage.
  if (count > 0 && points[0].x != kStrokeMark) return false;
  std::memcpy(points_, points, sizeof(Point) * static_cast<size_t>(count));
  count_ = count;
  lastStroke_ = -1;
  return true;
}

namespace {

void stamp(uint8_t* bits, const int rowBytes, const int w, const int h, const int cx, const int cy, const int r,
           const bool on) {
  for (int dy = -r; dy <= r; ++dy) {
    const int y = cy + dy;
    if (y < 0 || y >= h) continue;
    for (int dx = -r; dx <= r; ++dx) {
      if (dx * dx + dy * dy > r * r) continue;
      const int x = cx + dx;
      if (x < 0 || x >= w) continue;
      uint8_t& byte = bits[y * rowBytes + x / 8];
      const uint8_t mask = static_cast<uint8_t>(0x80u >> (x % 8));
      if (on) {
        byte |= mask;
      } else {
        byte &= static_cast<uint8_t>(~mask);
      }
    }
  }
}

}  // namespace

void rasterise(const Page& page, uint8_t* bits, const int rowBytes) {
  std::memset(bits, 0, static_cast<size_t>(rowBytes) * static_cast<size_t>(page.height));
  forEachSegment(page, [&](const Tool tool, const Point a, const Point b) {
    const int r = widthOf(tool) / 2;
    const bool on = tool == Tool::Pen;
    int x0 = a.x, y0 = a.y;
    const int x1 = b.x, y1 = b.y;
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      stamp(bits, rowBytes, page.width, page.height, x0, y0, r, on);
      if (x0 == x1 && y0 == y1) break;
      const int e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y0 += sy;
      }
    }
  });
}

int bmpRowBytes(const int width) { return ((width + 31) / 32) * 4; }

namespace {
void le32(uint8_t* p, const uint32_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
  p[2] = static_cast<uint8_t>(v >> 16);
  p[3] = static_cast<uint8_t>(v >> 24);
}
}  // namespace

void bmpHeader(const int width, const int height, uint8_t* out) {
  std::memset(out, 0, kBmpHeaderSize);
  const uint32_t imageSize = static_cast<uint32_t>(bmpRowBytes(width) * height);
  out[0] = 'B';
  out[1] = 'M';
  le32(out + 2, kBmpHeaderSize + imageSize);
  le32(out + 10, kBmpHeaderSize);
  le32(out + 14, 40);
  le32(out + 18, static_cast<uint32_t>(width));
  le32(out + 22, static_cast<uint32_t>(height));
  out[26] = 1;  // planes
  out[28] = 1;  // bits per pixel
  le32(out + 34, imageSize);
  le32(out + 38, 2835);  // 72 dpi
  le32(out + 42, 2835);
  le32(out + 46, 2);  // palette entries
  // Palette: index 0 white, index 1 black, so a set bit is ink.
  out[54] = out[55] = out[56] = 0xFF;
  out[58] = out[59] = out[60] = 0x00;
}

void bmpRow(const uint8_t* bits, const int rowBytes, const int width, const int y, uint8_t* out) {
  const int bmpBytes = bmpRowBytes(width);
  std::memset(out, 0, static_cast<size_t>(bmpBytes));
  std::memcpy(out, bits + y * rowBytes, static_cast<size_t>(rowBytes));
}

}  // namespace ink

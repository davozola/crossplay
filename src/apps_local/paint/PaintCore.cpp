#include "PaintCore.h"

#include <cstring>

namespace paint {

void Sheet::clear() { std::memset(bits_, 0, static_cast<size_t>(rowBytes()) * static_cast<size_t>(h_)); }

bool Sheet::empty() const {
  const int n = rowBytes() * h_;
  for (int i = 0; i < n; ++i) {
    if (bits_[i]) return false;
  }
  return true;
}

bool Sheet::at(const int x, const int y) const {
  if (x < 0 || y < 0 || x >= w_ || y >= h_) return false;
  return (bits_[y * rowBytes() + x / 8] >> (7 - x % 8)) & 1;
}

void Sheet::span(int x0, int x1, const int y, const bool ink) {
  if (y < 0 || y >= h_) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= w_) x1 = w_ - 1;
  uint8_t* row = bits_ + y * rowBytes();
  for (int x = x0; x <= x1; ++x) {
    const uint8_t mask = static_cast<uint8_t>(0x80 >> (x % 8));
    if (ink)
      row[x / 8] |= mask;
    else
      row[x / 8] &= static_cast<uint8_t>(~mask);
  }
}

void Sheet::dot(const int x, const int y, const int radius, const bool ink) {
  for (int dy = -radius; dy <= radius; ++dy) {
    // The widest whole half-chord at this row.
    int half = 0;
    while ((half + 1) * (half + 1) + dy * dy <= radius * radius) ++half;
    span(x - half, x + half, y + dy, ink);
  }
}

void Sheet::line(const int x0, const int y0, const int x1, const int y1, const int radius, const bool ink) {
  const int dx = x1 - x0;
  const int dy = y1 - y0;
  const int adx = dx < 0 ? -dx : dx;
  const int ady = dy < 0 ? -dy : dy;
  // A dot every third of a radius is a stroke with no visible scallops.
  const int stride = radius / 3 < 1 ? 1 : radius / 3;
  const int steps = (adx > ady ? adx : ady) / stride + 1;
  for (int i = 0; i <= steps; ++i) dot(x0 + dx * i / steps, y0 + dy * i / steps, radius, ink);
}

void Sheet::stamp(const uint8_t* src, const int sw, const int sh, const int cx, const int cy, const int scale) {
  if (src == nullptr || scale < 1) return;
  const int srcRow = (sw + 7) / 8;
  const int left = cx - sw * scale / 2;
  const int top = cy - sh * scale / 2;
  for (int sy = 0; sy < sh; ++sy) {
    for (int sx = 0; sx < sw; ++sx) {
      if ((src[sy * srcRow + sx / 8] >> (7 - sx % 8)) & 1) continue;  // a set bit is paper
      for (int k = 0; k < scale; ++k)
        span(left + sx * scale, left + sx * scale + scale - 1, top + sy * scale + k, true);
    }
  }
}

}  // namespace paint

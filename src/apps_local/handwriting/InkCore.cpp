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

// A disc of the tool at (cx, cy). The pencil sets every other pixel of its
// disc, on a lattice fixed to the page so overlapping passes do not fill in.
void stamp(uint8_t* bits, const int rowBytes, const int w, const int h, const int cx, const int cy, const Tool tool) {
  // The fine nib is a 2x2 square; every other tool is a disc of its width.
  const bool fine = tool == Tool::Fine;
  const int r = fine ? 1 : widthOf(tool) / 2;
  const int lo = fine ? 0 : -r;
  for (int dy = lo; dy <= r; ++dy) {
    const int y = cy + dy;
    if (y < 0 || y >= h) continue;
    for (int dx = lo; dx <= r; ++dx) {
      if (!fine && dx * dx + dy * dy > r * r) continue;
      const int x = cx + dx;
      if (x < 0 || x >= w) continue;
      uint8_t& byte = bits[y * rowBytes + x / 8];
      const uint8_t mask = static_cast<uint8_t>(0x80u >> (x % 8));
      if (tool == Tool::Eraser) {
        byte &= static_cast<uint8_t>(~mask);
      } else if (tool != Tool::Pencil || ((x + y) & 1) == 0) {
        byte |= mask;
      }
    }
  }
}

void line(uint8_t* bits, const int rowBytes, const int w, const int h, const Tool tool, const Point a, const Point b) {
  int x0 = a.x, y0 = a.y;
  const int x1 = b.x, y1 = b.y;
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    stamp(bits, rowBytes, w, h, x0, y0, tool);
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
}

Tool toolOf(const int16_t id) { return id >= 0 && id < kToolCount ? static_cast<Tool>(id) : Tool::Pen; }

}  // namespace

const char* toolName(const Tool tool) {
  switch (tool) {
    case Tool::Fine:
      return "FINE";
    case Tool::Marker:
      return "MARKER";
    case Tool::Pencil:
      return "PENCIL";
    case Tool::Eraser:
      return "ERASE";
    case Tool::Pen:
    default:
      return "PEN";
  }
}

void rasteriseMore(const Page& page, uint8_t* bits, const int rowBytes, const int from, int upTo) {
  const Point* p = page.points();
  if (upTo > page.count()) upTo = page.count();
  // The tool in force at `from` is the last stroke mark at or before it.
  Tool tool = Tool::Pen;
  int start = from < 0 ? 0 : from;
  for (int i = start < upTo ? start : upTo - 1; i >= 0; --i) {
    if (p[i].x == kStrokeMark) {
      tool = toolOf(p[i].y);
      break;
    }
  }
  for (int i = start; i < upTo; ++i) {
    if (p[i].x == kStrokeMark) {
      tool = toolOf(p[i].y);
      continue;
    }
    // Each point is drawn with the piece that leads to it; the first of a
    // stroke is a dot, so a tap leaves a mark.
    if (i > 0 && p[i - 1].x != kStrokeMark) {
      line(bits, rowBytes, page.width, page.height, tool, p[i - 1], p[i]);
    } else {
      stamp(bits, rowBytes, page.width, page.height, p[i].x, p[i].y, tool);
    }
  }
}

void rasterise(const Page& page, uint8_t* bits, const int rowBytes) {
  std::memset(bits, 0, static_cast<size_t>(rowBytes) * static_cast<size_t>(page.height));
  rasteriseMore(page, bits, rowBytes, 0, page.count());
}

void thumbnail(const uint8_t* bits, const int rowBytes, const int width, const int height, uint8_t* out, const int tw,
               const int th) {
  const int outRow = (tw + 7) / 8;
  std::memset(out, 0, static_cast<size_t>(outRow) * static_cast<size_t>(th));
  for (int ty = 0; ty < th; ++ty) {
    const int y0 = ty * height / th;
    int y1 = (ty + 1) * height / th;
    if (y1 <= y0) y1 = y0 + 1;
    for (int tx = 0; tx < tw; ++tx) {
      const int x0 = tx * width / tw;
      int x1 = (tx + 1) * width / tw;
      if (x1 <= x0) x1 = x0 + 1;
      int set = 0;
      for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
          if (bits[y * rowBytes + x / 8] & (0x80u >> (x % 8))) ++set;
        }
      }
      // A block that is half ink is the pencil's half-tone, and stays one.
      const int area = (y1 - y0) * (x1 - x0);
      bool on = set > 0;
      if (area >= 6 && set * 2 >= area - 1 && set * 2 <= area + 1) on = ((tx + ty) & 1) == 0;
      if (on) out[ty * outRow + tx / 8] |= static_cast<uint8_t>(0x80u >> (tx % 8));
    }
  }
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

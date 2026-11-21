#include "ClockFaces.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxTokens.h"
#include "ClockType.h"
#include "sloppy/SloppyDigits.h"

namespace clockfaces {

namespace {

namespace cc = clockcore;

constexpr float kPi = 3.14159265f;

// The panel as a face sees it.
struct Canvas {
  GfxRenderer& r;
  int w;
  int h;
  bool wide() const { return w > h; }
};

void disc(const Canvas& c, const int cx, const int cy, const int radius, const bool ink = true) {
  for (int dy = -radius; dy <= radius; ++dy) {
    const int half = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
    c.r.fillRect(cx - half, cy + dy, half * 2 + 1, 1, ink);
  }
}

// A straight stroke with round ends.
void stroke(const Canvas& c, const float x0, const float y0, const float x1, const float y1, const float pen,
            const bool ink = true) {
  const float len = std::hypot(x1 - x0, y1 - y0);
  if (len > 0.5f) {
    const float nx = -(y1 - y0) / len * pen;
    const float ny = (x1 - x0) / len * pen;
    const int xs[4] = {static_cast<int>(std::lround(x0 + nx)), static_cast<int>(std::lround(x1 + nx)),
                       static_cast<int>(std::lround(x1 - nx)), static_cast<int>(std::lround(x0 - nx))};
    const int ys[4] = {static_cast<int>(std::lround(y0 + ny)), static_cast<int>(std::lround(y1 + ny)),
                       static_cast<int>(std::lround(y1 - ny)), static_cast<int>(std::lround(y0 - ny))};
    c.r.fillPolygon(xs, ys, 4, ink);
  }
  disc(c, static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)), static_cast<int>(pen), ink);
  disc(c, static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)), static_cast<int>(pen), ink);
}

// A numeral drawn with a round pen inside the box (x, y, boxW, boxW * 1.6).
// The pen's own width is taken out of the box, so the ink never leaves it.
void numeral(const Canvas& c, const int digit, const float x, const float y, const float boxW, const float pen,
             const bool ink = true) {
  const cc::Stroke* strokes = nullptr;
  const int count = cc::strokesOf(digit, strokes);
  const float sx = boxW - 2 * pen;
  const float sy = (boxW * cc::kNumeralHeight - 2 * pen) / cc::kNumeralHeight;
  const float ox = x + pen;
  const float oy = y + pen;
  // Discs a third of a pen apart read as one smooth stroke.
  const float step = pen / 3 < 1 ? 1 : pen / 3;
  for (int i = 0; i < count; ++i) {
    const cc::Stroke& s = strokes[i];
    if (s.kind == 'L') {
      stroke(c, ox + s.v[0] * sx, oy + s.v[1] * sy, ox + s.v[2] * sx, oy + s.v[3] * sy, pen, ink);
      continue;
    }
    const float rx = s.v[2] * sx;
    const float ry = s.v[3] * sy;
    const float sweep = (s.v[5] - s.v[4]) * kPi / 180;
    const int steps = static_cast<int>(std::fabs(sweep) * (rx > ry ? rx : ry) / step) + 2;
    for (int k = 0; k <= steps; ++k) {
      const float a = s.v[4] * kPi / 180 + sweep * static_cast<float>(k) / static_cast<float>(steps);
      disc(c, static_cast<int>(std::lround(ox + s.v[0] * sx + rx * std::cos(a))),
           static_cast<int>(std::lround(oy + s.v[1] * sy + ry * std::sin(a))), static_cast<int>(pen), ink);
    }
  }
}

// Text in one of the Toybox cuts, placed by its ink: `top` is where the
// capitals begin. align: -1 left of x, 0 centred on x, 1 right-aligned to x.
void text(const Canvas& c, const int font, const toybox::CutMetrics& cut, const int x, const int top, const char* s,
          const int align = 0, const bool ink = true) {
  const int width = c.r.getTextWidth(font, s);
  const int left = align < 0 ? x : (align == 0 ? x - width / 2 : x - width);
  c.r.drawText(font, left, top - (cut.ascender - cut.inkHeight), s, ink);
}

// ---------------------------------------------------------------- the type
// The faces are set in type baked by tools_local/clock/make_type.py: a string
// is drawn from its pen position on the baseline, in UTF-8.

using clocktype::Font;
using clocktype::Glyph;

const Glyph* glyphOf(const Font& font, const uint16_t codepoint) {
  int lo = 0;
  int hi = font.count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (font.glyphs[mid].codepoint == codepoint) return &font.glyphs[mid];
    if (font.glyphs[mid].codepoint < codepoint) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return nullptr;
}

// The next character of a UTF-8 string; one and two byte forms are all the
// faces use.
uint16_t nextChar(const char*& s) {
  const uint8_t a = static_cast<uint8_t>(*s++);
  if (a < 0x80 || *s == '\0') return a;
  const uint8_t b = static_cast<uint8_t>(*s++);
  return static_cast<uint16_t>(((a & 0x1F) << 6) | (b & 0x3F));
}

int widthOf(const Font& font, const char* s, const int track = 0) {
  int width = 0;
  int count = 0;
  while (*s != '\0') {
    const Glyph* g = glyphOf(font, nextChar(s));
    if (g == nullptr) continue;
    width += g->advance;
    ++count;
  }
  return count > 1 ? width + track * (count - 1) : width;
}

// `half` leaves every other pixel out, which is the nearest this panel comes
// to grey without a slow refresh.
void type(const Canvas& c, const Font& font, int x, const int baseline, const char* s, const bool ink = true,
          const int track = 0, const bool half = false) {
  while (*s != '\0') {
    const Glyph* g = glyphOf(font, nextChar(s));
    if (g == nullptr) continue;
    const uint8_t* runs = font.runs + g->offset;
    const int left = x + g->left;
    const int top = baseline - g->top;
    for (int row = 0; row < g->rows; ++row) {
      const int count = *runs++;
      for (int i = 0; i < count; ++i) {
        if (!half) {
          c.r.fillRect(left + runs[0], top + row, runs[1], 1, ink);
        } else {
          for (int px = left + runs[0] + ((left + runs[0] + top + row) & 1); px < left + runs[0] + runs[1]; px += 2) {
            c.r.fillRect(px, top + row, 1, 1, ink);
          }
        }
        runs += 2;
      }
    }
    x += g->advance + track;
  }
}

// align: -1 from x, 0 centred on x, 1 ending at x.
void typeAt(const Canvas& c, const Font& font, const int x, const int baseline, const char* s, const int align,
            const bool ink = true, const int track = 0, const bool half = false) {
  const int width = widthOf(font, s, track);
  type(c, font, align < 0 ? x : (align == 0 ? x - width / 2 : x - width), baseline, s, ink, track, half);
}

// A box with round corners of any radius, solid.
void slab(const Canvas& c, const int x, const int y, const int w, const int h, int radius, const bool ink = true) {
  if (radius * 2 > w) radius = w / 2;
  if (radius * 2 > h) radius = h / 2;
  for (int row = 0; row < h; ++row) {
    int inset = 0;
    const int fromEdge = row < radius ? radius - row : (row >= h - radius ? row - (h - radius - 1) : 0);
    if (fromEdge > 0) {
      inset = radius - static_cast<int>(std::sqrt(static_cast<float>(radius * radius - fromEdge * fromEdge)));
    }
    c.r.fillRect(x + inset, y + row, w - inset * 2, 1, ink);
  }
}

// A straight bar with square ends, lying along the dial: from `r0` to `r1`
// out from the centre, at `degrees` clockwise from twelve.
void spoke(const Canvas& c, const int cx, const int cy, const float degrees, const float r0, const float r1,
           const float width) {
  const float a = degrees * kPi / 180.0f;
  const float dx = std::sin(a);
  const float dy = -std::cos(a);
  const float nx = -dy * width / 2;
  const float ny = dx * width / 2;
  const int xs[4] = {static_cast<int>(std::lround(cx + dx * r0 + nx)), static_cast<int>(std::lround(cx + dx * r1 + nx)),
                     static_cast<int>(std::lround(cx + dx * r1 - nx)),
                     static_cast<int>(std::lround(cx + dx * r0 - nx))};
  const int ys[4] = {static_cast<int>(std::lround(cy + dy * r0 + ny)), static_cast<int>(std::lround(cy + dy * r1 + ny)),
                     static_cast<int>(std::lround(cy + dy * r1 - ny)),
                     static_cast<int>(std::lround(cy + dy * r0 - ny))};
  c.r.fillPolygon(xs, ys, 4, true);
}

// What every face says, worked out once. All of it is empty until the clock
// has been set, and an empty string draws nothing.
struct Said {
  char hh[4] = "";
  char mm[4] = "";
  char time[8] = "";   // 10:09
  char day[4] = "";    // 18
  char week[16] = "";  // semana 47
  char weekCaps[16] = "";
  char weekShort[8] = "";   // S47
  char shortCaps[24] = "";  // MIE 18 NOV, with its accent
  const char* weekday = "";
  const char* weekdayCaps = "";
};

Said saidOf(const Moment& now) {
  Said s;
  if (!now.valid) return s;
  std::snprintf(s.hh, sizeof(s.hh), "%02d", now.hour % 100);
  std::snprintf(s.mm, sizeof(s.mm), "%02d", now.minute % 100);
  std::snprintf(s.time, sizeof(s.time), "%s:%s", s.hh, s.mm);
  std::snprintf(s.day, sizeof(s.day), "%d", now.day % 100);
  const int week = cc::isoWeek(now.year, now.yearDay, now.weekday) % 100;
  std::snprintf(s.week, sizeof(s.week), "semana %d", week);
  std::snprintf(s.weekCaps, sizeof(s.weekCaps), "SEMANA %d", week);
  std::snprintf(s.weekShort, sizeof(s.weekShort), "S%d", week);
  std::snprintf(s.shortCaps, sizeof(s.shortCaps), "%s %d %s", cc::weekdayShort(now.weekday, true), now.day % 100,
                cc::monthShort(now.month, true));
  s.weekday = cc::weekdayLong(now.weekday, false);
  s.weekdayCaps = cc::weekdayLong(now.weekday, true);
  return s;
}

// Two rows of numerals, one above the other, each centred on `cx` (or begun
// at it), with the pair centred on `cy`.
void stacked(const Canvas& c, const Font& top, const Font& bottom, const Said& s, const int cx, const int cy,
             const int pitch, const int track, const bool ink, const int align = 0) {
  const int first = cy - (top.capTop + pitch) / 2 + top.capTop;
  typeAt(c, top, cx, first, s.hh, align, ink, track);
  typeAt(c, bottom, cx, first + pitch, s.mm, align, ink, track);
}

// ---------------------------------------------------------------- 01 EDITORIAL
// A page of a magazine: the hours upright and the minutes in italic, between
// two rules, with the time written out underneath.
void faceEditorial(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  const int pad = wide ? 40 : 36;
  const int head = wide ? 41 : 45;
  char date[32] = "";
  char phrase[96] = "";
  if (now.valid) {
    std::snprintf(date, sizeof(date), "%d · %s · %d", now.day % 100, cc::monthRoman(now.month), now.year % 10000);
    cc::longTimeEs(now.hour, now.minute, phrase, sizeof(phrase));
  }
  type(c, clocktype::kMonoLabel, pad, head, s.weekdayCaps, true, 2);
  typeAt(c, clocktype::kMonoLabel, c.w - pad, head, date, 1, true, 2);
  c.r.fillRect(pad, head + 15, c.w - pad * 2, 2, true);

  int ruleY = 0;
  if (wide) {
    ruleY = 407;
    type(c, clocktype::kSerifPhrase, pad, 445, phrase);
    typeAt(c, clocktype::kMonoLabel, c.w - pad, 445, s.weekCaps, 1, true, 2);
  } else {
    // The phrase breaks in two when it is longer than the page is wide.
    const int room = c.w - pad * 2;
    char second[96] = "";
    if (widthOf(clocktype::kSerifPhrase, phrase) > room) {
      for (int cut = static_cast<int>(std::strlen(phrase)) - 1; cut > 0; --cut) {
        if (phrase[cut] != ' ') continue;
        phrase[cut] = '\0';
        if (widthOf(clocktype::kSerifPhrase, phrase) <= room) {
          std::snprintf(second, sizeof(second), "%s", phrase + cut + 1);
          break;
        }
        phrase[cut] = ' ';
      }
    }
    const int last = 730;
    const int first = second[0] != '\0' ? last - 36 : last;
    type(c, clocktype::kSerifPhrase, pad, first, phrase);
    type(c, clocktype::kSerifPhrase, pad, last, second);
    type(c, clocktype::kMonoLabel, pad, 765, s.weekCaps, true, 2);
    ruleY = first - 40;
  }
  c.r.fillRect(pad, ruleY, c.w - pad * 2, 2, true);

  const int top = head + 17;
  if (wide) {
    const Font& upright = clocktype::kSerifBig;
    const Font& italic = clocktype::kSerifBigItalic;
    const int track = -9;
    const int gap = 6;
    const char* colon = now.valid ? ":" : "";
    const int total =
        widthOf(upright, s.hh, track) + gap + widthOf(upright, colon) + gap + widthOf(italic, s.mm, track);
    int x = (c.w - total) / 2;
    const int baseline = (top + ruleY) / 2 + upright.capTop / 2;
    type(c, upright, x, baseline, s.hh, true, track);
    x += widthOf(upright, s.hh, track) + gap;
    type(c, upright, x, baseline, colon);
    x += widthOf(upright, colon) + gap;
    type(c, italic, x, baseline, s.mm, true, track);
  } else {
    stacked(c, clocktype::kSerifTall, clocktype::kSerifTallItalic, s, c.w / 2, (top + ruleY) / 2, 264, -10, true);
  }
}

// ---------------------------------------------------------------- 02 PUNTOS
// White lamps on black, and the day as twenty-four dots: the hours gone
// solid, this one a ring, the ones to come dim.
void dimDisc(const Canvas& c, const int cx, const int cy, const int radius) {
  for (int dy = -radius; dy <= radius; ++dy) {
    const int half = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
    if (((cy + dy) & 1) != 0) continue;
    for (int x = cx - half; x <= cx + half; ++x) {
      if ((x & 1) == 0) c.r.fillRect(x, cy + dy, 1, 1, false);
    }
  }
}

void hourDot(const Canvas& c, const Moment& now, const int index, const int cx, const int cy, const int radius,
             const int ringWidth) {
  if (!now.valid || index > now.hour) {
    dimDisc(c, cx, cy, radius);
  } else {
    disc(c, cx, cy, radius, false);
    if (index == now.hour) disc(c, cx, cy, radius - ringWidth, true);
  }
}

void facePuntos(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  c.r.fillRect(0, 0, c.w, c.h, true);
  const Font& label = clocktype::kDotLabel;
  if (wide) {
    type(c, label, 40, 52, s.shortCaps, false, 2);
    typeAt(c, label, c.w - 40, 52, s.weekCaps, 1, false, 2);
    typeAt(c, clocktype::kDotBig, c.w / 2, 236 + clocktype::kDotBig.capTop / 2, s.time, 0, false);
    const int cy = 438;
    type(c, label, 40, cy + label.capTop / 2, "00", false);
    typeAt(c, label, c.w - 40, cy + label.capTop / 2, "24", 1, false);
    const int pitch = 26;
    const int left = (c.w - 23 * pitch) / 2;
    for (int i = 0; i < 24; ++i) hourDot(c, now, i, left + i * pitch, cy, 7, 3);
  } else {
    type(c, label, 36, 56, s.shortCaps, false, 2);
    typeAt(c, label, c.w - 36, 56, s.weekShort, 1, false, 2);
    const int pitch = 48;
    const int gridTop = c.h - 34 - 3 * pitch - 28;
    stacked(c, clocktype::kDotTall, clocktype::kDotTall, s, c.w / 2, (70 + gridTop - 24) / 2, 241, 0, false);
    const int left = (c.w - 5 * pitch) / 2;
    for (int i = 0; i < 24; ++i) hourDot(c, now, i, left + (i % 6) * pitch, gridTop + 14 + (i / 6) * pitch, 14, 5);
  }
}

// ---------------------------------------------------------------- 03 POSTER
// The panel cut in two: the hours white on black, the minutes black on white,
// as large as they go, and a bar along the foot that grows with the minute.
void facePoster(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  const Font& big = wide ? clocktype::kPosterBig : clocktype::kPosterTall;
  const Font& label = clocktype::kPosterLabel;
  const int track = -4;
  if (wide) {
    c.r.fillRect(0, 0, c.w / 2, c.h, true);
    typeAt(c, big, c.w / 4, c.h / 2 + big.capTop / 2, s.hh, 0, false, track);
    typeAt(c, big, c.w * 3 / 4, c.h / 2 + big.capTop / 2, s.mm, 0, true, track);
    type(c, label, 24, 34, s.shortCaps, false, 2);
    typeAt(c, label, c.w - 24, 34, s.weekCaps, 1, true, 2);
    if (now.valid) c.r.fillRect(c.w / 2, c.h - 14, (c.w / 2) * now.minute / 60, 14, true);
  } else {
    c.r.fillRect(0, 0, c.w, c.h / 2, true);
    typeAt(c, big, c.w / 2, c.h / 4 + big.capTop / 2 + 8, s.hh, 0, false, track);
    typeAt(c, big, c.w / 2, c.h * 3 / 4 + big.capTop / 2 + 8, s.mm, 0, true, track);
    type(c, label, 20, 27, s.shortCaps, false, 2);
    typeAt(c, label, c.w - 20, c.h / 2 + 27, s.weekCaps, 1, true, 2);
    if (now.valid) c.r.fillRect(0, c.h - 14, c.w * now.minute / 60, 14, true);
  }
}

// ---------------------------------------------------------------- 04 GROTESCA
// The date in three pills along the top and the time heavy at the foot.
int pill(const Canvas& c, const int x, const int y, const char* s, const bool solid) {
  const Font& font = clocktype::kGroteskLabel;
  if (s[0] == '\0') return 0;
  const int w = widthOf(font, s) + 44;
  const int h = 46;
  slab(c, x, y, w, h, h / 2, true);
  if (!solid) slab(c, x + 3, y + 3, w - 6, h - 6, h / 2 - 3, false);
  type(c, font, x + 22, y + h / 2 + font.capTop / 2, s, !solid);
  return w;
}

void faceGrotesca(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  char date[16] = "";
  if (now.valid) std::snprintf(date, sizeof(date), "%d %s", now.day % 100, cc::monthShort(now.month, false));
  if (wide) {
    int x = 40;
    x += pill(c, x, 32, s.weekday, true) + 12;
    pill(c, x, 32, date, false);
    const Font& font = clocktype::kGroteskLabel;
    typeAt(c, font, c.w - 40, 32 + 23 + font.capTop / 2, s.week, 1);
    char time[8] = "";
    if (now.valid) std::snprintf(time, sizeof(time), "%s.%s", s.hh, s.mm);
    type(c, clocktype::kGroteskBig, 32, c.h - 34, time, true, -12);
  } else {
    // The pills wrap: what does not fit beside the last one starts a new row.
    const char* pills[3] = {s.weekday, date, s.week};
    int x = 36;
    int y = 36;
    for (int i = 0; i < 3; ++i) {
      const int w = widthOf(clocktype::kGroteskLabel, pills[i]) + 44;
      if (x > 36 && x + w > c.w - 36) {
        x = 36;
        y += 58;
      }
      x += pill(c, x, y, pills[i], i == 0) + 12;
    }
    const int second = c.h - 40;
    type(c, clocktype::kGroteskTall, 26, second - 254, s.hh, true, -15);
    type(c, clocktype::kGroteskTall, 26, second, s.mm, true, -15);
  }
}

// ---------------------------------------------------------------- 05 NOIR
// Black, a hairline frame, and the time in a heavy italic.
void faceNoir(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  c.r.fillRect(0, 0, c.w, c.h, true);
  c.r.fillRect(16, 16, c.w - 32, 2, false);
  c.r.fillRect(16, c.h - 18, c.w - 32, 2, false);
  c.r.fillRect(16, 16, 2, c.h - 32, false);
  c.r.fillRect(c.w - 18, 16, 2, c.h - 32, false);
  char date[48] = "";
  if (now.valid) {
    char number[24];
    cc::numberEs(now.day, number, sizeof(number));
    std::snprintf(date, sizeof(date), "%s de %s", number, cc::monthLong(now.month));
  }
  typeAt(c, clocktype::kNoirDay, c.w / 2, wide ? 54 : 62, s.weekdayCaps, 0, false, 5);
  typeAt(c, clocktype::kNoirDate, c.w / 2, wide ? c.h - 46 : c.h - 54, date, 0, false);
  if (wide) {
    typeAt(c, clocktype::kNoirBig, c.w / 2, c.h / 2 + clocktype::kNoirBig.capTop / 2 - 4, s.time, 0, false, -7);
  } else {
    stacked(c, clocktype::kNoirTall, clocktype::kNoirTall, s, c.w / 2, c.h / 2, 246, -9, false);
  }
}

// ---------------------------------------------------------------- 06 TERMINAL
// A prompt, the answer to `date`, and how much of the day is gone.
void faceTerminal(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  const Font& label = clocktype::kTermLabel;
  const int pad = wide ? 40 : 36;
  const int head = wide ? 50 : 54;
  type(c, label, pad, head, "sticky ~ % date");
  c.r.fillRect(pad + widthOf(label, "sticky ~ % date") + 10, head - 18, 11, 22, true);

  char line[48] = "";
  char percent[16] = "";
  int gone = 0;
  if (now.valid) {
    std::snprintf(line, sizeof(line), "%s %d %s %d · %s %d", cc::weekdayShort(now.weekday, false), now.day % 100,
                  cc::monthShort(now.month, false), now.year % 10000, wide ? "semana" : "sem",
                  cc::isoWeek(now.year, now.yearDay, now.weekday) % 100);
    gone = (now.hour * 60 + now.minute) * 100 / 1440;
    std::snprintf(percent, sizeof(percent), "%d%%", gone % 1000);
  }
  const int barBase = c.h - (wide ? 36 : 40);
  type(c, label, pad, barBase - 36, line);
  type(c, label, pad, barBase, "día");
  typeAt(c, label, c.w - pad, barBase, percent, 1);
  const int barX = pad + widthOf(label, "día") + 14;
  const int barW = c.w - pad - widthOf(label, "100%") - 14 - barX;
  const int barY = barBase - 18;
  c.r.fillRect(barX, barY, barW, 22, true);
  c.r.fillRect(barX + 3, barY + 3, barW - 6, 16, false);
  c.r.fillRect(barX + 6, barY + 6, (barW - 12) * gone / 100, 10, true);

  if (wide) {
    type(c, clocktype::kTermBig, 34, 226 + clocktype::kTermBig.capTop / 2, s.time, true, -7);
  } else {
    stacked(c, clocktype::kTermTall, clocktype::kTermTall, s, 28, 396, 216, -10, true, -1);
  }
}

// ---------------------------------------------------------------- 07 CAPSULA
// The time in a black capsule that takes the page, the date above it.
void faceCapsula(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  const Font& label = clocktype::kCapsuleLabel;
  char date[16] = "";
  if (now.valid) std::snprintf(date, sizeof(date), "%d %s", now.day % 100, cc::monthShort(now.month, true));
  const int padX = wide ? 36 : 28;
  const int head = wide ? 46 : 50;
  type(c, label, padX + (wide ? 28 : 24), head, s.weekdayCaps, true, 1);
  typeAt(c, label, c.w - padX - (wide ? 28 : 24), head, date, 1, true, 1);
  const int top = head + (wide ? 24 : 27);
  const int height = c.h - (wide ? 28 : 32) - top;
  slab(c, padX, top, c.w - padX * 2, height, wide ? 120 : 140, true);
  if (wide) {
    typeAt(c, clocktype::kCapsuleBig, c.w / 2, top + height / 2 + clocktype::kCapsuleBig.capTop / 2, s.time, 0, false,
           -7);
  } else {
    stacked(c, clocktype::kCapsuleTall, clocktype::kCapsuleTall, s, c.w / 2, top + height / 2, 219, -9, false);
  }
}

// ---------------------------------------------------------------- 08 PIXEL
// A chequered band, the time in blocks, and the date on a black foot.
void facePixel(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  for (int y = 0; y < 32; y += 8) {
    for (int x = ((y / 8) & 1) * 8; x < c.w; x += 16) c.r.fillRect(x, y, 8, 8, true);
  }
  const int footH = 60;
  c.r.fillRect(0, c.h - footH, c.w, footH, true);
  const Font& label = clocktype::kPixelLabel;
  const int base = c.h - footH / 2 + label.capTop / 2;
  type(c, label, wide ? 28 : 24, base, s.shortCaps, false, 2);
  typeAt(c, label, c.w - (wide ? 28 : 24), base, wide ? s.weekCaps : s.weekShort, 1, false, 2);
  const int middle = (32 + c.h - footH) / 2;
  if (wide) {
    typeAt(c, clocktype::kPixelBig, c.w / 2, middle + clocktype::kPixelBig.capTop / 2, s.time, 0);
  } else {
    stacked(c, clocktype::kPixelTall, clocktype::kPixelTall, s, c.w / 2, middle, 302, 0, true);
  }
}

// ---------------------------------------------------------------- 09 PALABRAS
// The time as it is said, to the minute, as large as the words allow.
void facePalabras(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  c.r.fillRect(0, 0, c.w, c.h, true);
  if (!now.valid) return;
  const int padX = wide ? 40 : 36;
  const cc::Said said = cc::saidEs(now.hour, now.minute);
  const int leadBase = wide ? 68 : 82;
  type(c, clocktype::kWords50, padX, leadBase, said.lead, false, -2, true);

  char date[48];
  std::snprintf(date, sizeof(date), "%s %d de %s", s.weekday, now.day % 100, cc::monthLong(now.month));
  const Font& label = clocktype::kWordsLabel;
  if (wide) {
    type(c, label, padX, c.h - 34, date, false);
    typeAt(c, label, c.w - padX, c.h - 34, s.time, 1, false);
  } else {
    type(c, label, padX, c.h - 74, date, false);
    type(c, label, padX, c.h - 44, s.time, false);
  }

  // The hour on one line; what follows it on one more, or on two with the
  // first word alone, whichever lets the type be larger.
  const int areaTop = leadBase + 14;
  const int areaH = c.h - (wide ? 58 : 108) - areaTop;
  const int room = c.w - padX * 2;
  char first[32];
  char rest[32] = "";
  std::snprintf(first, sizeof(first), "%s", said.tail);
  if (char* space = std::strchr(first, ' ')) {
    *space = '\0';
    std::snprintf(rest, sizeof(rest), "%s", space + 1);
  }
  static const Font* const kSizes[] = {&clocktype::kWords148, &clocktype::kWords120, &clocktype::kWords96,
                                       &clocktype::kWords76,  &clocktype::kWords60,  &clocktype::kWords50};
  static constexpr int kPixels[] = {148, 120, 96, 76, 60, 50};
  for (int i = 0; i < 6; ++i) {
    const Font& font = *kSizes[i];
    const int track = -kPixels[i] / 25;
    const int pitch = kPixels[i] * 92 / 100;
    const bool last = i == 5;
    for (int split = 0; split < 2; ++split) {
      if (split == 1 && rest[0] == '\0') break;
      const char* lines[3] = {said.hour, split == 0 ? said.tail : first, split == 0 ? "" : rest};
      const int count = split == 0 ? 2 : 3;
      bool fits = count * pitch <= areaH;
      for (int k = 0; k < count; ++k) fits = fits && widthOf(font, lines[k], track) <= room;
      if (!fits && !(last && split == 1)) continue;
      // Set from the top: the first line's tallest letter just under the lead.
      int baseline = areaTop + pitch - pitch / 8;
      for (int k = 0; k < count; ++k, baseline += pitch) type(c, font, padX, baseline, lines[k], false, track);
      return;
    }
  }
}

// ---------------------------------------------------------------- 10 ANALOGICO
// A dial with twelve marks and two hands, and the day of the month large
// beside it.
void faceAnalogico(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  const bool wide = c.wide();
  const int cx = 240;
  const int cy = 240;
  disc(c, cx, cy, 200, true);
  disc(c, cx, cy, 190, false);
  for (int k = 0; k < 12; ++k) {
    if (k % 3 == 0) {
      spoke(c, cx, cy, k * 30.0f, 150, 180, 8);
    } else {
      spoke(c, cx, cy, k * 30.0f, 162, 180, 4);
    }
  }
  if (now.valid) {
    spoke(c, cx, cy, (now.hour % 12) * 30.0f + now.minute * 0.5f, 0, 110, 16);
    spoke(c, cx, cy, now.minute * 6.0f, 0, 160, 8);
  }
  disc(c, cx, cy, 14, true);

  const Font& label = clocktype::kDialLabel;
  const char* month = now.valid ? cc::monthLong(now.month) : "";
  if (wide) {
    const int x = 488;
    const Font& day = clocktype::kDialDay;
    const int dayBase = 150 + 20 + day.capTop;
    type(c, label, x, 150, s.weekdayCaps, true, 3);
    type(c, day, x - 6, dayBase, s.day, true, -7);
    type(c, clocktype::kDialMonth, x, dayBase + 44, month);
    type(c, label, x, dayBase + 82, s.week);
  } else {
    const Font& day = clocktype::kDialDayTall;
    const int dayBase = c.h - 44;
    type(c, label, 40, dayBase - day.capTop - 22, s.weekdayCaps, true, 3);
    type(c, day, 32, dayBase, s.day, true, -10);
    typeAt(c, clocktype::kDialMonth, c.w - 40, dayBase - 38, month, 1);
    typeAt(c, label, c.w - 40, dayBase - 6, s.week, 1);
  }
}

// ------------------------------------------------------------------ 11 SLOPPY
// The time written by hand: CrossMux's sloppy numerals (sloppy/README.md). The
// hand is dealt from the date and the hour, so it holds still from one minute
// to the next and is somebody else's every hour. Style and seeds are static:
// 440 bytes that the render task's stack does not have to carry.
void faceSloppy(const Canvas& c, const Moment& now) {
  const Said s = saidOf(now);
  if (!now.valid) return;
  static sloppy::Style style;
  static sloppy::Seeds seeds;
  static uint32_t dealt = 0;
  uint32_t seed = static_cast<uint32_t>(now.year * 366 + now.yearDay) * 2654435761u;
  seed ^= static_cast<uint32_t>(now.hour + 1) * 0x85EBCA6Bu;
  if (seed == 0) seed = 1;
  if (seed != dealt) {
    sloppy::rollStyle(seed, style);
    sloppy::prepareSeeds(seed, style, seeds);
    // The thinnest hands CrossMux deals are for a screen held in the hand;
    // this one is read from a stand across the room.
    if (style.strokeWidth < 4) style.strokeWidth = 4;
    dealt = seed;
  }
  const int foot = toybox::kTileCut.inkHeight + 28;
  text(c, toybox::kTileFontId, toybox::kTileCut, c.w / 2, c.h - foot + 6, s.shortCaps, 0);
  if (c.wide()) {
    // Hours and minutes side by side, with the colon as two blots between.
    const int gap = 72;
    const int half = (c.w - gap) / 2;
    const int top = 12;
    const int tall = c.h - foot - top;
    sloppy::draw(c.r, style, seeds, s.hh, sloppy::Bounds{8, top, half - 8, tall});
    sloppy::draw(c.r, style, seeds, s.mm, sloppy::Bounds{half + gap, top, half - 8, tall});
    const int dot = style.strokeWidth + 3;
    disc(c, c.w / 2, top + tall / 2 - tall / 7, dot);
    disc(c, c.w / 2 - 2, top + tall / 2 + tall / 7, dot);
  } else {
    char rows[8];
    std::snprintf(rows, sizeof(rows), "%s\n%s", s.hh, s.mm);
    sloppy::draw(c.r, style, seeds, rows, sloppy::Bounds{0, 16, c.w, c.h - foot - 16});
  }
}

}  // namespace

void draw(GfxRenderer& renderer, const int face, const Moment& now) {
  const Canvas c{renderer, renderer.getScreenWidth(), renderer.getScreenHeight()};
  switch (face) {
    case 0:
      faceEditorial(c, now);
      break;
    case 1:
      facePuntos(c, now);
      break;
    case 2:
      facePoster(c, now);
      break;
    case 3:
      faceGrotesca(c, now);
      break;
    case 4:
      faceNoir(c, now);
      break;
    case 5:
      faceTerminal(c, now);
      break;
    case 6:
      faceCapsula(c, now);
      break;
    case 7:
      facePixel(c, now);
      break;
    case 8:
      facePalabras(c, now);
      break;
    case 9:
      faceAnalogico(c, now);
      break;
    default:
      faceSloppy(c, now);
      break;
  }
}

void drawNumeral(GfxRenderer& renderer, const int digit, const float x, const float y, const float boxW,
                 const float pen, const bool ink) {
  const Canvas c{renderer, renderer.getScreenWidth(), renderer.getScreenHeight()};
  numeral(c, digit, x, y, boxW, pen, ink);
}

void drawHint(GfxRenderer& renderer, const char* message) {
  const Canvas c{renderer, renderer.getScreenWidth(), renderer.getScreenHeight()};
  const int h = toybox::kTileCut.inkHeight + 18;
  renderer.fillRect(0, c.h - h, c.w, h, true);
  text(c, toybox::kTileFontId, toybox::kTileCut, c.w / 2, c.h - h + 9, message, 0, false);
}

}  // namespace clockfaces

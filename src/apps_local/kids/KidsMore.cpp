#include "KidsMore.h"

#include <HalStorage.h>
#include <Memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../components/UITheme.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"
#include "KidsDots.h"
#include "KidsPictures.h"

namespace kidsmoreui {

namespace lab = toyboxlab;
using namespace kidsmore;

namespace {

constexpr int kTopBar = 44;
// How long a right answer is admired before the next question.
constexpr uint32_t kAdvanceMs = 1600;

// --- Drawing, straight to the panel ---------------------------------------------

struct Face {
  int font;
  const toybox::CutMetrics* cut;
};
const Face kHuge = {toybox::kHugeFontId, &toybox::kHugeCut};
const Face kLarge = {toybox::kLargeFontId, &toybox::kLargeCut};
const Face kDisplay = {toybox::kDisplayFontId, &toybox::kDisplayCut};
const Face kUi = {toybox::kUiFontId, &toybox::kUiCut};
const Face kTile = {toybox::kTileFontId, &toybox::kTileCut};

// Text with its ink centred on (cx, cy).
void text(GfxRenderer& r, const Face& face, const int cx, const int cy, const char* s, const bool black = true) {
  const int w = r.getTextWidth(face.font, s);
  const int inkTop = cy - face.cut->inkHeight / 2;
  r.drawText(face.font, cx - w / 2, inkTop - (face.cut->ascender - face.cut->inkHeight), s, black);
}

void textLeft(GfxRenderer& r, const Face& face, const int x, const int cy, const char* s) {
  r.drawText(face.font, x, cy - face.cut->inkHeight / 2 - (face.cut->ascender - face.cut->inkHeight), s, true);
}

// The biggest face that fits `maxW`, from `first` down.
const Face& fitting(GfxRenderer& r, const char* s, const int maxW, const int maxH, const Face& first) {
  const Face* order[] = {&kHuge, &kLarge, &kDisplay, &kUi, &kTile};
  bool started = false;
  for (const Face* f : order) {
    started = started || f == &first;
    if (!started) continue;
    if (r.getTextWidth(f->font, s) <= maxW && f->cut->inkHeight <= maxH) return *f;
  }
  return kTile;
}

// A picture centred on (cx, cy): every source pixel a `mul` square, or one
// pixel for every `div` squared when it is drawn small.
void picture(GfxRenderer& r, const int index, const int cx, const int cy, const int mul, const int div = 1,
             const bool black = true) {
  if (index < 0 || index >= kids::kPictures) return;
  const freeink::Icon& icon = *kidspictures::kAll[index];
  const int rowBytes = (icon.w + 7) / 8;
  auto ink = [&](const int x, const int y) { return (icon.bits[y * rowBytes + x / 8] & (0x80u >> (x % 8))) == 0; };
  if (div > 1) {
    const int w = icon.w / div;
    const int h = icon.h / div;
    const int x0 = cx - w / 2;
    const int y0 = cy - h / 2;
    for (int y = 0; y < h; ++y) {
      int run = 0;
      for (int x = 0; x <= w; ++x) {
        bool any = false;
        if (x < w) {
          for (int j = 0; j < div && !any; ++j) {
            for (int i = 0; i < div && !any; ++i) any = ink(x * div + i, y * div + j);
          }
        }
        if (any) {
          ++run;
        } else if (run > 0) {
          r.fillRect(x0 + x - run, y0 + y, run, 1, black);
          run = 0;
        }
      }
    }
    return;
  }
  const int x0 = cx - icon.w * mul / 2;
  const int y0 = cy - icon.h * mul / 2;
  for (int y = 0; y < icon.h; ++y) {
    int run = 0;
    for (int x = 0; x <= icon.w; ++x) {
      if (x < icon.w && ink(x, y)) {
        ++run;
      } else if (run > 0) {
        r.fillRect(x0 + (x - run) * mul, y0 + y * mul, run * mul, mul, black);
        run = 0;
      }
    }
  }
}

// A picture as big as a `side` square allows.
void pictureIn(GfxRenderer& r, const int index, const int cx, const int cy, const int side, const bool black = true) {
  if (side >= 384) {
    picture(r, index, cx, cy, 4, 1, black);
  } else if (side >= 288) {
    picture(r, index, cx, cy, 3, 1, black);
  } else if (side >= 192) {
    picture(r, index, cx, cy, 2, 1, black);
  } else if (side >= 96) {
    picture(r, index, cx, cy, 1, 1, black);
  } else if (side >= 48) {
    picture(r, index, cx, cy, 1, 2, black);
  } else {
    picture(r, index, cx, cy, 1, 3, black);
  }
}

void disc(GfxRenderer& r, const int cx, const int cy, const int radius, const bool black = true) {
  for (int y = -radius; y <= radius; ++y) {
    const int half = static_cast<int>(std::lround(std::sqrt(static_cast<float>(radius * radius - y * y))));
    r.fillRect(cx - half, cy + y, half * 2 + 1, 1, black);
  }
}

void ring(GfxRenderer& r, const int cx, const int cy, const int radius, const int weight) {
  disc(r, cx, cy, radius, true);
  disc(r, cx, cy, radius - weight, false);
}

void star(GfxRenderer& r, const int cx, const int cy, const int radius, const bool black = true) {
  int xs[10];
  int ys[10];
  for (int i = 0; i < 10; ++i) {
    const float a = -1.5707963f + static_cast<float>(i) * 0.6283185f;
    const float rad = static_cast<float>(i % 2 == 0 ? radius : radius * 2 / 5);
    xs[i] = cx + static_cast<int>(std::lround(std::cos(a) * rad));
    ys[i] = cy + static_cast<int>(std::lround(std::sin(a) * rad));
  }
  r.fillPolygon(xs, ys, 10, black);
}

void starRow(GfxRenderer& r, const int stars, const int cx, const int cy) {
  const int shown = stars > 10 ? 10 : stars;
  int x = cx - (shown - 1) * 20;
  for (int i = 0; i < shown; ++i, x += 40) star(r, x, cy, 15);
}

// A frame with round corners: `weight` thick, or solid.
void frame(GfxRenderer& r, const int x, const int y, const int w, const int h, const int weight,
           const bool solid = false) {
  if (solid) {
    r.fillRoundedRect(x, y, w, h, 14, Color::Black);
  } else {
    r.drawRoundedRect(x, y, w, h, weight, 14, true);
  }
}

// `n` of a picture laid out in the box, the last `crossed` of them struck out.
void group(GfxRenderer& r, const int x, const int y, const int w, const int h, const int n, const int index,
           const int crossed = 0, const bool black = true) {
  if (n <= 0) return;
  // The biggest of the picture's sizes whose grid fits the box.
  constexpr int kSides[] = {96, 48, 32};
  int side = 32;
  int cols = 1;
  for (const int s : kSides) {
    const int pitch = s + s / 8;
    const int perRow = std::max(1, w / pitch);
    const int rows = (n + perRow - 1) / perRow;
    side = s;
    cols = std::min(n, perRow);
    if (rows * pitch <= h) break;
  }
  // A full last row looks better than one straggler: even the rows out.
  int rows = (n + cols - 1) / cols;
  while (cols > 1 && (n + cols - 2) / (cols - 1) == rows) --cols;
  rows = (n + cols - 1) / cols;
  const int pitch = side + side / 8;
  const int top = y + (h - rows * pitch) / 2 + pitch / 2;
  for (int i = 0; i < n; ++i) {
    const int row = i / cols;
    const int inRow = row == rows - 1 ? n - row * cols : cols;
    const int cx = x + w / 2 - (inRow - 1) * pitch / 2 + (i % cols) * pitch;
    const int cy = top + row * pitch;
    pictureIn(r, index, cx, cy, side, black);
    if (i >= n - crossed) {
      const int half = side / 2 - 2;
      r.drawLine(cx - half, cy - half, cx + half, cy + half, 5, black);
      r.drawLine(cx - half, cy + half, cx + half, cy - half, 5, black);
    }
  }
}

// The page a finished game leaves: stars, two lines, and "touch to go on".
void drawWin(GfxRenderer& r, const int pictureIndex, const char* big, const char* small, const int bottom) {
  const int w = r.getScreenWidth();
  const int cx = w / 2;
  for (int i = 0; i < 5; ++i) star(r, cx + (i - 2) * 84, 110 + (i % 2) * 34, 30);
  pictureIn(r, pictureIndex, cx, 330, 288);
  text(r, kLarge, cx, 540, big);
  if (small[0]) text(r, kUi, cx, 620, small);
  text(r, kTile, cx, bottom - 20, "TOCA PER TORNAR A JUGAR");
}

bool inside(const int x, const int y, const int rx, const int ry, const int rw, const int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

// --- MEMORIA -------------------------------------------------------------------

class MemoryMini final : public Mini {
  const char* title() const override { return "MEMORIA"; }
  const char* file() const override { return "/.crosspoint/ilya-memoria.cfg"; }
  const Option* options() const override { return memory::kOptions; }
  int optionCount() const override { return memory::OptCount; }
  int face() const override { return 25; }

  struct Grid {
    int cols, rows, x, y, w, h, gap;
  };
  Grid grid(const GfxRenderer& r) const {
    Grid g;
    g.cols = memory::columns(game_.count);
    g.rows = memory::rows(game_.count);
    g.gap = 8;
    const int width = r.getScreenWidth() - 2 * toybox::kMargin;
    const int height = bottom(r) - kTopBar - 4;
    g.w = (width - g.gap * (g.cols - 1)) / g.cols;
    g.h = std::min((height - g.gap * (g.rows - 1)) / g.rows, g.w * 3 / 2);
    g.x = toybox::kMargin;
    g.y = kTopBar + 4 + (height - (g.h * g.rows + g.gap * (g.rows - 1))) / 2;
    return g;
  }

  void start(uint32_t) override { memory::deal(game_, values_, *rng_); }

  Did onTap(GfxRenderer& r, const int x, const int y, uint32_t) override {
    if (game_.done()) {
      memory::deal(game_, values_, *rng_);
      return Did::Clean;
    }
    const Grid g = grid(r);
    int index = -1;
    for (int i = 0; i < game_.count; ++i) {
      if (inside(x, y, g.x + (i % g.cols) * (g.w + g.gap), g.y + (i / g.cols) * (g.h + g.gap), g.w, g.h)) index = i;
    }
    switch (memory::tap(game_, index)) {
      case memory::Tap::Nothing:
        return Did::Nothing;
      case memory::Tap::Pair:
        ++stars_;
        return Did::Cheer;
      case memory::Tap::Done:
        ++stars_;
        return Did::Cheer;
      default:
        return Did::Paint;
    }
  }

  void drawFace(GfxRenderer& r, const memory::Card& card, const int cx, const int cy, const int w, const int h) {
    const int side = std::min(w, h) - 10;
    char s[12];
    switch (card.face) {
      case memory::Face::Picture:
        pictureIn(r, card.value, cx, cy, side);
        break;
      case memory::Face::Letter:
        s[0] = kids::letterAt(card.value);
        s[1] = 0;
        text(r, fitting(r, s, w - 12, h - 12, kHuge), cx, cy, s);
        break;
      case memory::Face::Number:
        std::snprintf(s, sizeof(s), "%d", card.value % 100);
        text(r, fitting(r, s, w - 12, h - 12, kHuge), cx, cy, s);
        break;
      case memory::Face::Dots: {
        const int n = card.value;
        const int cols = n <= 3 ? n : n <= 8 ? (n + 1) / 2 : 4;
        const int rows = (n + cols - 1) / cols;
        const int pitch = std::min((w - 16) / std::max(cols, 3), (h - 16) / std::max(rows, 3));
        const int radius = std::max(4, pitch / 2 - 4);
        for (int i = 0; i < n; ++i) {
          const int row = i / cols;
          const int inRow = row == rows - 1 ? n - row * cols : cols;
          disc(r, cx - (inRow - 1) * pitch / 2 + (i % cols) * pitch, cy - (rows - 1) * pitch / 2 + row * pitch, radius);
        }
        break;
      }
      case memory::Face::Word: {
        const char* word = kids::wordOf(card.value);
        text(r, fitting(r, word, w - 22, h - 12, kUi), cx, cy, word);
        break;
      }
    }
  }

  void drawPlay(GfxRenderer& r) override {
    const int width = r.getScreenWidth();
    if (game_.done()) {
      char line[48];
      if (game_.players == 2) {
        if (game_.score[0] == game_.score[1]) {
          std::snprintf(line, sizeof(line), "EMPAT: %d A %d", game_.score[0] % 100, game_.score[1] % 100);
        } else {
          std::snprintf(line, sizeof(line), "GUANYA EL JUGADOR %d", game_.score[0] > game_.score[1] ? 1 : 2);
        }
      } else {
        std::snprintf(line, sizeof(line), "EN %d INTENTS", game_.moves % 1000);
      }
      drawWin(r, 21, "MOLT BE!", line, bottom(r));
      return;
    }
    // The head: whose turn, or how many tries.
    char head[40];
    if (game_.players == 2) {
      for (int p = 0; p < 2; ++p) {
        std::snprintf(head, sizeof(head), "JUGADOR %d: %d", p + 1, game_.score[p] % 100);
        const int x = p == 0 ? toybox::kMargin : width / 2 + 4;
        const int w = width / 2 - toybox::kMargin - 4;
        const bool turn = game_.turn == p;
        if (turn) r.fillRoundedRect(x, 6, w, kTopBar - 10, 8, Color::Black);
        text(r, kTile, x + w / 2, 6 + (kTopBar - 10) / 2, head, !turn);
      }
    } else if (game_.peeking) {
      text(r, kTile, width / 2, kTopBar / 2, "MIRA-LES BE I TOCA PER AMAGAR-LES");
    } else {
      std::snprintf(head, sizeof(head), "PARELLES %d DE %d", game_.found % 100, (game_.count / 2) % 100);
      textLeft(r, kTile, toybox::kMargin, kTopBar / 2, head);
      std::snprintf(head, sizeof(head), "INTENTS %d", game_.moves % 1000);
      r.drawText(
          toybox::kTileFontId, width - toybox::kMargin - r.getTextWidth(toybox::kTileFontId, head),
          kTopBar / 2 - toybox::kTileCut.inkHeight / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight), head,
          true);
    }
    const Grid g = grid(r);
    for (int i = 0; i < game_.count; ++i) {
      const memory::Card& card = game_.cards[i];
      const int x = g.x + (i % g.cols) * (g.w + g.gap);
      const int y = g.y + (i / g.cols) * (g.h + g.gap);
      if (card.found) {
        // Found: its face stays, in a light frame, out of the game.
        r.drawRoundedRect(x, y, g.w, g.h, 1, 14, true);
        drawFace(r, card, x + g.w / 2, y + g.h / 2, g.w, g.h);
      } else if (card.up) {
        frame(r, x, y, g.w, g.h, 5);
        drawFace(r, card, x + g.w / 2, y + g.h / 2, g.w, g.h);
      } else {
        // Face down: a grey back with a white heart of a square in it.
        r.fillRoundedRect(x, y, g.w, g.h, 14, Color::DarkGray);
        const int inset = std::min(g.w, g.h) / 4;
        r.fillRoundedRect(x + inset, y + inset, g.w - 2 * inset, g.h - 2 * inset, 10, Color::White);
        star(r, x + g.w / 2, y + g.h / 2, std::min(g.w, g.h) / 6);
      }
    }
  }

  memory::Game game_;
};

// --- SUMES ---------------------------------------------------------------------

class SumsMini final : public Mini {
  const char* title() const override { return "SUMES"; }
  const char* file() const override { return "/.crosspoint/ilya-sumes.cfg"; }
  const Option* options() const override { return sums::kOptions; }
  int optionCount() const override { return sums::OptCount; }
  int face() const override { return 40; }

  bool pictures() const { return values_[sums::OptHelp] == 0; }

  struct Layout {
    int areaY, areaH, tileY, tileH, tileW, gap, starsY;
  };
  Layout layout(const GfxRenderer& r) const {
    Layout l;
    l.gap = 12;
    l.starsY = bottom(r) - 22;
    l.tileH = 190;
    l.tileY = l.starsY - 30 - l.tileH;
    l.areaY = kTopBar + 34;
    l.areaH = l.tileY - 16 - l.areaY;
    l.tileW = (r.getScreenWidth() - 2 * toybox::kMargin - 2 * l.gap) / 3;
    return l;
  }
  // More: the two groups are the answers, side by side.
  void sides(const GfxRenderer& r, int& y, int& w, int& h) const {
    y = kTopBar + 60;
    w = (r.getScreenWidth() - 2 * toybox::kMargin - 12) / 2;
    h = bottom(r) - 70 - y;
  }

  void start(uint32_t) override {
    round_ = sums::deal(values_, *rng_, sums::Round{});
    advanceAt_ = 0;
  }

  void next() {
    round_ = sums::deal(values_, *rng_, round_);
    advanceAt_ = 0;
  }

  Did onTick(const uint32_t now) override {
    if (advanceAt_ != 0 && now - advanceAt_ < 0x80000000u) {
      next();
      return Did::Paint;
    }
    return Did::Nothing;
  }

  Did onTap(GfxRenderer& r, const int x, const int y, const uint32_t now) override {
    if (round_.solved) {
      next();
      return Did::Paint;
    }
    int option = -1;
    if (round_.kind == sums::Kind::More) {
      int sy = 0;
      int sw = 0;
      int sh = 0;
      sides(r, sy, sw, sh);
      for (int i = 0; i < 2; ++i) {
        if (inside(x, y, toybox::kMargin + i * (sw + 12), sy, sw, sh)) option = i;
      }
    } else {
      const Layout l = layout(r);
      for (int i = 0; i < round_.optionCount; ++i) {
        if (inside(x, y, toybox::kMargin + i * (l.tileW + l.gap), l.tileY, l.tileW, l.tileH)) option = i;
      }
    }
    if (option < 0 || (round_.wrongMask & (1u << option)) != 0) return Did::Nothing;
    if (round_.choose(option)) {
      ++stars_;
      advanceAt_ = now + kAdvanceMs;
      if (advanceAt_ == 0) advanceAt_ = 1;
      return Did::Cheer;
    }
    return Did::Paint;
  }

  void drawPlay(GfxRenderer& r) override {
    const int width = r.getScreenWidth();
    const int inner = width - 2 * toybox::kMargin;
    const int cx = width / 2;
    const sums::Round& q = round_;
    const char* ask = "QUANTS N'HI HA?";
    if (q.kind == sums::Kind::Add) ask = "QUANTS EN FAN?";
    if (q.kind == sums::Kind::Take) ask = "QUANTS EN QUEDEN?";
    if (q.kind == sums::Kind::More) ask = q.fewer ? "ON N'HI HA MENYS?" : "ON N'HI HA MES?";
    if (q.kind == sums::Kind::Missing) ask = "QUIN NOMBRE FALTA?";
    text(r, kUi, cx, kTopBar / 2 + 14, ask);

    if (q.kind == sums::Kind::More) {
      int sy = 0;
      int sw = 0;
      int sh = 0;
      sides(r, sy, sw, sh);
      for (int i = 0; i < 2; ++i) {
        const int x = toybox::kMargin + i * (sw + 12);
        const bool right = q.solved && i == q.answer;
        const bool wrong = (q.wrongMask & (1u << i)) != 0;
        if (right) {
          frame(r, x, sy, sw, sh, 0, true);
        } else if (wrong) {
          r.drawRoundedRect(x, sy, sw, sh, 1, 14, true);
        } else {
          frame(r, x, sy, sw, sh, 4);
        }
        if (pictures()) {
          group(r, x + 8, sy + 8, sw - 16, sh - 16, q.options[i], q.picture, 0, !right);
        } else {
          char n[toybox::kIntChars + 1];
          std::snprintf(n, sizeof(n), "%d", q.options[i] % 100);
          text(r, fitting(r, n, sw - 20, sh, kHuge), x + sw / 2, sy + sh / 2, n, !right);
        }
        if (wrong) {
          r.drawLine(x + 20, sy + 20, x + sw - 20, sy + sh - 20, 3, true);
        }
      }
      starRow(r, stars_, cx, bottom(r) - 22);
      return;
    }

    const Layout l = layout(r);
    char sum[32];
    switch (q.kind) {
      case sums::Kind::Count:
        group(r, toybox::kMargin, l.areaY, inner, l.areaH, q.a, q.picture);
        break;
      case sums::Kind::Add:
      case sums::Kind::Take: {
        const bool add = q.kind == sums::Kind::Add;
        std::snprintf(sum, sizeof(sum), add ? "%d + %d" : "%d - %d", q.a % 100, q.b % 100);
        if (!pictures()) {
          text(r, fitting(r, sum, inner, l.areaH, kHuge), cx, l.areaY + l.areaH / 2, sum);
          break;
        }
        const int sumH = toybox::kLargeCut.inkHeight + 20;
        const int boxH = l.areaH - sumH;
        if (add) {
          const int plusW = 44;
          const int boxW = (inner - plusW) / 2;
          frame(r, toybox::kMargin, l.areaY, boxW, boxH, 3);
          frame(r, toybox::kMargin + boxW + plusW, l.areaY, boxW, boxH, 3);
          group(r, toybox::kMargin + 6, l.areaY + 6, boxW - 12, boxH - 12, q.a, q.picture);
          group(r, toybox::kMargin + boxW + plusW + 6, l.areaY + 6, boxW - 12, boxH - 12, q.b, q.picture);
          text(r, kDisplay, cx, l.areaY + boxH / 2, "+");
        } else {
          frame(r, toybox::kMargin, l.areaY, inner, boxH, 3);
          group(r, toybox::kMargin + 6, l.areaY + 6, inner - 12, boxH - 12, q.a, q.picture, q.b);
        }
        text(r, kLarge, cx, l.areaY + boxH + sumH / 2 + 4, sum);
        break;
      }
      case sums::Kind::Missing: {
        const int gap = 8;
        const int boxW = (inner - gap * (sums::kSequence - 1)) / sums::kSequence;
        const int boxH = boxW + 20;
        const int y = l.areaY + (l.areaH - boxH) / 2;
        for (int i = 0; i < sums::kSequence; ++i) {
          const int x = toybox::kMargin + i * (boxW + gap);
          const bool hole = i == q.hole;
          char n[toybox::kIntChars + 1];
          std::snprintf(n, sizeof(n), "%d", q.sequence[i] % 100);
          if (hole && !q.solved) {
            r.fillRoundedRect(x, y, boxW, boxH, 12, Color::LightGray);
            frame(r, x, y, boxW, boxH, 4);
            text(r, kDisplay, x + boxW / 2, y + boxH / 2, "?");
          } else {
            frame(r, x, y, boxW, boxH, hole ? 0 : 3, hole);
            text(r, fitting(r, n, boxW - 10, boxH, kDisplay), x + boxW / 2, y + boxH / 2, n, !hole);
          }
        }
        break;
      }
      case sums::Kind::More:
        break;
    }

    for (int i = 0; i < q.optionCount; ++i) {
      const int x = toybox::kMargin + i * (l.tileW + l.gap);
      const bool right = q.solved && i == q.answer;
      const bool wrong = (q.wrongMask & (1u << i)) != 0;
      char n[toybox::kIntChars + 1];
      std::snprintf(n, sizeof(n), "%d", q.options[i] % 100);
      if (right) {
        frame(r, x, l.tileY, l.tileW, l.tileH, 0, true);
        text(r, fitting(r, n, l.tileW - 16, l.tileH, kHuge), x + l.tileW / 2, l.tileY + l.tileH / 2, n, false);
      } else if (wrong) {
        r.drawRoundedRect(x, l.tileY, l.tileW, l.tileH, 1, 14, true);
        text(r, kUi, x + l.tileW / 2, l.tileY + l.tileH / 2, n);
      } else {
        frame(r, x, l.tileY, l.tileW, l.tileH, 4);
        text(r, fitting(r, n, l.tileW - 16, l.tileH, kHuge), x + l.tileW / 2, l.tileY + l.tileH / 2, n);
      }
    }
    starRow(r, stars_, cx, l.starsY);
  }

  sums::Round round_;
  uint32_t advanceAt_ = 0;
};

// --- LABERINT ------------------------------------------------------------------

class MazeMini final : public Mini {
  const char* title() const override { return "LABERINT"; }
  const char* file() const override { return "/.crosspoint/ilya-laberint.cfg"; }
  const Option* options() const override { return maze::kOptions; }
  int optionCount() const override { return maze::OptCount; }
  int face() const override { return 4; }

  struct Board {
    int cell, x, y;
  };
  Board board(const GfxRenderer& r) const {
    Board b;
    const int width = r.getScreenWidth() - 16;
    const int height = bottom(r) - kTopBar - 8;
    b.cell = std::min(width / maze_.w, height / maze_.h);
    b.x = (r.getScreenWidth() - b.cell * maze_.w) / 2;
    b.y = kTopBar + 4 + (height - b.cell * maze_.h) / 2;
    return b;
  }

  void start(uint32_t) override {
    maze::build(maze_, values_, *rng_);
    pair_ = maze::pairOf(values_, *rng_);
    hintCount_ = 0;
  }

  Did onTap(GfxRenderer& r, const int x, const int y, uint32_t) override {
    if (maze_.done()) {
      start(0);
      return Did::Clean;
    }
    // The head's right half asks for a hint.
    if (y < kTopBar && x > r.getScreenWidth() / 2) {
      hintCount_ = maze::hint(maze_, hint_, kHintCells);
      return hintCount_ > 0 ? Did::Paint : Did::Nothing;
    }
    const Board b = board(r);
    if (x < b.x || y < b.y) return Did::Nothing;
    const int cx = (x - b.x) / b.cell;
    const int cy = (y - b.y) / b.cell;
    if (cx >= maze_.w || cy >= maze_.h) return Did::Nothing;
    const int before = maze_.taken;
    if (maze::walkTowards(maze_, cy * maze_.w + cx) == 0) return Did::Nudge;
    hintCount_ = 0;
    if (maze_.done()) {
      ++stars_;
      return Did::Cheer;
    }
    return maze_.taken != before ? Did::Cheer : Did::Paint;
  }

  void drawPlay(GfxRenderer& r) override {
    const int width = r.getScreenWidth();
    if (maze_.done()) {
      char line[48];
      if (maze_.starCount > 0) {
        std::snprintf(line, sizeof(line), "%d PASSOS I %d ESTRELLES", maze_.steps % 10000, maze_.taken % 10);
      } else {
        std::snprintf(line, sizeof(line), "EN %d PASSOS", maze_.steps % 10000);
      }
      drawWin(r, pair_.goal, "HAS ARRIBAT!", line, bottom(r));
      return;
    }
    char head[32];
    if (maze_.starCount > 0) {
      for (int i = 0; i < maze_.starCount; ++i) {
        const int sx = toybox::kMargin + 14 + i * 34;
        star(r, sx, kTopBar / 2, 14);
        if (i >= maze_.taken) star(r, sx, kTopBar / 2, 8, false);
      }
    } else {
      std::snprintf(head, sizeof(head), "PASSOS %d", maze_.steps % 10000);
      textLeft(r, kTile, toybox::kMargin, kTopBar / 2, head);
    }
    const int hintW = 150;
    r.drawRoundedRect(width - toybox::kMargin - hintW, 5, hintW, kTopBar - 10, 2, 8, true);
    text(r, kTile, width - toybox::kMargin - hintW / 2, kTopBar / 2, "UNA PISTA");

    const Board b = board(r);
    const int wall = b.cell >= 70 ? 5 : b.cell >= 50 ? 4 : 3;
    const int w = maze_.w;
    const int h = maze_.h;
    // The outer wall, then each cell's north and west sides where they are shut.
    r.fillRect(b.x - wall / 2, b.y - wall / 2, w * b.cell + wall, wall, true);
    r.fillRect(b.x - wall / 2, b.y + h * b.cell - wall / 2, w * b.cell + wall, wall, true);
    r.fillRect(b.x - wall / 2, b.y - wall / 2, wall, h * b.cell + wall, true);
    r.fillRect(b.x + w * b.cell - wall / 2, b.y - wall / 2, wall, h * b.cell + wall, true);
    for (int c = 0; c < w * h; ++c) {
      const int x = b.x + (c % w) * b.cell;
      const int y = b.y + (c / w) * b.cell;
      if (c / w > 0 && !maze_.canGo(c, maze::North)) r.fillRect(x - wall / 2, y - wall / 2, b.cell + wall, wall, true);
      if (c % w > 0 && !maze_.canGo(c, maze::West)) r.fillRect(x - wall / 2, y - wall / 2, wall, b.cell + wall, true);
      const int cx = x + b.cell / 2;
      const int cy = y + b.cell / 2;
      if (values_[maze::OptTrail] == 0 && maze_.seen[c] && c != maze_.at) disc(r, cx, cy, std::max(3, b.cell / 12));
    }
    for (int i = 0; i < maze_.starCount; ++i) {
      if (maze_.stars[i] < 0) continue;
      star(r, b.x + (maze_.stars[i] % w) * b.cell + b.cell / 2, b.y + (maze_.stars[i] / w) * b.cell + b.cell / 2,
           b.cell / 3);
    }
    for (int i = 0; i < hintCount_; ++i) {
      ring(r, b.x + (hint_[i] % w) * b.cell + b.cell / 2, b.y + (hint_[i] / w) * b.cell + b.cell / 2,
           std::max(6, b.cell / 4), 3);
    }
    const int side = b.cell - wall - 2;
    pictureIn(r, pair_.goal, b.x + (maze_.goal % w) * b.cell + b.cell / 2, b.y + (maze_.goal / w) * b.cell + b.cell / 2,
              side);
    // The walker stands on a white patch, so the trail never shows through it.
    const int ax = b.x + (maze_.at % w) * b.cell + b.cell / 2;
    const int ay = b.y + (maze_.at / w) * b.cell + b.cell / 2;
    pictureIn(r, pair_.walker, ax, ay, side);
  }

  static constexpr int kHintCells = 4;
  maze::Maze maze_;
  maze::Pair pair_ = {4, 12};
  int hint_[kHintCells] = {};
  int hintCount_ = 0;
};

// --- PUNTS ---------------------------------------------------------------------

class DotsMini final : public Mini {
  const char* title() const override { return "PUNTS"; }
  const char* file() const override { return "/.crosspoint/ilya-punts.cfg"; }
  const Option* options() const override { return dots::kOptions; }
  int optionCount() const override { return dots::OptCount; }
  int face() const override { return 21; }

  static constexpr int kScale = 4;
  static constexpr int kTop = 70;

  int originX(const GfxRenderer& r) const { return (r.getScreenWidth() - 96 * kScale) / 2; }
  const kidsdots::Span& span() const { return kidsdots::kSpans[game_.figure * 3 + game_.level]; }
  void dot(const GfxRenderer& r, const int index, int& x, int& y) const {
    const uint8_t* p = kidsdots::kDots + (span().first + index % span().count) * 2;
    x = originX(r) + p[0] * kScale + kScale / 2;
    y = kTop + p[1] * kScale + kScale / 2;
  }

  void next() {
    uint8_t counts[kidsdots::kFigures * 3];
    for (int i = 0; i < kidsdots::kFigures * 3; ++i) counts[i] = kidsdots::kSpans[i].count;
    dots::start(game_, values_, *rng_, previous_, kidsdots::kFigures, counts);
    previous_ = game_.figure;
  }

  void start(uint32_t) override {
    previous_ = -1;
    next();
  }

  Did onTap(GfxRenderer& r, const int x, const int y, uint32_t) override {
    if (game_.done()) {
      next();
      return Did::Clean;
    }
    // The next dot gets the benefit of the doubt: a finger near it means it.
    int nx = 0;
    int ny = 0;
    dot(r, game_.joined, nx, ny);
    const long d2 = static_cast<long>(x - nx) * (x - nx) + static_cast<long>(y - ny) * (y - ny);
    if (d2 > 56L * 56L) return Did::Nudge;
    dots::tap(game_, game_.joined);
    if (game_.done()) {
      ++stars_;
      return Did::Cheer;
    }
    return Did::Paint;
  }

  void drawPlay(GfxRenderer& r) override {
    const int width = r.getScreenWidth();
    const int cx = width / 2;
    const int count = game_.count;
    const int picture = kidsdots::kPicture[game_.figure];
    const int ox = originX(r);

    // The middle of the figure, which the labels stand away from.
    long sumX = 0;
    long sumY = 0;
    for (int i = 0; i < count; ++i) {
      int x = 0;
      int y = 0;
      dot(r, i, x, y);
      sumX += x;
      sumY += y;
    }
    const int midX = static_cast<int>(sumX / count);
    const int midY = static_cast<int>(sumY / count);

    if (game_.done()) {
      // The lines close, and the picture they were is drawn over them.
      pictureIn(r, picture, ox + 96 * kScale / 2, kTop + 96 * kScale / 2, 96 * kScale);
      const char* word = kids::wordOf(picture);
      text(r, fitting(r, word, width - 2 * toybox::kMargin, 120, kHuge), cx, kTop + 96 * kScale + 90, word);
      starRow(r, stars_, cx, bottom(r) - 60);
      text(r, kTile, cx, bottom(r) - 18, "TOCA PER FER-NE UN ALTRE");
      return;
    }

    char head[40];
    std::snprintf(head, sizeof(head), "PUNT %d DE %d", (game_.joined + 1) % 100, count % 100);
    textLeft(r, kTile, toybox::kMargin, kTopBar / 2, head);
    const char* ask = "QUE SORTIRA?";
    r.drawText(toybox::kTileFontId, width - toybox::kMargin - r.getTextWidth(toybox::kTileFontId, ask),
               kTopBar / 2 - toybox::kTileCut.inkHeight / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight),
               ask, true);

    for (int i = 1; i < game_.joined; ++i) {
      int ax = 0;
      int ay = 0;
      int bx = 0;
      int by = 0;
      dot(r, i - 1, ax, ay);
      dot(r, i, bx, by);
      r.drawLine(ax, ay, bx, by, 4, true);
    }
    // The first dot is always marked: it is where the pencil goes down.
    const bool help = values_[dots::OptHelp] == 0 || game_.joined == 0;
    for (int i = 0; i < count; ++i) {
      int x = 0;
      int y = 0;
      dot(r, i, x, y);
      const bool reached = i < game_.joined;
      const bool isNext = i == game_.joined;
      if (isNext && help) {
        ring(r, x, y, 15, 3);
        disc(r, x, y, 8);
      } else if (reached) {
        disc(r, x, y, 5);
      } else {
        ring(r, x, y, 8, 3);
      }
      if (reached) continue;
      char label[8];
      dots::label(values_, i, count, label, sizeof(label));
      if (!label[0]) continue;
      // Beside the dot, on the side away from the middle.
      float dx = static_cast<float>(x - midX);
      float dy = static_cast<float>(y - midY);
      const float len = std::sqrt(dx * dx + dy * dy);
      if (len < 1.0f) {
        dx = 0.0f;
        dy = -1.0f;
      } else {
        dx /= len;
        dy /= len;
      }
      const int lx = x + static_cast<int>(dx * 24.0f);
      const int ly = y + static_cast<int>(dy * 22.0f);
      text(r, kTile, lx, ly, label);
    }
    starRow(r, stars_, cx, bottom(r) - 22);
  }

  dots::Game game_;
  int previous_ = -1;
};

}  // namespace

const char* nameOf(const Which which) {
  switch (which) {
    case Which::Sums:
      return "SUMES";
    case Which::Maze:
      return "LABERINT";
    case Which::Dots:
      return "PUNTS";
    case Which::Memory:
    default:
      return "MEMORIA";
  }
}

int pictureOf(const Which which) {
  switch (which) {
    case Which::Sums:
      return 38;  // GALETA
    case Which::Maze:
      return 4;  // CONILL
    case Which::Dots:
      return 21;  // ESTRELLA
    case Which::Memory:
    default:
      return 36;  // REGAL
  }
}

std::unique_ptr<Mini> make(const Which which) {
  switch (which) {
    case Which::Sums:
      return makeUniqueNoThrow<SumsMini>();
    case Which::Maze:
      return makeUniqueNoThrow<MazeMini>();
    case Which::Dots:
      return makeUniqueNoThrow<DotsMini>();
    case Which::Memory:
    default:
      return makeUniqueNoThrow<MemoryMini>();
  }
}

// --- What every one of them shares ---------------------------------------------

int Mini::bottom(const GfxRenderer& renderer) const {
  return renderer.getScreenHeight() - (touch_ ? 6 : lab::kBottomReserve + 6);
}

void Mini::load() {
  uint8_t bytes[kMaxOptions] = {};
  int got = 0;
  HalFile file;
  if (Storage.exists(this->file()) && Storage.openFileForRead("ILYA", this->file(), file)) {
    got = file.read(bytes, sizeof(bytes));
  }
  unpackOptions(bytes, got < 0 ? 0 : got, options(), optionCount(), values_);
}

void Mini::save() {
  HalFile file;
  if (!Storage.openFileForWrite("ILYA", this->file(), file)) return;
  uint8_t bytes[kMaxOptions] = {};
  packOptions(values_, optionCount(), bytes);
  file.write(bytes, static_cast<size_t>(optionCount()));
  file.flush();
}

void Mini::open(kids::Rng& rng, const bool touch) {
  rng_ = &rng;
  touch_ = touch;
  playing_ = false;
  stars_ = 0;
  load();
}

bool Mini::back() {
  if (!playing_) return false;
  playing_ = false;
  return true;
}

Did Mini::action(const fui::ActionId action) {
  if (playing_) return Did::Nothing;
  if (action == ActionPlay) {
    stars_ = 0;
    start(0);
    playing_ = true;
    return Did::Clean;
  }
  const int index = static_cast<int>(action) - static_cast<int>(ActionOption);
  if (index < 0 || index >= optionCount()) return Did::Nothing;
  values_[index] = static_cast<uint8_t>((values_[index] + 1) % options()[index].count);
  save();
  return Did::Paint;
}

Did Mini::tap(GfxRenderer& renderer, const int x, const int y, const uint32_t now) {
  return playing_ ? onTap(renderer, x, y, now) : Did::Nothing;
}

Did Mini::tick(const uint32_t now) { return playing_ ? onTick(now) : Did::Nothing; }

void Mini::draw(toybox::Screen& screen, GfxRenderer& renderer) {
  if (playing_) {
    drawPlay(renderer);
    return;
  }
  // The page of choices: each row steps to its next value when touched.
  lab::chrome(screen, title());
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - 2 * toybox::kMargin;
  int y = toybox::kChromeHeight + 14;
  const int rowH = 78;
  for (int i = 0; i < optionCount(); ++i) {
    const Option& option = options()[i];
    lab::button(screen, lab::rect(x, y, inner, rowH), "", static_cast<fui::ActionId>(ActionOption + i), 0, false,
                false);
    lab::caption(screen, x + 18, y + 12, inner - 36, option.name);
    lab::value(screen, x + 18, y + 12 + toybox::kTileCut.inkHeight + 10, inner - 36, option.values[values_[i]]);
    y += rowH + 10;
  }
  const int playH = 96;
  const int playY = bottom(renderer) - playH;
  if (playY - y >= 110) picture(renderer, face(), width / 2, y + (playY - y) / 2, 1);
  lab::button(screen, lab::rect(x, playY, inner, playH), "JUGAR", ActionPlay, 0, true, false);
}

}  // namespace kidsmoreui

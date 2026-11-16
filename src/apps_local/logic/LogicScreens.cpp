#include "LogicScreens.h"

#include <cstdio>

#include "../ui/ToyboxFormat.h"

namespace logicui {

namespace {

namespace lg = logic;

constexpr int kBottomReserve = 40;  // the button hints
constexpr int kRadius = toybox::kPillRadius / 2;

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);
const fui::Paint kShade = fui::Paint::dither(fui::Color::LightGray);
const fui::Paint kFaint = fui::Paint::dither(fui::Color::DarkGray);

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

void box(toybox::Screen& screen, const int x, const int y, const int w, const int h, const fui::Paint& paint,
         const int radius = 0) {
  if (w <= 0 || h <= 0) return;
  screen.target().fill(rect(x, y, w, h), paint, static_cast<uint8_t>(radius));
}

void label(toybox::Screen& screen, const fui::Rect& r, const char* text, const fui::FontId font,
           const toybox::CutMetrics& cut, const fui::TextAlign align = fui::TextAlign::Center,
           const fui::Color color = fui::Color::Black) {
  fui::TextStyle s;
  s.font = font;
  s.align = align;
  s.color = color;
  s.maxLines = 1;
  screen.target().text(toybox::inkCentred(r, cut), text, s);
}

void caption(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
             const fui::TextAlign align = fui::TextAlign::Left) {
  label(screen, rect(x, y, w, toybox::kTileCut.inkHeight), text, toybox::kTileFont, toybox::kTileCut, align);
}

void disc(toybox::Screen& screen, const int cx, const int cy, const int r, const fui::Color c) {
  if (r > 0) toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r), c);
}

// A number centred in a cell: the body cut where the cell has room for it.
void number(toybox::Screen& screen, const int cx, const int cy, const int cell, const int value,
            const fui::Color colour = fui::Color::Black) {
  char text[toybox::kIntChars + 1];
  std::snprintf(text, sizeof(text), "%d", value);
  const bool big = cell >= 40;
  const toybox::CutMetrics& cut = big ? toybox::kUiCut : toybox::kTileCut;
  label(screen, rect(cx - cell / 2, cy - cut.inkHeight / 2, cell, cut.inkHeight), text,
        big ? toybox::kUiFont : toybox::kTileFont, cut, fui::TextAlign::Center, colour);
}

void chrome(toybox::Screen& screen, const char* title, const char* right) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = right;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kSmallFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  header.sidePadding = screen.theme().headerSidePadding;
  toybox::headerBand(screen, header);
}

// The faint lattice the cell games sit on, and its frame.
void lattice(toybox::Screen& screen, const Geometry& g) {
  const int size = g.cell * g.side;
  for (int i = 1; i < g.side; ++i) {
    box(screen, g.x + i * g.cell, g.y, 1, size, kFaint);
    box(screen, g.x, g.y + i * g.cell, size, 1, kFaint);
  }
  screen.target().stroke(rect(g.x - 2, g.y - 2, size + 4, size + 4), kInk, 2);
}

void drawSlither(toybox::Screen& screen, const Geometry& g, const lg::Slither& s) {
  const int w = s.b.w;
  const int h = s.b.h;
  const int t = g.cell >= 56 ? 6 : 5;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const char ch = s.b.at(x, y);
      if (ch == '.') continue;
      // A clue with too many lines round it is drawn inverted: the one mistake
      // the board can point at without giving the answer away.
      const int cx = g.x + x * g.cell + g.cell / 2;
      const int cy = g.y + y * g.cell + g.cell / 2;
      const bool over = s.linesAround(x, y) > ch - '0';
      if (over) disc(screen, cx, cy, g.cell / 2 - t - 2, fui::Color::Black);
      number(screen, cx, cy, g.cell, ch - '0', over ? fui::Color::White : fui::Color::Black);
    }
  }
  auto cross = [&](const int cx, const int cy) {
    for (int k = -4; k <= 4; ++k) {
      box(screen, cx + k - 1, cy + k - 1, 2, 2, kInk);
      box(screen, cx + k - 1, cy - k - 1, 2, 2, kInk);
    }
  };
  for (int y = 0; y <= h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint8_t e = s.hz[y * w + x];
      const int px = g.x + x * g.cell;
      const int py = g.y + y * g.cell;
      if (e == lg::Slither::Line) box(screen, px, py - t / 2, g.cell, t, kInk, t / 2);
      if (e == lg::Slither::Cross) cross(px + g.cell / 2, py);
    }
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x <= w; ++x) {
      const uint8_t e = s.vt[y * (w + 1) + x];
      const int px = g.x + x * g.cell;
      const int py = g.y + y * g.cell;
      if (e == lg::Slither::Line) box(screen, px - t / 2, py, t, g.cell, kInk, t / 2);
      if (e == lg::Slither::Cross) cross(px, py + g.cell / 2);
    }
  }
  for (int y = 0; y <= h; ++y) {
    for (int x = 0; x <= w; ++x) disc(screen, g.x + x * g.cell, g.y + y * g.cell, 3, fui::Color::Black);
  }
}

void drawShikaku(toybox::Screen& screen, const Geometry& g, const lg::Shikaku& s) {
  // Finished rectangles are shaded, so what is left to cut stands out white.
  for (int i = 0; i < s.count; ++i) {
    const lg::Shikaku::Rect& r = s.rects[i];
    if (s.good(i)) box(screen, g.x + r.x * g.cell, g.y + r.y * g.cell, r.w * g.cell, r.h * g.cell, kShade);
  }
  lattice(screen, g);
  for (int i = 0; i < s.count; ++i) {
    const lg::Shikaku::Rect& r = s.rects[i];
    screen.target().stroke(rect(g.x + r.x * g.cell + 2, g.y + r.y * g.cell + 2, r.w * g.cell - 3, r.h * g.cell - 3),
                           kInk, 4, 6);
  }
  for (int y = 0; y < s.b.h; ++y) {
    for (int x = 0; x < s.b.w; ++x) {
      const char ch = s.b.at(x, y);
      if (ch == '.') continue;
      const int cx = g.x + x * g.cell + g.cell / 2;
      const int cy = g.y + y * g.cell + g.cell / 2;
      disc(screen, cx, cy, g.cell / 2 - 7, fui::Color::White);
      number(screen, cx, cy, g.cell, ch - '0');
    }
  }
  if (s.anchorX >= 0) {
    toybox::bracket(screen, rect(g.x + s.anchorX * g.cell + 3, g.y + s.anchorY * g.cell + 3, g.cell - 5, g.cell - 5),
                    static_cast<int16_t>(g.cell / 3), 5);
  }
}

void drawAkari(toybox::Screen& screen, const Geometry& g, const lg::Akari& a) {
  for (int y = 0; y < a.b.h; ++y) {
    for (int x = 0; x < a.b.w; ++x) {
      const int px = g.x + x * g.cell;
      const int py = g.y + y * g.cell;
      const char ch = a.b.at(x, y);
      if (ch != '.') {
        box(screen, px, py, g.cell + 1, g.cell + 1, kInk);
        if (ch != '#') number(screen, px + g.cell / 2, py + g.cell / 2, g.cell, ch - '0', fui::Color::White);
      } else if (!a.lit(x, y)) {
        // Dark is shaded and light is paper: the board is done when it is white.
        box(screen, px, py, g.cell, g.cell, kShade);
      }
    }
  }
  lattice(screen, g);
  for (int y = 0; y < a.b.h; ++y) {
    for (int x = 0; x < a.b.w; ++x) {
      const uint8_t m = a.mark[y * a.b.w + x];
      const int cx = g.x + x * g.cell + g.cell / 2;
      const int cy = g.y + y * g.cell + g.cell / 2;
      const int r = g.cell * 30 / 100;
      if (m == lg::Akari::Bulb) {
        // A bulb is a white disc with a black heart; one that another can see
        // loses the heart and gains a bar through it.
        disc(screen, cx, cy, r + 2, fui::Color::Black);
        disc(screen, cx, cy, r - 2, fui::Color::White);
        if (a.clash(x, y)) {
          box(screen, cx - r - 4, cy - 2, r * 2 + 8, 5, kInk);
        } else {
          disc(screen, cx, cy, r - 7, fui::Color::Black);
        }
      } else if (m == lg::Akari::Dot) {
        box(screen, cx - 3, cy - 3, 7, 7, kInk);
      }
    }
  }
}

void drawHashi(toybox::Screen& screen, const Geometry& g, const lg::Hashi& h) {
  auto centre = [&](const lg::Hashi::Island& i, int& cx, int& cy) {
    cx = g.x + i.x * g.cell + g.cell / 2;
    cy = g.y + i.y * g.cell + g.cell / 2;
  };
  for (int e = 0; e < h.edgeCount; ++e) {
    const lg::Hashi::Edge& edge = h.edges[e];
    int ax = 0, ay = 0, bx = 0, by = 0;
    centre(h.islands[edge.a], ax, ay);
    centre(h.islands[edge.b], bx, by);
    const bool flat = h.horizontal(e);
    if (edge.count == 0) {
      // Where a bridge could go, as a row of specks: the board is a map of
      // possible bridges before it is a puzzle.
      if (flat) {
        for (int x = ax; x < bx; x += 8) box(screen, x, ay, 2, 1, kInk);
      } else {
        for (int y = ay; y < by; y += 8) box(screen, ax, y, 1, 2, kInk);
      }
      continue;
    }
    const int t = 4;
    const int off = edge.count == 2 ? 5 : 0;
    for (int k = 0; k < edge.count; ++k) {
      const int d = edge.count == 2 ? (k == 0 ? -off : off) : 0;
      if (flat) {
        box(screen, ax, ay + d - t / 2, bx - ax, t, kInk);
      } else {
        box(screen, ax + d - t / 2, ay, t, by - ay, kInk);
      }
    }
  }
  const int r = g.cell * 44 / 100;
  for (int i = 0; i < h.islandCount; ++i) {
    int cx = 0, cy = 0;
    centre(h.islands[i], cx, cy);
    const int sum = h.sumOf(i);
    const bool full = sum == h.islands[i].n;
    disc(screen, cx, cy, r, fui::Color::Black);
    if (!full) disc(screen, cx, cy, r - 3, fui::Color::White);
    // Too many bridges: a second ring, so it cannot pass for finished.
    if (sum > h.islands[i].n) {
      disc(screen, cx, cy, r - 6, fui::Color::Black);
      disc(screen, cx, cy, r - 8, fui::Color::White);
    }
    number(screen, cx, cy, g.cell, h.islands[i].n, full ? fui::Color::White : fui::Color::Black);
  }
}

constexpr int kPickerCols = 6;
constexpr int kPickerGap = 8;

fui::Rect pickerTile(const fui::DeviceContext& device, const int n) {
  const int inner = device.width - toybox::kMargin * 2;
  const int tile = (inner - kPickerGap * (kPickerCols - 1)) / kPickerCols;
  const int top = toybox::kChromeHeight + 14 + toybox::kTileCut.lineHeight * 3 + 8 + toybox::kPillHeight + 14;
  return rect(toybox::kMargin + (n % kPickerCols) * (tile + kPickerGap), top + (n / kPickerCols) * (tile + kPickerGap),
              tile, tile);
}

}  // namespace

int pickerBoardAt(const fui::DeviceContext& device, const int x, const int y) {
  for (int n = 0; n < lg::boardsPerSize(); ++n) {
    const fui::Rect r = pickerTile(device, n);
    if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return n;
  }
  return -1;
}

Geometry boardGeometry(const fui::DeviceContext& device, const int side) {
  Geometry g;
  g.side = side;
  const int room = device.width - toybox::kMargin * 2;
  g.cell = room / side;
  g.x = (device.width - g.cell * side) / 2;
  g.y = toybox::kChromeHeight + 52;
  return g;
}

void buildPicker(toybox::Screen& screen, const PickerModel& model) {
  chrome(screen, lg::gameName(model.game), nullptr);
  const fui::DeviceContext device = screen.device();
  const int inner = device.width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 14;
  fui::TextStyle rule;
  rule.font = toybox::kTileFont;
  rule.align = fui::TextAlign::Left;
  rule.maxLines = 3;
  screen.target().text(rect(toybox::kMargin, y, inner, toybox::kTileCut.lineHeight * 3), lg::gameRule(model.game),
                       rule);
  y += toybox::kTileCut.lineHeight * 3 + 8;

  const int gap = 8;
  const int tabW = (inner - gap * 2) / 3;
  for (int i = 0; i < lg::kSizes; ++i) {
    char text[2 * toybox::kIntChars + 4];
    const int side = lg::sideOf(model.game, i);
    std::snprintf(text, sizeof(text), "%d X %d", side, side);
    fui::ButtonProps tab;
    tab.label = text;
    tab.action = ActionSize;
    tab.value = static_cast<int16_t>(i);
    tab.styles = toybox::rowStyles();
    tab.radius = kRadius;
    tab.state = i == model.sizeIndex ? fui::StateSelected : fui::StateNormal;
    screen.button(tab, rect(toybox::kMargin + i * (tabW + gap), y, tabW, toybox::kPillHeight));
  }
  y += toybox::kPillHeight + 14;

  // Thirty boards, six across. Done ones are solid; the last one opened wears
  // a bracket so there is somewhere to pick up from.
  const int total = lg::boardsPerSize();
  int done = 0;
  for (int n = 0; n < total; ++n) {
    const bool solved = model.solved && ((*model.solved >> n) & 1u);
    done += solved ? 1 : 0;
    const fui::Rect r = pickerTile(device, n);
    if (solved) {
      box(screen, r.x, r.y, r.width, r.height, kInk, kRadius);
    } else {
      screen.target().stroke(r, kInk, 1, kRadius);
    }
    char text[toybox::kIntChars + 1];
    std::snprintf(text, sizeof(text), "%d", n + 1);
    label(screen, rect(r.x, r.y + (r.height - toybox::kUiCut.inkHeight) / 2, r.width, toybox::kUiCut.inkHeight), text,
          toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Center, solved ? fui::Color::White : fui::Color::Black);
    if (n == model.lastOpened && !solved) {
      toybox::bracket(screen, rect(r.x + 4, r.y + 4, r.width - 8, r.height - 8), 12, 3);
    }
  }
  const int cols = kPickerCols;
  const int tile = pickerTile(device, 0).width;
  y = pickerTile(device, 0).y;
  y += ((total + cols - 1) / cols) * (tile + kPickerGap) + 8;
  char progress[2 * toybox::kIntChars + 16];
  std::snprintf(progress, sizeof(progress), "%d OF %d SOLVED", done, total);
  caption(screen, toybox::kMargin, y, inner, progress, fui::TextAlign::Center);
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  const int side = lg::sideOf(model.game, model.sizeIndex);
  char right[3 * toybox::kIntChars + 8];
  std::snprintf(right, sizeof(right), "%dX%d  #%d", side, side, model.number + 1);
  chrome(screen, lg::gameName(model.game), right);
  const fui::DeviceContext device = screen.device();
  const int inner = device.width - toybox::kMargin * 2;
  const Geometry g = boardGeometry(device, side);

  // One line over the board: how to touch it, or that it is done.
  const char* hint = "";
  switch (model.game) {
    case lg::Game::Slither:
      hint = "TAP BETWEEN DOTS: LINE, CROSS, NOTHING";
      break;
    case lg::Game::Shikaku:
      hint = "TAP TWO CORNERS. TAP A RECTANGLE TO REMOVE IT";
      break;
    case lg::Game::Akari:
      hint = "TAP A CELL: BULB, DOT, NOTHING";
      break;
    case lg::Game::Hashi:
      hint = "TAP BETWEEN ISLANDS: ONE, TWO, NONE";
      break;
  }
  const int hintY = toybox::kChromeHeight + 18;
  if (model.solved) {
    box(screen, toybox::kMargin, hintY - 8, inner, toybox::kTileCut.inkHeight + 16, kInk, 8);
    label(screen, rect(toybox::kMargin, hintY, inner, toybox::kTileCut.inkHeight), "SOLVED", toybox::kTileFont,
          toybox::kTileCut, fui::TextAlign::Center, fui::Color::White);
  } else {
    caption(screen, toybox::kMargin, hintY, inner, hint, fui::TextAlign::Center);
  }

  switch (model.game) {
    case lg::Game::Slither:
      if (model.slither) drawSlither(screen, g, *model.slither);
      break;
    case lg::Game::Shikaku:
      if (model.shikaku) drawShikaku(screen, g, *model.shikaku);
      break;
    case lg::Game::Akari:
      if (model.akari) drawAkari(screen, g, *model.akari);
      break;
    case lg::Game::Hashi:
      if (model.hashi) drawHashi(screen, g, *model.hashi);
      break;
  }

  const int gap = 10;
  const int half = (inner - gap) / 2;
  const int by = device.height - kBottomReserve - toybox::kPillHeight - 8;
  fui::ButtonProps reset;
  reset.label = model.resetArmed ? "SURE?" : "RESET";
  reset.action = ActionReset;
  reset.styles = toybox::rowStyles();
  reset.text = toybox::buttonText(screen.theme());
  reset.radius = kRadius;
  reset.state = model.resetArmed ? fui::StateSelected : fui::StateNormal;
  screen.button(reset, rect(toybox::kMargin, by, half, toybox::kPillHeight));
  fui::ButtonProps next;
  next.label = "NEXT BOARD";
  next.action = ActionNext;
  next.styles = toybox::rowStyles();
  next.text = toybox::buttonText(screen.theme());
  next.radius = kRadius;
  next.state = model.solved ? fui::StateSelected : fui::StateNormal;
  screen.button(next, rect(toybox::kMargin + half + gap, by, half, toybox::kPillHeight));
}

}  // namespace logicui

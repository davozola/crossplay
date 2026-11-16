#include "MoleScreens.h"

#include <cmath>
#include <cstdio>

#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "MoleArt.h"

namespace moleui {

namespace lab = toyboxlab;

namespace {

static_assert(moleart::kAnimals == mole::kAnimals, "one picture for every animal in MoleCore");

constexpr int kBottomReserve = 40;  // the button hints
constexpr int kStarRowH = 46;
constexpr int kRadius = 18;

// A picture with its top left corner at (x, y), one fill a run.
void draw(GfxRenderer& renderer, const moleart::Picture& picture, const int x, const int y) {
  const uint8_t* p = picture.rows;
  for (int row = 0; row < picture.size; ++row) {
    const int runs = *p++;
    int at = 0;
    bool ink = false;
    for (int i = 0; i < runs; ++i) {
      const int run = p[i];
      if (ink && run > 0) renderer.fillRect(x + at, y + row, run, 1, true);
      at += run;
      // Every run flips the colour; a long one is 255, then 0 to flip back.
      ink = !ink;
    }
    p += runs;
  }
}

// A five-pointed star; hollow is an outline of the same size.
void star(GfxRenderer& renderer, const int cx, const int cy, const int r, const bool solid, const int rotDeg = 0) {
  auto points = [&](const int radius, int* xs, int* ys) {
    for (int i = 0; i < 10; ++i) {
      const float a = (-90.0f + static_cast<float>(rotDeg) + static_cast<float>(i) * 36.0f) * 0.01745329f;
      const float rad = static_cast<float>(radius) * (i % 2 == 0 ? 1.0f : 0.45f);
      xs[i] = cx + static_cast<int>(std::lround(std::cos(a) * rad));
      ys[i] = cy + static_cast<int>(std::lround(std::sin(a) * rad));
    }
  };
  int xs[10];
  int ys[10];
  points(r, xs, ys);
  renderer.fillPolygon(xs, ys, 10, true);
  if (!solid) {
    points(r - 5, xs, ys);
    renderer.fillPolygon(xs, ys, 10, false);
  }
}

void disc(GfxRenderer& renderer, const int cx, const int cy, const int r, const bool ink) {
  renderer.fillRoundedRect(cx - r, cy - r, r * 2, r * 2, r, ink ? Color::Black : Color::White);
}

void sun(GfxRenderer& renderer, const int cx, const int cy, const int r) {
  for (int i = 0; i < 12; ++i) {
    const float a = static_cast<float>(i) * 0.5235988f;
    const float inner = static_cast<float>(r) + 8.0f;
    const float outer = static_cast<float>(r) + (i % 2 == 0 ? 22.0f : 15.0f);
    renderer.drawLine(cx + static_cast<int>(std::cos(a) * inner), cy + static_cast<int>(std::sin(a) * inner),
                      cx + static_cast<int>(std::cos(a) * outer), cy + static_cast<int>(std::sin(a) * outer), 4, true);
  }
  disc(renderer, cx, cy, r, true);
  disc(renderer, cx, cy, r - 4, false);
}

// Three bumps on a flat base; `u` is the size of one step.
void cloud(GfxRenderer& renderer, const int cx, const int cy, const int u) {
  const int bumps[3][3] = {{-2 * u, u / 2, 2 * u}, {0, -u, 3 * u}, {5 * u / 2, u / 2, 2 * u}};
  for (int pass = 0; pass < 2; ++pass) {
    const int grow = pass == 0 ? 0 : -4;
    for (const auto& b : bumps) disc(renderer, cx + b[0], cy + b[1], b[2] + grow, pass == 0);
    if (pass == 0) {
      renderer.fillRect(cx - 2 * u, cy + u / 2, 9 * u / 2, 2 * u, true);
    } else {
      renderer.fillRect(cx - 2 * u, cy + u / 2, 9 * u / 2, 2 * u - 4, false);
    }
  }
}

void tuft(GfxRenderer& renderer, const int x, const int y) {
  const int blades[4][2] = {{-12, -12}, {-4, -22}, {5, -17}, {13, -9}};
  for (const auto& b : blades) renderer.drawLine(x + b[0] / 2, y, x + b[0], y + b[1], 3, true);
}

// Text in one of the big cuts, ink top at `y`, centred on `cx`.
void bigText(GfxRenderer& renderer, const int fontId, const toybox::CutMetrics& cut, const int cx, const int y,
             const char* text, const bool black = true) {
  const int w = renderer.getTextWidth(fontId, text);
  renderer.drawText(fontId, cx - w / 2, y - (cut.ascender - cut.inkHeight), text, black);
}

// One choice on the setup page: its name on the left, what it is set to on
// the right, the whole row the target.
void choice(toybox::Screen& screen, GfxRenderer& renderer, const fui::Rect& r, const char* name, const char* value,
            const fui::ActionId action) {
  fui::ButtonProps b;
  b.label = "";
  b.action = action;
  b.styles = toybox::rowStyles();
  b.styles.normal.borderWidth = 0;
  b.minTouchSize = 0;
  screen.button(b, r);
  screen.target().stroke(r, fui::Paint::solid(fui::Color::Black), 3, kRadius);
  lab::value(screen, r.x + 20, r.y + (r.height - toybox::kUiCut.inkHeight) / 2, 200, name, fui::TextAlign::Left,
             fui::Color::DarkGray);
  const int w = renderer.getTextWidth(toybox::kDisplayFontId, value);
  renderer.drawText(toybox::kDisplayFontId, r.x + r.width - 22 - w,
                    r.y + (r.height - toybox::kDisplayCut.inkHeight) / 2 -
                        (toybox::kDisplayCut.ascender - toybox::kDisplayCut.inkHeight),
                    value, true);
}

}  // namespace

Meadow meadowFor(const mole::Config& config, const int width, const int height) {
  Meadow m;
  m.columns = mole::columns(config);
  m.rows = mole::rows(config);
  m.cell = config.holes == 2 ? 144 : 208;
  const int top = toybox::kChromeHeight + 6;
  const int bottom = height - kBottomReserve - kStarRowH;
  m.stepX = width / m.columns;
  m.stepY = (bottom - top) / m.rows;
  // Four holes would leave the page half empty: close the rows up a little.
  if (m.stepY > m.cell + 70) m.stepY = m.cell + 70;
  m.left = (width - m.stepX * m.columns) / 2;
  m.top = top + ((bottom - top) - m.stepY * m.rows) / 2;
  return m;
}

int holeAt(const Meadow& m, const int x, const int y) {
  if (x < m.left || y < m.top) return -1;
  const int col = (x - m.left) / m.stepX;
  const int row = (y - m.top) / m.stepY;
  if (col >= m.columns || row >= m.rows) return -1;
  return row * m.columns + col;
}

void drawIcon(GfxRenderer& renderer, const int cx, const int cy) {
  draw(renderer, moleart::kIcon, cx - moleart::kIcon.size / 2, cy - moleart::kIcon.size / 2);
}

void buildSetup(toybox::Screen& screen, GfxRenderer& renderer, const mole::Config& config) {
  lab::chrome(screen, "EL TALP");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = screen.device().height - kBottomReserve - 6;

  // Who is coming out to play.
  const int heroTop = toybox::kChromeHeight + 2;
  if (config.who < mole::kAnimals) {
    draw(renderer, moleart::kUp208[config.who], width / 2 - 104, heroTop);
  } else {
    const int trio[3] = {1, 0, 3};
    for (int i = 0; i < 3; ++i) draw(renderer, moleart::kUp144[trio[i]], 24 + i * 144, heroTop + 50);
  }

  int y = heroTop + 208 + 6;
  const int rowH = 58;
  const int gap = 8;
  choice(screen, renderer, lab::rect(x, y, inner, rowH), "FORATS", mole::holesName(config), ActionHoles);
  y += rowH + gap;
  choice(screen, renderer, lab::rect(x, y, inner, rowH), "QUI SURT", mole::whoName(config), ActionWho);
  y += rowH + gap;
  choice(screen, renderer, lab::rect(x, y, inner, rowH), "S'AMAGA", mole::hidesName(config), ActionHides);
  y += rowH + gap;
  choice(screen, renderer, lab::rect(x, y, inner, rowH), "SO", config.sound ? "ACTIVAT" : "APAGAT", ActionSound);
  y += rowH + gap + 4;

  // And the one button the player presses.
  const fui::Rect play = lab::rect(x, y, inner, bottom - y);
  fui::ButtonProps b;
  b.label = "";
  b.action = ActionPlay;
  b.styles = toybox::rowStyles();
  b.styles.normal.borderWidth = 0;
  b.minTouchSize = 0;
  screen.button(b, play);
  lab::box(screen, play.x, play.y, play.width, play.height, fui::Paint::solid(fui::Color::Black), kRadius);
  bigText(renderer, toybox::kLargeFontId, toybox::kLargeCut, width / 2,
          play.y + (play.height - toybox::kLargeCut.inkHeight) / 2, "JUGA!", false);
}

void drawMeadow(toybox::Screen& screen, GfxRenderer& renderer, const mole::Game& game) {
  lab::chrome(screen, "EL TALP");
  // The stars so far, in the band, large enough to read from a lap.
  if (game.stars > 0) {
    char count[toybox::kIntChars + 1];
    std::snprintf(count, sizeof(count), "%d", game.stars);
    const int w = renderer.getTextWidth(toybox::kDisplayFontId, count);
    const int right = screen.device().width - toybox::kMargin;
    const int ty = (toybox::kChromeHeight - toybox::kDisplayCut.inkHeight) / 2 - 2;
    renderer.drawText(toybox::kDisplayFontId, right - w,
                      ty - (toybox::kDisplayCut.ascender - toybox::kDisplayCut.inkHeight), count, false);
    int xs[10];
    int ys[10];
    for (int i = 0; i < 10; ++i) {
      const float a = (-90.0f + static_cast<float>(i) * 36.0f) * 0.01745329f;
      const float rad = i % 2 == 0 ? 17.0f : 7.6f;
      xs[i] = right - w - 28 + static_cast<int>(std::lround(std::cos(a) * rad));
      ys[i] = ty + toybox::kDisplayCut.inkHeight / 2 + static_cast<int>(std::lround(std::sin(a) * rad));
    }
    renderer.fillPolygon(xs, ys, 10, false);
  }

  const int width = screen.device().width;
  const int height = screen.device().height;
  const Meadow m = meadowFor(game.config, width, height);
  const bool small = m.cell == 144;

  // A sky, where the meadow leaves room for one.
  const int sky = m.top + (m.stepY - m.cell) / 2 - toybox::kChromeHeight;
  if (sky >= 70) {
    const int sy = toybox::kChromeHeight + sky / 2 + 6;
    sun(renderer, width - 74, sy, 24);
    cloud(renderer, 96, sy + 4, 10);
    cloud(renderer, 262, sy - 12, 7);
  }
  // And grass between the rows.
  if (m.stepY - m.cell >= 60) {
    for (int row = 1; row < m.rows; ++row) {
      const int gy = m.top + row * m.stepY - 4;
      tuft(renderer, 56, gy + 6);
      tuft(renderer, width / 2 + 6, gy - 8);
      tuft(renderer, width - 70, gy + 10);
    }
  }

  for (int i = 0; i < mole::holeCount(game.config); ++i) {
    const mole::Hole& hole = game.holes[i];
    const int cx = m.left + (i % m.columns) * m.stepX + m.stepX / 2;
    const int cy = m.top + (i / m.columns) * m.stepY + m.stepY / 2;
    const moleart::Picture* picture = small ? &moleart::kHole144 : &moleart::kHole208;
    if (hole.state == mole::State::Up) {
      picture = small ? &moleart::kUp144[hole.animal] : &moleart::kUp208[hole.animal];
    } else if (hole.state == mole::State::Bopped) {
      picture = small ? &moleart::kBopped144[hole.animal] : &moleart::kBopped208[hole.animal];
    }
    draw(renderer, *picture, cx - m.cell / 2, cy - m.cell / 2);
  }

  // The way to the next celebration: ten stars, filled as they are won.
  const int won = game.stars % mole::kPartyEvery;
  const int step = 42;
  const int sy = height - kBottomReserve - kStarRowH / 2;
  int sx = width / 2 - (mole::kPartyEvery - 1) * step / 2;
  for (int i = 0; i < mole::kPartyEvery; ++i) {
    star(renderer, sx, sy, 17, i < won);
    sx += step;
  }
}

void drawParty(toybox::Screen& screen, GfxRenderer& renderer, const mole::Game& game) {
  const int width = screen.device().width;
  const int height = screen.device().height;
  const int cx = width / 2;
  // The biggest cut the words fit in.
  if (renderer.getTextWidth(toybox::kHugeFontId, "MOLT BE!") <= width - toybox::kMargin * 2) {
    bigText(renderer, toybox::kHugeFontId, toybox::kHugeCut, cx, 70, "MOLT BE!");
  } else {
    bigText(renderer, toybox::kLargeFontId, toybox::kLargeCut, cx, 84, "MOLT BE!");
  }

  // The one who took the tenth bop, none the worse for it, in a ring of stars.
  const int cy = 392;
  draw(renderer, moleart::kUp208[game.partyAnimal < mole::kAnimals ? game.partyAnimal : 0], cx - 104, cy - 104);
  for (int i = 0; i < 9; ++i) {
    const float a = (-90.0f + static_cast<float>(i - 4) * 33.0f) * 0.01745329f;
    const int r = i % 2 == 0 ? 30 : 20;
    star(renderer, cx + static_cast<int>(std::lround(std::cos(a) * 186.0f)),
         cy + 20 + static_cast<int>(std::lround(std::sin(a) * 186.0f)), r, i % 2 == 0, (i - 4) * 9);
  }

  // How many that makes.
  char count[toybox::kIntChars + 1];
  std::snprintf(count, sizeof(count), "%d", game.stars);
  const int w = renderer.getTextWidth(toybox::kHugeFontId, count);
  const int ty = height - kBottomReserve - 60 - toybox::kHugeCut.inkHeight;
  const int total = w + 24 + 80;
  renderer.drawText(toybox::kHugeFontId, cx - total / 2, ty - (toybox::kHugeCut.ascender - toybox::kHugeCut.inkHeight),
                    count, true);
  star(renderer, cx - total / 2 + w + 24 + 40, ty + toybox::kHugeCut.inkHeight / 2, 42, true);
}

}  // namespace moleui

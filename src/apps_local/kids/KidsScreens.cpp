#include "KidsScreens.h"

#include <cmath>
#include <cstdio>

#include "../ui/ToyboxFormat.h"
#include "KidsPictures.h"
#include "MoleScreens.h"

namespace kidsui {

namespace {

static_assert(sizeof(kidspictures::kAll) / sizeof(kidspictures::kAll[0]) == kids::kPictures,
              "one picture for every word in KidsCore");

constexpr int kBottomReserve = 40;  // the button hints
constexpr int kRadius = 18;

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

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

int textWidth(toybox::Screen& screen, const char* text) {
  fui::TextStyle s;
  s.font = toybox::kDisplayFont;
  return screen.target().measureText(toybox::kDisplayFont, text, s).width;
}

// A picture centred on (cx, cy), every source pixel a `scale` square. Drawn in
// runs, so a 96px picture is a few hundred fills at any scale.
void picture(toybox::Screen& screen, const int index, const int cx, const int cy, const int scale,
             const fui::Paint& paint) {
  if (index < 0 || index >= kids::kPictures) return;
  const freeink::Icon& icon = *kidspictures::kAll[index];
  const int rowBytes = (icon.w + 7) / 8;
  const int x0 = cx - icon.w * scale / 2;
  const int y0 = cy - icon.h * scale / 2;
  for (int y = 0; y < icon.h; ++y) {
    const uint8_t* row = icon.bits + y * rowBytes;
    int run = 0;
    for (int x = 0; x <= icon.w; ++x) {
      const bool ink = x < icon.w && (row[x / 8] & (0x80u >> (x % 8))) == 0;
      if (ink) {
        ++run;
      } else if (run > 0) {
        box(screen, x0 + (x - run) * scale, y0 + y * scale, run * scale, scale, paint);
        run = 0;
      }
    }
  }
}

// Text in the biggest of the lent cuts that fits `maxW`, ink centred in `r`.
void bigText(toybox::Screen& screen, const FaceSwap& faces, Face face, const fui::Rect& r, const char* text,
             const fui::Color colour = fui::Color::Black) {
  if (!faces.bindTitle) face = Face::Display;
  for (;;) {
    if (faces.bindTitle) faces.bindTitle(faces.ctx, face);
    if (textWidth(screen, text) <= r.width || face == Face::Display) break;
    face = face == Face::Huge ? Face::Large : Face::Display;
  }
  const toybox::CutMetrics& cut =
      face == Face::Huge ? toybox::kHugeCut : (face == Face::Large ? toybox::kLargeCut : toybox::kDisplayCut);
  label(screen, r, text, toybox::kDisplayFont, cut, fui::TextAlign::Center, colour);
  if (faces.bindTitle) faces.bindTitle(faces.ctx, Face::Display);
}

// A five-pointed star, radius `r`.
void star(toybox::Screen& screen, const int cx, const int cy, const int r, const fui::Paint& paint) {
  float xs[10];
  float ys[10];
  for (int i = 0; i < 10; ++i) {
    const float a = -1.5707963f + static_cast<float>(i) * 0.6283185f;
    const float rad = static_cast<float>(i % 2 == 0 ? r : r * 2 / 5);
    xs[i] = static_cast<float>(cx) + std::cos(a) * rad;
    ys[i] = static_cast<float>(cy) + std::sin(a) * rad;
  }
  // Scanline fill, even-odd.
  for (int y = cy - r; y <= cy + r; ++y) {
    const float sy = static_cast<float>(y) + 0.5f;
    float cross[10];
    int n = 0;
    for (int i = 0, j = 9; i < 10; j = i++) {
      if ((ys[i] <= sy && ys[j] > sy) || (ys[j] <= sy && ys[i] > sy)) {
        cross[n++] = xs[i] + (sy - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
      }
    }
    for (int a = 1; a < n; ++a) {
      for (int b = a; b > 0 && cross[b - 1] > cross[b]; --b) {
        const float t = cross[b];
        cross[b] = cross[b - 1];
        cross[b - 1] = t;
      }
    }
    for (int k = 0; k + 1 < n; k += 2) {
      const int a = static_cast<int>(std::lround(cross[k]));
      const int b = static_cast<int>(std::lround(cross[k + 1]));
      if (b > a) box(screen, a, y, b - a, 1, paint);
    }
  }
}

void chrome(toybox::Screen& screen, const char* title, const char* right = nullptr) {
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

// A tile that is all target: thick outline, or solid once it is the answer.
void tile(toybox::Screen& screen, const fui::Rect& r, const fui::ActionId action, const int value, const bool solid,
          const bool enabled) {
  fui::ButtonProps b;
  b.label = "";
  b.action = action;
  b.value = static_cast<int16_t>(value);
  b.enabled = enabled;
  b.styles = toybox::rowStyles();
  b.styles.normal.borderWidth = 0;
  b.styles.disabled = b.styles.normal;
  b.minTouchSize = 0;
  screen.button(b, r);
  if (solid) {
    box(screen, r.x, r.y, r.width, r.height, kInk, kRadius);
  } else if (enabled) {
    screen.target().stroke(r, kInk, 4, kRadius);
  } else {
    screen.target().stroke(r, fui::Paint::dither(fui::Color::DarkGray), 4, kRadius);
  }
}

// Stars won this visit, along the foot.
void starRow(toybox::Screen& screen, const int stars, const int y, const int width) {
  const int shown = stars > 10 ? 10 : stars;
  if (shown <= 0) return;
  const int step = 40;
  int x = width / 2 - (shown - 1) * step / 2;
  for (int i = 0; i < shown; ++i) {
    star(screen, x, y, 15, kInk);
    x += step;
  }
}

}  // namespace

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  chrome(screen, "ILYA");
  const fui::DeviceContext device = screen.device();
  const int inner = device.width - toybox::kMargin * 2;
  const int top = toybox::kChromeHeight + 12;
  const int bottom = device.height - kBottomReserve - 6;
  const int gap = 8;
  const int rowH = (bottom - top - gap * (kids::kGames - 1)) / kids::kGames;
  for (int i = 0; i < kids::kGames; ++i) {
    const kids::Game game = static_cast<kids::Game>(i);
    const fui::Rect r = rect(toybox::kMargin, top + i * (rowH + gap), inner, rowH);
    tile(screen, r, ActionGame, i, false, true);
    picture(screen, kids::gamePicture(game), r.x + 16 + 48, r.y + rowH / 2, 1, kInk);
    if (game == kids::Game::Mole && model.renderer)
      moleui::drawIcon(*model.renderer, r.x + 16 + 48, r.y + rowH / 2 - 2);
    label(screen, rect(r.x + 128, r.y, inner - 144, rowH), kids::gameName(game), toybox::kDisplayFont,
          toybox::kDisplayCut, fui::TextAlign::Left);
  }
}

void buildCard(toybox::Screen& screen, const CardModel& model) {
  char counter[toybox::kOfCounterChars];
  std::snprintf(counter, sizeof(counter), "%d / %d", model.index + 1, model.total);
  chrome(screen, model.letter ? "LLETRES" : "DIBUIXOS", counter);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int top = toybox::kChromeHeight;
  const int bottom = device.height - kBottomReserve;

  // The whole page turns the card: the left third back, the rest on.
  fui::ButtonProps prev;
  prev.label = "";
  prev.action = ActionPrevious;
  prev.styles = toybox::rowStyles();
  prev.styles.normal.borderWidth = 0;
  prev.minTouchSize = 0;
  screen.button(prev, rect(0, top + 4, width / 3, bottom - top - 4));
  fui::ButtonProps next = prev;
  next.action = ActionNext;
  screen.button(next, rect(width / 3, top + 4, width - width / 3, bottom - top - 4));

  const char* word = kids::wordOf(model.picture);
  if (model.letter) {
    // The letter, as big as the panel has, beside its picture; the word under
    // both with the letter it starts with picked out.
    const char one[2] = {model.letter, 0};
    const int rowCy = top + 200;
    box(screen, toybox::kMargin, rowCy - 110, 196, 220, kInk, kRadius);
    bigText(screen, model.faces, Face::Huge, rect(toybox::kMargin, rowCy - 54, 196, toybox::kHugeCut.inkHeight), one,
            fui::Color::White);
    picture(screen, model.picture, width - toybox::kMargin - 120, rowCy, 2, kInk);
    bigText(screen, model.faces, Face::Huge, rect(toybox::kMargin, rowCy + 190, width - toybox::kMargin * 2, 110),
            word);
    return;
  }
  const int cy = top + 20 + 192;
  picture(screen, model.picture, width / 2, cy, 4, kInk);
  bigText(screen, model.faces, Face::Huge, rect(toybox::kMargin, cy + 192 + 34, width - toybox::kMargin * 2, 110),
          word);
}

void buildRound(toybox::Screen& screen, const RoundModel& model) {
  const kids::Round& r = *model.round;
  chrome(screen, kids::gameName(model.game));
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int top = toybox::kChromeHeight;
  const int bottom = device.height - kBottomReserve;
  const int gap = 12;
  const int starsY = bottom - 24;

  if (model.game == kids::Game::Count) {
    // The things to count, three to a row, then three numbers.
    const int perRow = 3;
    const int rows = (r.count + perRow - 1) / perRow;
    const int cell = 136;
    const int areaTop = top + 16;
    const int areaH = 300;
    const int startY = areaTop + (areaH - rows * cell) / 2 + cell / 2;
    for (int i = 0; i < r.count; ++i) {
      const int row = i / perRow;
      const int inRow = row == rows - 1 ? r.count - row * perRow : perRow;
      const int col = i % perRow;
      const int x = width / 2 - (inRow - 1) * cell / 2 + col * cell;
      picture(screen, r.target, x, startY + row * cell, 1, kInk);
    }
    const int tileW = (inner - gap * 2) / 3;
    const int tileY = areaTop + areaH + 14;
    const int tileH = starsY - 30 - tileY;
    for (int i = 0; i < r.optionCount; ++i) {
      const bool right = r.solved && i == r.answer;
      const bool wrong = (r.wrongMask & (1u << i)) != 0;
      const fui::Rect t = rect(toybox::kMargin + i * (tileW + gap), tileY, tileW, tileH);
      tile(screen, t, ActionOption, i, right, !wrong && !r.solved);
      char n[toybox::kIntChars + 1];
      std::snprintf(n, sizeof(n), "%d", r.options[i]);
      if (wrong) {
        label(screen,
              rect(t.x, t.y + (tileH - toybox::kDisplayCut.inkHeight) / 2, tileW, toybox::kDisplayCut.inkHeight), n,
              toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Center, fui::Color::DarkGray);
      } else {
        bigText(screen, model.faces, Face::Huge,
                rect(t.x, t.y + (tileH - toybox::kHugeCut.inkHeight) / 2, tileW, toybox::kHugeCut.inkHeight), n,
                right ? fui::Color::White : fui::Color::Black);
      }
    }
    starRow(screen, model.stars, starsY, width);
    return;
  }

  // Find names the picture; Match shows it. Then four to choose from.
  int y = top + 12;
  int gridTop = 0;
  if (model.game == kids::Game::Find) {
    label(screen, rect(toybox::kMargin, y, inner, toybox::kTileCut.inkHeight), "ON ES?", toybox::kTileFont,
          toybox::kTileCut);
    bigText(screen, model.faces, Face::Huge, rect(toybox::kMargin, y + 30, inner, toybox::kHugeCut.inkHeight),
            kids::wordOf(r.target));
    gridTop = y + 30 + toybox::kHugeCut.inkHeight + 22;
  } else {
    const int frame = 150;
    box(screen, width / 2 - frame / 2, y, frame, frame, kInk, kRadius);
    box(screen, width / 2 - frame / 2 + 6, y + 6, frame - 12, frame - 12, kPaper, kRadius - 6);
    picture(screen, r.target, width / 2, y + frame / 2, 1, kInk);
    gridTop = y + frame + 14;
  }
  const int tileW = (inner - gap) / 2;
  const int gridBottom = starsY - 28;
  const int tileH = (gridBottom - gridTop - gap) / 2;
  const int scale = tileH >= 200 && tileW >= 200 ? 2 : 1;
  for (int i = 0; i < r.optionCount; ++i) {
    const bool right = r.solved && i == r.answer;
    const bool wrong = (r.wrongMask & (1u << i)) != 0;
    const fui::Rect t =
        rect(toybox::kMargin + (i % 2) * (tileW + gap), gridTop + (i / 2) * (tileH + gap), tileW, tileH);
    tile(screen, t, ActionOption, i, right, !wrong && !r.solved);
    const fui::Paint paint = right ? kPaper : (wrong ? fui::Paint::dither(fui::Color::LightGray) : kInk);
    picture(screen, r.options[i], t.x + tileW / 2, t.y + tileH / 2, scale, paint);
  }
  starRow(screen, model.stars, starsY, width);
}

}  // namespace kidsui

#include "TriplesScreens.h"

#include <cstdio>

#include "../ui/ToyboxFormat.h"

namespace triplesui {

namespace {

namespace tr = triples;

constexpr int kBottomReserve = 40;  // the button hints
constexpr int kRadius = 14;

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

void caption(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
             const fui::TextAlign align = fui::TextAlign::Left) {
  label(screen, rect(x, y, w, toybox::kTileCut.inkHeight), text, toybox::kTileFont, toybox::kTileCut, align);
}

// One tile. A 1 is solid, a 2 is grey, and everything they make is paper: the
// two halves of a 3 are the two things on the board that do not look like it.
void tile(toybox::Screen& screen, const FaceSwap& faces, const int x, const int y, const int size, const uint16_t value,
          const bool top, const bool fresh) {
  if (value == 0) {
    box(screen, x, y, size, size, fui::Paint::dither(fui::Color::LightGray), kRadius);
    box(screen, x + 2, y + 2, size - 4, size - 4, kPaper, kRadius - 2);
    return;
  }
  fui::Color ink = fui::Color::Black;
  if (value == 1) {
    box(screen, x, y, size, size, kInk, kRadius);
    ink = fui::Color::White;
  } else {
    box(screen, x, y, size, size, kInk, kRadius);
    const int edge = top ? 7 : 3;
    box(screen, x + edge, y + edge, size - edge * 2, size - edge * 2,
        value == 2 ? fui::Paint::dither(fui::Color::LightGray) : kPaper, kRadius - edge);
    if (value == 2) {
      // A paper plate under the figure, so it reads on the grey.
      toybox::disc(screen, static_cast<int16_t>(x + size / 2), static_cast<int16_t>(y + size / 2),
                   static_cast<int16_t>(size * 30 / 100), fui::Color::White);
    }
  }
  char text[toybox::kIntChars + 1];
  std::snprintf(text, sizeof(text), "%d", value);
  const bool small = size < 90;
  if (value < 100 && !small && faces.bindTitle) {
    faces.bindTitle(faces.ctx, true);
    label(screen, rect(x, y + (size - toybox::kLargeCut.inkHeight) / 2, size, toybox::kLargeCut.inkHeight), text,
          toybox::kDisplayFont, toybox::kLargeCut, fui::TextAlign::Center, ink);
    faces.bindTitle(faces.ctx, false);
  } else if (value < 1000 || small) {
    const bool body = small && value >= 100;
    const toybox::CutMetrics& cut = body ? toybox::kUiCut : toybox::kDisplayCut;
    label(screen, rect(x, y + (size - cut.inkHeight) / 2, size, cut.inkHeight), text,
          body ? toybox::kUiFont : toybox::kDisplayFont, cut, fui::TextAlign::Center, ink);
  } else {
    label(screen, rect(x, y + (size - toybox::kUiCut.inkHeight) / 2, size, toybox::kUiCut.inkHeight), text,
          toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Center, ink);
  }
  if (fresh) {
    // The tile that just came in: a notch in its corner, since nothing here
    // can slide into view.
    toybox::disc(screen, static_cast<int16_t>(x + size - 16), static_cast<int16_t>(y + 16), 6,
                 value == 1 ? fui::Color::White : fui::Color::Black);
  }
}

}  // namespace

void build(toybox::Screen& screen, const Model& model) {
  fui::HeaderProps header;
  header.title = "TRIPLES";
  header.borderEdges = fui::EdgesNone;
  header.sidePadding = screen.theme().headerSidePadding;
  toybox::headerBand(screen, header);
  if (!model.game) return;
  const tr::Game& g = *model.game;
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;

  // What comes in next, and the score.
  int y = toybox::kChromeHeight + 14;
  const int nextSize = 72;
  caption(screen, toybox::kMargin, y, 80, "NEXT");
  tile(screen, model.faces, toybox::kMargin, y + toybox::kTileCut.inkHeight + 8, nextSize, g.nextTile, false, false);
  char score[toybox::kIntChars + 1];
  std::snprintf(score, sizeof(score), "%lu", static_cast<unsigned long>(g.score()));
  caption(screen, width / 2, y, inner / 2, "SCORE", fui::TextAlign::Right);
  label(screen,
        rect(width / 3, y + toybox::kTileCut.inkHeight + 12, inner - width / 3 + toybox::kMargin,
             toybox::kDisplayCut.inkHeight),
        score, toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Right);
  char best[toybox::kIntChars + 8];
  std::snprintf(best, sizeof(best), "BEST %lu", static_cast<unsigned long>(model.best));
  caption(screen, width / 2, y + toybox::kTileCut.inkHeight + 12 + toybox::kDisplayCut.inkHeight + 12, inner / 2, best,
          fui::TextAlign::Right);
  y += toybox::kTileCut.inkHeight + 8 + nextSize + 14;

  // The board.
  const int gap = 8;
  const int size = (inner - gap * (tr::kSide - 1)) / tr::kSide;
  const int boardX = (width - (size * tr::kSide + gap * (tr::kSide - 1))) / 2;
  const uint16_t top = g.highest();
  for (int cy = 0; cy < tr::kSide; ++cy) {
    for (int cx = 0; cx < tr::kSide; ++cx) {
      const uint16_t v = g.at(cx, cy);
      tile(screen, model.faces, boardX + cx * (size + gap), y + cy * (size + gap), size, v, v >= 6 && v == top,
           model.entered == cy * tr::kSide + cx);
    }
  }
  y += tr::kSide * (size + gap) + 6;

  const int by = device.height - kBottomReserve - toybox::kPillHeight - 6;
  if (model.over) {
    box(screen, toybox::kMargin, y, inner, by - y - 10, kInk, 10);
    label(screen,
          rect(toybox::kMargin, y + (by - y - 10 - toybox::kUiCut.inkHeight) / 2, inner, toybox::kUiCut.inkHeight),
          "NO MOVES LEFT", toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Center, fui::Color::White);
  } else {
    caption(screen, toybox::kMargin, y + (by - y - 10 - toybox::kTileCut.inkHeight) / 2, inner,
            "SWIPE TO SLIDE. 1 AND 2 MAKE 3, THEN PAIRS DOUBLE", fui::TextAlign::Center);
  }
  fui::ButtonProps fresh;
  fresh.label = model.newArmed ? "SURE?" : "NEW GAME";
  fresh.action = ActionNew;
  fresh.styles = toybox::rowStyles();
  fresh.text = toybox::buttonText(screen.theme());
  fresh.radius = toybox::kPillRadius / 2;
  fresh.state = model.newArmed || model.over ? fui::StateSelected : fui::StateNormal;
  screen.button(fresh, rect(toybox::kMargin, by, inner, toybox::kPillHeight));
}

}  // namespace triplesui

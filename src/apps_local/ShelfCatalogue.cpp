#include "ShelfCatalogue.h"

#include <FreeInkUIIcon.h>
#include <GfxRenderer.h>

#include <cstdio>

#include "../components/UITheme.h"
#include "ShelfScreen.h"
#include "ShelfShots.h"
#include "ui/ExtraIcons.h"
#include "ui/Toybox.h"
#include "ui/ToyboxFonts.h"
#include "ui/ToyboxFormat.h"
#include "ui/ToyboxLab.h"
#include "ui/ToyboxSerif.h"
#include "ui/ToyboxTheme.h"

namespace shelfcat {

namespace lab = toyboxlab;

namespace {

constexpr int kTabH = 36;
constexpr int kTabGap = 6;
constexpr int kRowH = 62;
constexpr int kRowGap = 6;
constexpr int kPageBarH = 40;

// Pixel-face text with its ink starting at `inkTop`.
void pixelText(const GfxRenderer& r, const int font, const toybox::CutMetrics& cut, const int x, const int inkTop,
               const char* text, const bool black = true) {
  r.drawText(font, x, inkTop - (cut.ascender - cut.inkHeight), text, black);
}

void invisibleButton(toybox::Screen& screen, const fui::Rect& rect, const fui::ActionId action, const int value) {
  fui::StyleSet invisible;
  invisible.explicitlySet = true;
  fui::ButtonProps b;
  b.action = action;
  b.value = static_cast<int16_t>(value);
  b.styles = invisible;
  b.minTouchSize = 0;
  screen.button(b, rect);
}

}  // namespace

void drawList(toybox::Screen& screen, GfxRenderer& renderer, const ListModel& model) {
  // The folder's own band, as the list it replaces drew it: the title, the
  // page count, the folder's mark, and the whole band the way into choosing
  // what the folder shows.
  fui::HeaderProps header;
  header.title = model.title;
  header.borderEdges = fui::EdgesNone;
  header.rightReserve = static_cast<int16_t>(toybox::kIconSize + toybox::kGutter);
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  invisibleButton(screen, toybox::headerBandRect(screen), shelfui::ActionChoose, 0);

  const fui::Rect panel = screen.device().screen();
  const int16_t markX = static_cast<int16_t>(panel.width - toybox::kIconSize - toybox::kMargin);
  if (model.mark != nullptr) {
    screen.target().bitmap(
        fui::makeRect(markX, toybox::bandCenterY(screen, toybox::kIconSize), toybox::kIconSize, toybox::kIconSize),
        fui::bitmapFromIcon(*model.mark), fui::BitmapMode::Contain, fui::Paint::solid(fui::Color::White));
  }
  if (model.pageCount > 1) {
    char counter[toybox::kSlashCounterChars];
    std::snprintf(counter, sizeof(counter), "%d/%d", model.page + 1, model.pageCount);
    fui::TextStyle style;
    style.font = toybox::kUiFont;
    style.align = fui::TextAlign::Right;
    style.color = fui::Color::White;
    const fui::Rect box = fui::makeRect(0, toybox::bandCenterY(screen, toybox::kUiCut.inkHeight),
                                        static_cast<int16_t>(markX - toybox::kGutter), toybox::kUiCut.inkHeight);
    screen.target().text(toybox::inkCentred(box, toybox::kUiCut), counter, style);
  }

  // The tabs: home, the folder's kinds, everything. Two rows once one is crowded.
  const int x = toybox::kMargin;
  const int inner = panel.width - toybox::kMargin * 2;
  const int tabs = model.kindCount + 2;
  const int tabRows = tabs > 5 ? 2 : 1;
  const int perRow = (tabs + tabRows - 1) / tabRows;
  const int tabW = (inner - kTabGap * (perRow - 1)) / perRow;
  int y = toybox::kChromeHeight + 8;
  for (int i = 0; i < tabs; ++i) {
    const char* name = i == 0 ? kHomeTab : (i == tabs - 1 ? kAllTab : model.kinds[i - 1]);
    lab::button(screen,
                lab::rect(x + (i % perRow) * (tabW + kTabGap), y + (i / perRow) * (kTabH + kTabGap), tabW, kTabH), name,
                ActionTab, i, i == model.tab, true);
  }
  y += tabRows * (kTabH + kTabGap) + 2;

  if (model.rowCount == 0) {
    // Only the home tab can be empty: nothing starred and nothing opened yet.
    toyboxserif::draw(renderer, toybox::kReadingSmallFontId, 25, x + 12, y + 40, inner - 24,
                      "Aqu\xC3\xAD aparecen tus favoritas y lo \xC3\xBAltimo que has abierto. Mant\xC3\xA9n pulsada "
                      "cualquier entrada de las otras pesta\xC3\xB1"
                      "as para ver su ficha y marcarla como favorita.",
                      6);
    return;
  }

  for (int i = 0; i < model.rowCount && i < kRowsPerPage; ++i) {
    const Row& row = model.rows[i];
    const int ry = y + i * (kRowH + kRowGap);
    fui::ButtonProps b;
    b.label = "";
    b.action = shelfui::ActionOpen;
    b.value = row.value;
    b.styles = toybox::rowStyles();
    b.radius = lab::kRadius;
    screen.button(b, lab::rect(x, ry, inner, kRowH));

    const int iconX = x + inner - 14 - toybox::kIconSize;
    if (row.icon != nullptr) {
      screen.target().bitmap(
          lab::rect(iconX, ry + (kRowH - toybox::kIconSize) / 2, toybox::kIconSize, toybox::kIconSize),
          fui::bitmapFromIcon(*row.icon), fui::BitmapMode::Contain, fui::Paint::solid(fui::Color::Black));
    }
    int textRight = iconX - 10;
    if (row.star) {
      textRight -= 28;
      screen.target().bitmap(lab::rect(textRight, ry + 8, 24, 24), fui::bitmapFromIcon(icon_star_24),
                             fui::BitmapMode::Contain, fui::Paint::solid(fui::Color::Black));
    }
    pixelText(renderer, toybox::kUiFontId, toybox::kUiCut, x + 14, ry + 8, row.title);
    toyboxserif::draw(renderer, toybox::kReadingSmallFontId, 25, x + 14, ry + 8 + toybox::kUiCut.inkHeight - 2,
                      iconX - 10 - (x + 14), row.blurb, 1);
  }

  if (model.pageCount > 1) {
    const int barY = panel.height - toybox::kMargin - kPageBarH;
    const int pitch = 46;
    const int startX = (panel.width - pitch * model.pageCount + 6) / 2;
    for (int p = 0; p < model.pageCount; ++p) {
      char number[toybox::kIntTextChars];
      std::snprintf(number, sizeof(number), "%d", p + 1);
      lab::button(screen, lab::rect(startX + p * pitch, barY, pitch - 6, kPageBarH), number, shelfui::ActionGoToPage, p,
                  p == model.page, true);
    }
  }
}

void drawSheet(toybox::Screen& screen, GfxRenderer& renderer, const SheetModel& model) {
  lab::chrome(screen, model.title);
  const fui::Rect panel = screen.device().screen();
  const int x = toybox::kMargin;
  const int inner = panel.width - toybox::kMargin * 2;
  const int top = toybox::kChromeHeight + 16;

  // The picture, framed, or the frame alone when there is none.
  const int pw = shelfshots::kWidth;
  const int ph = shelfshots::kHeight;
  renderer.drawRect(x, top, pw + 6, ph + 6, 2, true);
  if (model.picture != nullptr) {
    const int stride = (pw + 7) / 8;
    for (int row = 0; row < ph; ++row) {
      const uint8_t* line = model.picture + row * stride;
      int run = -1;
      for (int col = 0; col <= pw; ++col) {
        const bool ink = col < pw && (line[col >> 3] & (0x80 >> (col & 7))) != 0;
        if (ink && run < 0) run = col;
        if (!ink && run >= 0) {
          renderer.fillRect(x + 3 + run, top + 3 + row, col - run, 1, true);
          run = -1;
        }
      }
    }
  }

  // Beside it: what kind of thing it is, whether it wants the network, and what it does.
  const int cx = x + pw + 6 + 16;
  const int cw = x + inner - cx;
  int y = top;
  const int tagW = renderer.getTextWidth(toybox::kTileFontId, model.kind) + 16;
  renderer.fillRect(cx, y, tagW, 22, true);
  pixelText(renderer, toybox::kTileFontId, toybox::kTileCut, cx + 8, y + (22 - toybox::kTileCut.inkHeight) / 2,
            model.kind, false);
  y += 32;
  pixelText(renderer, toybox::kTileFontId, toybox::kTileCut, cx, y, model.online ? "USA WIFI" : "SIN CONEXION");
  y += toybox::kTileCut.inkHeight + 10;
  if (model.favourite) {
    screen.target().bitmap(lab::rect(cx, y - 4, 24, 24), fui::bitmapFromIcon(icon_star_24), fui::BitmapMode::Contain,
                           fui::Paint::solid(fui::Color::Black));
    pixelText(renderer, toybox::kTileFontId, toybox::kTileCut, cx + 30, y + 3, "FAVORITA");
    y += 28;
  }
  y += 6;
  renderer.fillRect(cx, y, cw, 1, true);
  y += 10;
  toyboxserif::draw(renderer, toybox::kReadingSmallFontId, 25, cx, y, cw, model.about, (top + ph + 6 - y) / 25);

  // What can be done with it.
  y = top + ph + 6 + 18;
  lab::button(screen, lab::rect(x, y, inner, 68), "ABRIR", ActionSheetOpen, 0, true, false);
  y += 68 + 10;
  lab::button(screen, lab::rect(x, y, inner, 54), model.favourite ? "QUITAR DE FAVORITAS" : "MARCAR COMO FAVORITA",
              ActionSheetFavourite, 0);
  y += 54 + 10;
  lab::button(screen, lab::rect(x, y, inner, 54), "OCULTAR DE LA LISTA", ActionSheetHide, 0);
  y += 54 + 14;
  toyboxserif::draw(renderer, toybox::kReadingSmallFontId, 25, x, y, inner,
                    "Lo que ocultas no se borra: vuelve tocando la cabecera de la lista.", 2);
}

}  // namespace shelfcat

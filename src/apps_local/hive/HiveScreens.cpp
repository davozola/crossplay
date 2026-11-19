#include "HiveScreens.h"

#include <cmath>
#include <cstdio>

#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxHowTo.h"
#include "HiveEngine.h"

namespace hiveui {

namespace {

namespace hv = hive;

constexpr int kBottomReserve = 40;  // the button hints
constexpr int kRadius = toybox::kPillRadius / 2;
constexpr int kTrayH = 70;
constexpr float kRoot3 = 1.7320508f;

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

void box(toybox::Screen& screen, const int x, const int y, const int w, const int h, const fui::Paint& paint) {
  if (w <= 0 || h <= 0) return;
  screen.target().fill(rect(x, y, w, h), paint);
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
             const fui::TextAlign align = fui::TextAlign::Center, const fui::Color color = fui::Color::Black) {
  label(screen, rect(x, y, w, toybox::kTileCut.inkHeight), text, toybox::kTileFont, toybox::kTileCut, align, color);
}

// A pointy-topped hexagon, `size` from centre to corner, a row at a time.
void hexagon(toybox::Screen& screen, const int cx, const int cy, const float size, const fui::Paint& paint) {
  if (size < 1) return;
  const int reach = static_cast<int>(size);
  for (int dy = -reach; dy <= reach; ++dy) {
    const float a = std::fabs(static_cast<float>(dy));
    const float half = a <= size / 2 ? size * kRoot3 / 2 : kRoot3 * (size - a);
    const int w = static_cast<int>(half);
    if (w > 0) box(screen, cx - w, cy + dy, w * 2, 1, paint);
  }
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

// One piece: white is paper with a black edge, black is solid. A piece that
// stands on others wears a second edge inside the first.
void piece(toybox::Screen& screen, const int cx, const int cy, const float size, const hv::Piece& p,
           const bool selected) {
  const bool black = p.color == 1;
  if (selected) hexagon(screen, cx, cy, size + 5, fui::Paint::dither(fui::Color::DarkGray));
  hexagon(screen, cx, cy, size - 1, kInk);
  if (!black) hexagon(screen, cx, cy, size - 4, kPaper);
  if (p.z > 0) {
    hexagon(screen, cx, cy, size - 8, black ? kPaper : kInk);
    hexagon(screen, cx, cy, size - 10, black ? kInk : kPaper);
  }
  const char text[2] = {hv::bugLetter(static_cast<hv::Bug>(p.bug)), 0};
  const bool big = size >= 26;
  const toybox::CutMetrics& cut = big ? toybox::kUiCut : toybox::kTileCut;
  const int w = static_cast<int>(size * 2);
  label(screen, rect(cx - w / 2, cy - cut.inkHeight / 2, w, cut.inkHeight), text,
        big ? toybox::kUiFont : toybox::kTileFont, cut, fui::TextAlign::Center,
        black ? fui::Color::White : fui::Color::Black);
}

}  // namespace

void Layout::centre(const int q, const int r, int& x, int& y) const {
  x = static_cast<int>(originX + size * kRoot3 * (static_cast<float>(q) + static_cast<float>(r) / 2));
  y = static_cast<int>(originY + size * 1.5f * static_cast<float>(r));
}

Layout layoutOf(const fui::DeviceContext& device, const hv::Game& game) {
  // The pieces and one ring of cells round them, which is everywhere a move
  // can end: the drawing does not shift when a piece is picked up.
  float minU = 0, maxU = 0;
  int minR = 0, maxR = 0;
  bool any = false;
  auto take = [&](const int q, const int r) {
    const float u = static_cast<float>(q) + static_cast<float>(r) / 2;
    if (!any) {
      minU = maxU = u;
      minR = maxR = r;
      any = true;
      return;
    }
    if (u < minU) minU = u;
    if (u > maxU) maxU = u;
    if (r < minR) minR = r;
    if (r > maxR) maxR = r;
  };
  for (int i = 0; i < hv::kPieces; ++i) {
    const hv::Piece& p = game.pieces[i];
    if (!p.placed) continue;
    take(p.q, p.r);
    for (int d = 0; d < 6; ++d) take(p.q + hv::kDirQ[d], p.r + hv::kDirR[d]);
  }
  if (!any) {
    take(0, 0);
    for (int d = 0; d < 6; ++d) take(hv::kDirQ[d], hv::kDirR[d]);
  }
  const float left = static_cast<float>(toybox::kMargin);
  const float top = static_cast<float>(toybox::kChromeHeight + 44);
  const float width = static_cast<float>(device.width - toybox::kMargin * 2);
  const float height = static_cast<float>(device.height - kBottomReserve - kTrayH - 34) - top;
  Layout layout;
  const float byWidth = width / (kRoot3 * (maxU - minU + 1));
  const float byHeight = height / (1.5f * static_cast<float>(maxR - minR) + 2);
  layout.size = byWidth < byHeight ? byWidth : byHeight;
  if (layout.size > 44) layout.size = 44;
  const float spanX = layout.size * kRoot3 * (maxU - minU);
  const float spanY = layout.size * 1.5f * static_cast<float>(maxR - minR);
  layout.originX = left + (width - spanX) / 2 - layout.size * kRoot3 * minU;
  layout.originY = top + (height - spanY) / 2 - layout.size * 1.5f * static_cast<float>(minR);
  return layout;
}

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  chrome(screen, "HIVE", nullptr);
  const fui::DeviceContext device = screen.device();
  const int inner = device.width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 14;
  fui::TextStyle rule;
  rule.font = toybox::kTileFont;
  rule.align = fui::TextAlign::Left;
  rule.maxLines = 5;
  screen.target().text(rect(toybox::kMargin, y, inner, toybox::kTileCut.lineHeight * 5),
                       "SURROUND THE OTHER QUEEN. PIECES GO DOWN BESIDE YOUR OWN, THEN MOVE EACH IN ITS WAY: "
                       "QUEEN ONE STEP, BEETLE ONE STEP AND OVER THE TOP, SPIDER THREE, GRASSHOPPER JUMPS A "
                       "LINE, ANT ANYWHERE ROUND THE EDGE. THE HIVE MAY NEVER SPLIT.",
                       rule);
  y += toybox::kTileCut.lineHeight * 5 + 12;
  auto row = [&](const char* text, const fui::ActionId action, const int value, const bool strong) {
    fui::ButtonProps b;
    b.label = text;
    b.action = action;
    b.value = static_cast<int16_t>(value);
    b.styles = toybox::rowStyles();
    b.radius = kRadius;
    b.state = strong ? fui::StateSelected : fui::StateNormal;
    screen.button(b, rect(toybox::kMargin, y, inner, 62));
    y += 62 + 8;
  };
  if (model.canContinue) row("CONTINUE", ActionContinue, 0, true);
  // Each level with the record against it.
  static const char* const kLevels[3] = {"EASY", "MEDIUM", "HARD"};
  char levelRows[3][40];
  for (int i = 0; i < 3; ++i) {
    std::snprintf(levelRows[i], sizeof(levelRows[i]), "DEVICE, %s  %d-%d", kLevels[i], model.won[i] % 1000,
                  model.lost[i] % 1000);
    row(levelRows[i], ActionStart, i, false);
  }
  row("TWO PLAYERS", ActionStart, 3, false);
  toyboxhowto::button(screen, rect(toybox::kMargin, y + 4, inner, toybox::kPillHeight));
}

void buildPlay(toybox::Screen& screen, const PlayModel& model) {
  chrome(screen, "HIVE", model.modeLabel);
  if (!model.game) return;
  const hv::Game& g = *model.game;
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  caption(screen, toybox::kMargin, toybox::kChromeHeight + 16, inner, model.status);

  const Layout layout = layoutOf(device, g);
  // Where the selected piece may go, under the pieces: a beetle's targets can
  // be cells that are taken.
  for (int i = 0; i < model.targetCount; ++i) {
    int x = 0, y = 0;
    layout.centre(model.targets[i].q, model.targets[i].r, x, y);
    if (g.heightAt(model.targets[i].q, model.targets[i].r) > 0) continue;
    hexagon(screen, x, y, layout.size - 2, fui::Paint::dither(fui::Color::DarkGray));
    hexagon(screen, x, y, layout.size - 5, kPaper);
    toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), 5, fui::Color::Black);
  }
  for (int i = 0; i < hv::kPieces; ++i) {
    const hv::Piece& p = g.pieces[i];
    if (!p.placed || g.topAt(p.q, p.r) != i) continue;
    int x = 0, y = 0;
    layout.centre(p.q, p.r, x, y);
    piece(screen, x, y, layout.size, p, i == model.selectedPiece);
    if (i == model.lastPiece) {
      // The piece that just moved: a dot at its top corner.
      const int dy = static_cast<int>(layout.size * 0.62f);
      toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y - dy), 4,
                   p.color == 1 ? fui::Color::White : fui::Color::Black);
    }
  }
  // A target that is on top of the hive is marked over the piece it is on.
  for (int i = 0; i < model.targetCount; ++i) {
    if (g.heightAt(model.targets[i].q, model.targets[i].r) == 0) continue;
    int x = 0, y = 0;
    layout.centre(model.targets[i].q, model.targets[i].r, x, y);
    const int dy = static_cast<int>(layout.size * 0.55f);
    toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y + dy), 7, fui::Color::White);
    toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y + dy), 5, fui::Color::Black);
  }

  // The hand of whoever is to move, and a line for the other.
  const int trayY = device.height - kBottomReserve - kTrayH - 4;
  const int other = 1 - model.handColor;
  char rest[64];
  std::snprintf(rest, sizeof(rest), "%s HAS  Q%d  B%d  S%d  G%d  A%d", other == 0 ? "WHITE" : "BLACK",
                g.inHand(other, hv::Bug::Queen), g.inHand(other, hv::Bug::Beetle), g.inHand(other, hv::Bug::Spider),
                g.inHand(other, hv::Bug::Hopper), g.inHand(other, hv::Bug::Ant));
  const int undoW = model.canUndo ? 96 : 0;
  caption(screen, toybox::kMargin, trayY - toybox::kTileCut.inkHeight - 10, inner - undoW, rest);
  if (model.canUndo) {
    fui::ButtonProps undo;
    undo.label = "UNDO";
    undo.action = ActionUndo;
    undo.styles = toybox::rowStyles();
    undo.text = toybox::buttonText(screen.theme());
    undo.radius = kRadius;
    screen.button(undo, rect(toybox::kMargin + inner - undoW, trayY - 44, undoW, 38));
  }
  const int gap = 6;
  const int w = (inner - gap * (hv::kBugTypes - 1)) / hv::kBugTypes;
  for (int b = 0; b < hv::kBugTypes; ++b) {
    const hv::Bug bug = static_cast<hv::Bug>(b);
    const int left = g.inHand(model.handColor, bug);
    const int handPiece = g.handPiece(model.handColor, bug);
    const bool on = handPiece >= 0 && handPiece == model.selectedPiece;
    const fui::Rect r = rect(toybox::kMargin + b * (w + gap), trayY, w, kTrayH);
    fui::ButtonProps button;
    button.label = "";
    button.action = ActionHand;
    button.value = static_cast<int16_t>(b);
    button.enabled = model.trayEnabled && left > 0;
    button.styles = toybox::rowStyles();
    button.radius = kRadius;
    button.state = on ? fui::StateSelected : fui::StateNormal;
    screen.button(button, r);
    const fui::Color ink = on ? fui::Color::White : (left > 0 ? fui::Color::Black : fui::Color::DarkGray);
    char top[toybox::kIntChars + 4];
    std::snprintf(top, sizeof(top), "%c %d", hv::bugLetter(bug), left);
    label(screen, rect(r.x, r.y + 12, w, toybox::kUiCut.inkHeight), top, toybox::kUiFont, toybox::kUiCut,
          fui::TextAlign::Center, ink);
    caption(screen, r.x, r.y + 12 + toybox::kUiCut.inkHeight + 8, w, hv::bugName(bug), fui::TextAlign::Center, ink);
  }
}

}  // namespace hiveui

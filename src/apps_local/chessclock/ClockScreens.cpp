#include "ClockScreens.h"

#include <cstdio>

#include "../ui/ToyboxFormat.h"

namespace clockui {

namespace {

namespace cc = chessclock;

constexpr int16_t kStrip = 84;

void toyboxChrome(toybox::Screen& screen, const char* title) {
  fui::HeaderProps header;
  header.title = title;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

// Seven segments, a..g. Bit 0 is a.
constexpr uint8_t kSegments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// Draws glyphs into `half`, laid out as if upright, and turned through 180
// degrees when `flip` is set. Every glyph is rectangles, and a rectangle turned
// through 180 degrees about the half's centre is a rectangle, so the top
// player's digits are the same drawing code read from the other side.
struct Pen {
  toybox::Screen& screen;
  fui::Rect half;
  bool flip;
  fui::Paint paint;

  void rect(const int x, const int y, const int w, const int h) const {
    int16_t sx = static_cast<int16_t>(half.x + x);
    int16_t sy = static_cast<int16_t>(half.y + y);
    if (flip) {
      sx = static_cast<int16_t>(half.x + half.width - x - w);
      sy = static_cast<int16_t>(half.y + half.height - y - h);
    }
    screen.target().fill(fui::makeRect(sx, sy, static_cast<int16_t>(w), static_cast<int16_t>(h)), paint);
  }

  void digit(const int x, const int y, const int w, const int h, const int t, const int d) const {
    const uint8_t s = kSegments[d % 10];
    const int mid = h / 2;
    const int arm = mid - t - t / 2;
    if (s & 0x01) rect(x + t, y, w - 2 * t, t);
    if (s & 0x02) rect(x + w - t, y + t, t, arm);
    if (s & 0x04) rect(x + w - t, y + mid + t / 2, t, arm);
    if (s & 0x08) rect(x + t, y + h - t, w - 2 * t, t);
    if (s & 0x10) rect(x, y + mid + t / 2, t, arm);
    if (s & 0x20) rect(x, y + t, t, arm);
    if (s & 0x40) rect(x + t, y + mid - t / 2, w - 2 * t, t);
  }
};

void drawHalf(toybox::Screen& screen, const FaceModel& model, const int side) {
  const fui::DeviceContext device = screen.device();
  const fui::Rect half = halfRect(device, side);
  const bool flip = side == 1;
  const bool running = model.state == cc::State::Running && model.active == side;
  const bool out = model.flagged == side;
  auto& t = screen.target();

  const fui::Rect inner = fui::makeRect(
      static_cast<int16_t>(half.x + toybox::kMargin), static_cast<int16_t>(half.y + toybox::kMargin),
      static_cast<int16_t>(half.width - toybox::kMargin * 2), static_cast<int16_t>(half.height - toybox::kMargin * 2));
  if (running) {
    t.fill(inner, fui::Paint::solid(fui::Color::Black), 18);
  } else if (out) {
    t.fill(inner, fui::Paint::dither(fui::Color::LightGray), 18);
    t.stroke(inner, fui::Paint::solid(fui::Color::Black), 4, 18);
  } else {
    t.stroke(inner, fui::Paint::solid(fui::Color::Black), 4, 18);
  }
  const fui::Color ink = running ? fui::Color::White : fui::Color::Black;
  const Pen pen{screen, inner, flip, fui::Paint::solid(ink)};

  // The time, in seven-segment digits as large as the half allows.
  const cc::Shown& s = model.shown[side];
  int digits[4];
  int count = 0;
  bool colon = false;
  if (s.withSeconds) {
    if (s.minutes >= 10) digits[count++] = (s.minutes / 10) % 10;
    digits[count++] = s.minutes % 10;
    digits[count++] = s.seconds / 10;
    digits[count++] = s.seconds % 10;
    colon = true;
  } else {
    if (s.minutes >= 10) digits[count++] = (s.minutes / 10) % 10;
    digits[count++] = s.minutes % 10;
  }
  const int w = inner.width;
  const int h = inner.height;
  int dh = h * 52 / 100;
  int dw = dh * 55 / 100;
  int gap = dh / 9;
  const int colonW = colon ? dh / 5 : 0;
  int total = count * dw + (count - 1) * gap + (colon ? colonW + gap : 0);
  if (total > w - 40) {
    const int scale = (w - 40) * 100 / total;
    dh = dh * scale / 100;
    dw = dw * scale / 100;
    gap = gap * scale / 100;
    total = (w - 40);
  }
  const int th = dh / 9 > 6 ? dh / 9 : 6;
  int x = (w - total) / 2;
  const int y = (h - dh) / 2 - 10;
  for (int i = 0; i < count; ++i) {
    if (colon && i == count - 2) {
      const int dot = th;
      pen.rect(x + (colonW - dot) / 2, y + dh / 3 - dot / 2, dot, dot);
      pen.rect(x + (colonW - dot) / 2, y + dh * 2 / 3 - dot / 2, dot, dot);
      x += colonW + gap;
    }
    pen.digit(x, y, dw, dh, th, digits[i]);
    x += dw + gap;
  }

  // With whole minutes only, a prime after the number, as in 5'. A clock in
  // its last minute reads 0:45 and needs nothing.
  if (!s.withSeconds) pen.rect(x - gap + th, y, th, dh / 4);

  // This side's move count, small, in the corner nearest its player. Digits
  // rather than text: every mark on the face is a rectangle, so the top half
  // is the same drawing turned round. The renderer's text cannot be turned
  // through 180 degrees.
  const int mh = 34;
  const int mw = mh * 55 / 100;
  const int mt = 5;
  int moves = model.moves[side];
  int md[5];
  int mc = 0;
  do {
    md[mc++] = moves % 10;
    moves /= 10;
  } while (moves > 0 && mc < 5);
  int mx = 22;
  for (int i = mc - 1; i >= 0; --i) {
    pen.digit(mx, h - mh - 20, mw, mh, mt, md[i]);
    mx += mw + 6;
  }
}

}  // namespace

fui::Rect halfRect(const fui::DeviceContext& device, const int side) {
  const int16_t halfH = static_cast<int16_t>((device.height - kStrip) / 2);
  if (side == 1) return fui::makeRect(0, 0, device.width, halfH);
  return fui::makeRect(0, static_cast<int16_t>(halfH + kStrip), device.width,
                       static_cast<int16_t>(device.height - halfH - kStrip));
}

int sideAt(const fui::DeviceContext& device, const int x, const int y) {
  for (int side = 0; side < 2; ++side) {
    const fui::Rect r = halfRect(device, side);
    if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom()) return side;
  }
  return -1;
}

void presetLabel(const int preset, char* out, const int outSize) {
  const cc::Preset& p = cc::kPresets[preset >= 0 && preset < cc::kPresetCount ? preset : cc::kDefaultPreset];
  std::snprintf(out, static_cast<size_t>(outSize), "%u+%u", static_cast<unsigned>(p.minutes),
                static_cast<unsigned>(p.increment));
}

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toyboxChrome(screen, "CHESS CLOCK");

  fui::TextStyle hint;
  hint.font = toybox::kBodyFont;
  hint.align = fui::TextAlign::Center;
  hint.maxLines = 8;
  const fui::Rect top = screen.takeTop(300);
  screen.target().text(top,
                       "LAY IT FLAT BETWEEN YOU. AFTER YOUR MOVE, TAP YOUR HALF TO START THEIR CLOCK. 3+2 IS THREE "
                       "MINUTES EACH, PLUS TWO SECONDS FOR EVERY MOVE.",
                       hint);

  char preset[2 * toybox::kIntChars + 2];
  presetLabel(model.preset, preset, sizeof(preset));
  char startLabel[2 * toybox::kIntChars + toybox::literalChars("START A NEW  GAME") + 2];
  std::snprintf(startLabel, sizeof(startLabel), "START A NEW %s GAME", preset);
  char timeLabel[2 * toybox::kIntChars + toybox::literalChars("TIME ") + 2];
  std::snprintf(timeLabel, sizeof(timeLabel), "TIME %s", preset);

  fui::ListItem rows[static_cast<int>(MenuRow::Count)] = {};
  int count = 0;
  auto add = [&](const char* label, const MenuRow row) {
    rows[count].label = label;
    rows[count].actionValue = static_cast<int16_t>(row);
    ++count;
  };
  if (model.canResume) add("RESUME", MenuRow::Resume);
  add(startLabel, MenuRow::Start);
  add(timeLabel, MenuRow::Time);
  add(model.alwaysSeconds ? "SECONDS: ALWAYS" : "SECONDS: LAST MINUTE", MenuRow::Seconds);

  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<uint16_t>(count);
  list.selectedIndex = 0;
  list.action = ActionMenuRow;
  const int16_t listHeight =
      static_cast<int16_t>(count * toybox::kRowHeight + (count - 1) * toybox::kGutter / 2 + toybox::kGutter);
  screen.list(list, listHeight, fui::LayoutAnchor::Bottom);
}

void buildFace(toybox::Screen& screen, const FaceModel& model) {
  toybox::absoluteChrome(screen);
  drawHalf(screen, model, 1);
  drawHalf(screen, model, 0);

  // The strip between the halves: the time control, and the one button.
  const fui::DeviceContext device = screen.device();
  const fui::Rect top = halfRect(device, 1);
  const int16_t y = static_cast<int16_t>(top.bottom() + (kStrip - toybox::kPillHeight) / 2);
  char preset[2 * toybox::kIntChars + 2];
  presetLabel(model.preset, preset, sizeof(preset));
  fui::TextStyle side;
  side.font = toybox::kTileFont;
  side.align = fui::TextAlign::Left;
  const fui::Rect left = fui::makeRect(toybox::kMargin, y, 110, toybox::kPillHeight);
  screen.target().text(toybox::inkCentred(left, toybox::kTileCut), preset, side);

  const int16_t buttonW = 200;
  const fui::Rect centre =
      fui::makeRect(static_cast<int16_t>((device.width - buttonW) / 2), y, buttonW, toybox::kPillHeight);
  fui::ButtonProps pause;
  pause.action = ActionPause;
  switch (model.state) {
    case cc::State::Running:
      pause.label = "PAUSE";
      pause.styles = toybox::rowStyles();
      break;
    case cc::State::Paused:
      pause.label = "RESUME";
      break;
    case cc::State::Ready:
      pause.label = "TAP A SIDE";
      pause.enabled = false;
      pause.styles = toybox::rowStyles();
      break;
    case cc::State::Flagged:
      pause.label = "NEW GAME";
      pause.action = ActionNew;
      break;
  }
  screen.button(pause, centre);

  if (model.state == cc::State::Paused) {
    fui::ButtonProps fresh;
    fresh.label = "NEW";
    fresh.action = ActionNew;
    fresh.styles = toybox::rowStyles();
    const int16_t w = 110;
    screen.button(fresh,
                  fui::makeRect(static_cast<int16_t>(device.width - toybox::kMargin - w), y, w, toybox::kPillHeight));
  }
}

}  // namespace clockui

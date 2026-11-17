#include "MarketsScreens.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../ui/ToyboxFormat.h"

namespace marketsui {

namespace {

namespace mk = markets;

constexpr int kBottomReserve = 40;  // the button hints
constexpr int kRadius = toybox::kPillRadius / 2;

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

// Text whose ink (cap height) is centred in `r` vertically.
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

// The small cut, ink top at `y`.
void caption(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
             const fui::TextAlign align = fui::TextAlign::Left, const fui::Color color = fui::Color::Black) {
  label(screen, rect(x, y, w, toybox::kTileCut.inkHeight), text, toybox::kTileFont, toybox::kTileCut, align, color);
}

int textWidth(toybox::Screen& screen, const char* text, const fui::FontId font) {
  fui::TextStyle s;
  s.font = font;
  return screen.target().measureText(font, text, s).width;
}

void disc(toybox::Screen& screen, const int cx, const int cy, const int r, const fui::Color c) {
  if (r > 0) toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r), c);
}

// Even-odd scanline fill in whole pixels: the chart line and the arrows.
void polygon(toybox::Screen& screen, const float* xs, const float* ys, const int n, const fui::Paint& paint) {
  float top = ys[0];
  float bottom = ys[0];
  for (int i = 1; i < n; ++i) {
    if (ys[i] < top) top = ys[i];
    if (ys[i] > bottom) bottom = ys[i];
  }
  float cross[8];
  for (int y = static_cast<int>(std::floor(top)); y <= static_cast<int>(std::ceil(bottom)); ++y) {
    const float sy = static_cast<float>(y) + 0.5f;
    int count = 0;
    for (int i = 0, j = n - 1; i < n && count < 8; j = i++) {
      if ((ys[i] <= sy && ys[j] > sy) || (ys[j] <= sy && ys[i] > sy)) {
        cross[count++] = xs[i] + (sy - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
      }
    }
    for (int a = 1; a < count; ++a) {
      for (int b = a; b > 0 && cross[b - 1] > cross[b]; --b) {
        const float t = cross[b];
        cross[b] = cross[b - 1];
        cross[b - 1] = t;
      }
    }
    for (int k = 0; k + 1 < count; k += 2) {
      const int x0 = static_cast<int>(std::lround(cross[k]));
      const int x1 = static_cast<int>(std::lround(cross[k + 1]));
      if (x1 > x0) box(screen, x0, y, x1 - x0, 1, paint);
    }
  }
}

void stroke(toybox::Screen& screen, const float x0, const float y0, const float x1, const float y1, const float w) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len <= 0) return;
  const float nx = -dy / len * w / 2;
  const float ny = dx / len * w / 2;
  const float xs[4] = {x0 + nx, x1 + nx, x1 - nx, x0 - nx};
  const float ys[4] = {y0 + ny, y1 + ny, y1 - ny, y0 - ny};
  polygon(screen, xs, ys, 4, kInk);
}

// A triangle pointing up or down, `size` across, centred on (cx, cy).
void arrow(toybox::Screen& screen, const int cx, const int cy, const int size, const bool up, const fui::Color colour) {
  const float h = static_cast<float>(size) * 0.8f;
  const float half = static_cast<float>(size) / 2;
  const float tip = up ? static_cast<float>(cy) - h / 2 : static_cast<float>(cy) + h / 2;
  const float base = up ? static_cast<float>(cy) + h / 2 : static_cast<float>(cy) - h / 2;
  const float xs[3] = {static_cast<float>(cx), static_cast<float>(cx) + half, static_cast<float>(cx) - half};
  const float ys[3] = {tip, base, base};
  polygon(screen, xs, ys, 3, fui::Paint::solid(colour));
}

void bind(const FaceSwap& faces, const Face face) {
  if (faces.bindTitle) faces.bindTitle(faces.ctx, face);
}

const toybox::CutMetrics& cutOf(const FaceSwap& faces, const Face face) {
  if (!faces.bindTitle || face == Face::Display) return toybox::kDisplayCut;
  return face == Face::Huge ? toybox::kHugeCut : toybox::kLargeCut;
}

// The price in one of the big cuts, ink top at `y`. Steps down a cut when it
// would not fit `maxW`. Returns the ink height used.
int priceText(toybox::Screen& screen, const FaceSwap& faces, Face face, const int x, const int y, const int maxW,
              const char* text) {
  for (;;) {
    bind(faces, face);
    const int w = textWidth(screen, text, toybox::kDisplayFont);
    if (w <= maxW || face == Face::Display || !faces.bindTitle) break;
    face = face == Face::Huge ? Face::Large : Face::Display;
  }
  const toybox::CutMetrics& cut = cutOf(faces, face);
  label(screen, rect(x, y, maxW, cut.inkHeight), text, toybox::kDisplayFont, cut, fui::TextAlign::Left);
  bind(faces, Face::Display);
  return cut.inkHeight;
}

// Up is an outline, down is solid: the direction reads from across a room
// before the number does. `rightX` >= 0 ends the pill there instead of
// starting it at `x`. Returns the pill's width.
int changePill(toybox::Screen& screen, int x, const int y, const int h, const double percent, const int rightX = -1) {
  char text[24];
  mk::formatPercent(percent, text, sizeof(text));
  const bool flat = std::strcmp(text, "0.00%") == 0;
  const bool down = !flat && percent < 0;
  const int tw = textWidth(screen, text, toybox::kUiFont);
  const int lead = h >= 36 ? 14 : 10;
  const int arrowW = flat ? 0 : (h >= 36 ? 26 : 22);
  const int w = lead + arrowW + tw + lead;
  if (rightX >= 0) x = rightX - w;
  if (down) {
    box(screen, x, y, w, h, kInk, h / 2);
  } else {
    screen.target().stroke(rect(x, y, w, h), kInk, 2, static_cast<uint8_t>(h / 2));
  }
  const fui::Color ink = down ? fui::Color::White : fui::Color::Black;
  if (!flat) arrow(screen, x + lead + 7, y + h / 2, h >= 36 ? 16 : 14, !down, ink);
  label(screen, rect(x + lead + arrowW, y, tw + 4, h), text, toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Left,
        ink);
  return w;
}

// The last `sessions` closes as a line in `r`, the newest replaced by the live
// price. `fill` shades under it; `weight` is the line's.
void chart(toybox::Screen& screen, const fui::Rect& r, const mk::Quote& q, const int sessions, const bool fill,
           const float weight) {
  int n = sessions > 0 && sessions < q.count ? sessions : q.count;
  if (n < 2) return;
  const int from = q.count - n;
  auto value = [&](const int i) { return i == n - 1 ? static_cast<float>(q.price) : q.closes[from + i]; };
  float lo = value(0);
  float hi = lo;
  for (int i = 1; i < n; ++i) {
    const float v = value(i);
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  const float span = hi - lo > 0 ? hi - lo : 1;
  const int pad = static_cast<int>(weight) + 4;  // room for the line and the end dot
  const float x0 = static_cast<float>(r.x + pad);
  const float w = static_cast<float>(r.width - pad * 2);
  const float y0 = static_cast<float>(r.y + pad);
  const float h = static_cast<float>(r.height - pad * 2);
  auto px = [&](const int i) { return x0 + w * static_cast<float>(i) / static_cast<float>(n - 1); };
  auto py = [&](const int i) { return y0 + h * (hi - value(i)) / span; };

  if (fill) {
    const fui::Paint shade = fui::Paint::dither(fui::Color::LightGray);
    const int bottom = r.y + r.height;
    for (int x = static_cast<int>(x0); x <= static_cast<int>(x0 + w); ++x) {
      const float t = (static_cast<float>(x) - x0) / w * static_cast<float>(n - 1);
      int i = static_cast<int>(t);
      if (i >= n - 1) i = n - 2;
      const float f = t - static_cast<float>(i);
      const int y = static_cast<int>(py(i) + (py(i + 1) - py(i)) * f);
      box(screen, x, y, 1, bottom - y, shade);
    }
  }
  for (int i = 0; i + 1 < n; ++i) {
    stroke(screen, px(i), py(i), px(i + 1), py(i + 1), weight);
    // Round the joints, or a thick line shows a notch at every turn.
    if (weight >= 3)
      disc(screen, static_cast<int>(px(i + 1)), static_cast<int>(py(i + 1)), static_cast<int>(weight / 2),
           fui::Color::Black);
  }
  const int ex = static_cast<int>(px(n - 1));
  const int ey = static_cast<int>(py(n - 1));
  const int dot = static_cast<int>(weight) + 3;
  disc(screen, ex, ey, dot + 2, fui::Color::White);
  disc(screen, ex, ey, dot, fui::Color::Black);
  disc(screen, ex, ey, dot - 3, fui::Color::White);
}

// "OPEN 10:32 EST" / "CLOSED FRI 13 NOV" / "16 NOV 07:58 UTC".
void statusLine(const mk::Asset asset, const mk::Quote& q, char* out, const size_t cap) {
  const mk::Civil c = mk::civilOf(q.time, q.gmtOffset);
  if (mk::infoOf(asset).always) {
    // A time on the stroke of midnight is a candle's day, not a clock reading.
    if (q.time % 86400 == 0)
      std::snprintf(out, cap, "%d %s %s", c.day, mk::monthName(c.month), q.zone);
    else
      std::snprintf(out, cap, "%d %s %02d:%02d %s", c.day, mk::monthName(c.month), c.hour, c.minute, q.zone);
  } else if (mk::sessionOpen(q)) {
    std::snprintf(out, cap, "OPEN %02d:%02d %s", c.hour, c.minute, q.zone);
  } else if (q.sessionEnd == 0) {
    // The source does not say whether the exchange is open: give the day the price is from.
    std::snprintf(out, cap, "AS OF %d %s", c.day, mk::monthName(c.month));
  } else {
    std::snprintf(out, cap, "CLOSED %d %s", c.day, mk::monthName(c.month));
  }
}

void chrome(toybox::Screen& screen, const char* title) {
  fui::HeaderProps header;
  header.title = title;
  header.borderEdges = fui::EdgesNone;
  header.sidePadding = screen.theme().headerSidePadding;
  toybox::headerBand(screen, header);
}

void signedPercent(const mk::Quote& q, const int sessions, char* out, const size_t cap) {
  double pct = 0;
  if (mk::changeOver(q, sessions, pct)) {
    mk::formatPercent(pct, out, cap);
  } else {
    std::snprintf(out, cap, "-");
  }
}

// --- dashboard -----------------------------------------------------------------

void card(toybox::Screen& screen, const fui::Rect& r, const mk::Asset asset, const mk::Quote& q,
          const FaceSwap& faces) {
  const mk::AssetInfo& info = mk::infoOf(asset);
  fui::ButtonProps b;
  b.label = "";
  b.action = ActionOpen;
  b.value = static_cast<int16_t>(asset);
  b.styles = toybox::rowStyles();
  b.radius = kRadius;
  screen.button(b, r);

  const int pad = 16;
  const int x = r.x + pad;
  const int innerW = r.width - pad * 2;
  int y = r.y + 16;
  label(screen, rect(x, y, innerW / 2, toybox::kUiCut.inkHeight), info.name, toybox::kUiFont, toybox::kUiCut,
        fui::TextAlign::Left);
  if (!q.valid) {
    caption(screen, x, y + 6, innerW, "NO DATA YET", fui::TextAlign::Right);
    caption(screen, x, y + toybox::kUiCut.inkHeight + 10, innerW, info.caption);
    caption(screen, x, r.y + r.height / 2 + 10, innerW, "TAP REFRESH TO FETCH IT", fui::TextAlign::Center);
    return;
  }
  char status[40];
  statusLine(asset, q, status, sizeof(status));
  caption(screen, x, y + 6, innerW, status, fui::TextAlign::Right);
  y += toybox::kUiCut.inkHeight + 10;
  caption(screen, x, y, innerW / 2, info.caption);
  const int monthSessions = sessionsIn(Span::Month, asset, q);
  char week[16];
  char month[16];
  signedPercent(q, sessionsIn(Span::Week, asset, q), week, sizeof(week));
  signedPercent(q, monthSessions, month, sizeof(month));
  char trail[48];
  std::snprintf(trail, sizeof(trail), "WEEK %s   MONTH %s", week, month);
  caption(screen, x + innerW / 3, y, innerW - innerW / 3, trail, fui::TextAlign::Right);
  y += toybox::kTileCut.inkHeight + 14;

  // The price, the same cut on every card, with the day's move beside it.
  char price[32];
  mk::formatPriceShort(q.price, price, sizeof(price));
  const int pillH = 32;
  // What trades around the clock has no close: its move is over 24 hours, and says so.
  const bool rolling = q.dayAgo > 0;
  constexpr int kRollingTagW = 40;
  const int inkH = priceText(screen, faces, Face::Large, x, y, innerW - 146 - (rolling ? kRollingTagW : 0), price);
  const int pillW = changePill(screen, 0, y + (inkH - pillH) / 2, pillH, mk::dayChangePercent(q), x + innerW);
  if (rolling) {
    caption(screen, x + innerW - pillW - 8 - kRollingTagW, y + (inkH - toybox::kTileCut.inkHeight) / 2, kRollingTagW,
            "24H", fui::TextAlign::Right);
  }
  y += inkH + 8;

  // A month of closes along the foot, inside the card's outline.
  const int chartBottom = r.y + r.height - 3;
  chart(screen, rect(r.x + 3, y, r.width - 6, chartBottom - y), q, monthSessions, true, 2);
  // Which stretch that is, on a patch of paper in the chart's corner.
  const char* const stretch = "1 MONTH";
  const int tagW = textWidth(screen, stretch, toybox::kTileFont) + 12;
  const int tagH = toybox::kTileCut.inkHeight + 8;
  box(screen, r.x + 8, chartBottom - tagH - 4, tagW, tagH, fui::Paint::solid(fui::Color::White), 4);
  caption(screen, r.x + 14, chartBottom - tagH, tagW, stretch);
}

}  // namespace

int sessionsIn(const Span span, const mk::Asset asset, const mk::Quote& quote) {
  const bool always = mk::infoOf(asset).always;
  switch (span) {
    case Span::Week:
      return always ? 7 : 5;
    case Span::Month:
      return always ? 30 : 22;
    case Span::Quarter:
      return always ? 91 : 64;
    case Span::Year:
    default:
      return quote.count;
  }
}

void buildDashboard(toybox::Screen& screen, const DashboardModel& model) {
  chrome(screen, "MARKETS");
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int footH = 48;
  const int footY = device.height - kBottomReserve - footH - 4;
  const int top = toybox::kChromeHeight + 12;
  const int gap = 10;
  const int cardH = (footY - 12 - top - gap * (mk::kAssets - 1)) / mk::kAssets;
  for (int i = 0; i < mk::kAssets; ++i) {
    card(screen, rect(toybox::kMargin, top + i * (cardH + gap), inner, cardH), static_cast<mk::Asset>(i),
         model.quotes[i], model.faces);
  }

  // The foot: the portfolio, how often it fetches by itself, and fetching now.
  const int third = (inner - gap * 2) / 3;
  // With auto on and a fetch behind it, the button says when that was: the
  // only way to tell a page that is refreshing from one that has stopped.
  char autoLabel[toybox::kIntChars * 3 + 16];
  if (model.autoMinutes <= 0) {
    std::snprintf(autoLabel, sizeof(autoLabel), "AUTO OFF");
  } else if (model.fetchedHour >= 0) {
    std::snprintf(autoLabel, sizeof(autoLabel), "%d M  %02d:%02d", model.autoMinutes % 1000, model.fetchedHour % 24,
                  model.fetchedMinute % 60);
  } else if (model.autoMinutes >= 60) {
    std::snprintf(autoLabel, sizeof(autoLabel), "AUTO 1 H");
  } else {
    std::snprintf(autoLabel, sizeof(autoLabel), "AUTO %d MIN", model.autoMinutes % 1000);
  }
  fui::ButtonProps p;
  p.label = "PORTFOLIO";
  p.action = ActionPortfolio;
  p.styles = toybox::rowStyles();
  p.text = toybox::buttonText(screen.theme());
  p.radius = kRadius;
  screen.button(p, rect(toybox::kMargin, footY, third, footH));
  fui::ButtonProps a = p;
  a.label = autoLabel;
  a.action = ActionAuto;
  a.state = model.autoMinutes > 0 ? fui::StateSelected : fui::StateNormal;
  screen.button(a, rect(toybox::kMargin + third + gap, footY, third, footH));
  fui::ButtonProps refresh = p;
  refresh.label = "REFRESH";
  refresh.action = ActionRefresh;
  screen.button(refresh, rect(toybox::kMargin + (third + gap) * 2, footY, inner - (third + gap) * 2, footH));
}

namespace {

// A low-to-high range as a track with a marker where the price sits.
void rangeRow(toybox::Screen& screen, const int x, const int y, const int w, const char* name, const double lo,
              const double hi, const double at) {
  caption(screen, x, y + 6, 92, name);
  char a[32];
  char b[32];
  mk::formatPrice(lo, a, sizeof(a));
  mk::formatPrice(hi, b, sizeof(b));
  const int numW = 82;
  const int left = x + 92;
  caption(screen, left, y + 6, numW, a, fui::TextAlign::Right);
  caption(screen, x + w - numW, y + 6, numW, b, fui::TextAlign::Left);
  const int trackL = left + numW + 12;
  const int trackR = x + w - numW - 12;
  const int cy = y + 6 + toybox::kTileCut.inkHeight / 2;
  box(screen, trackL, cy - 2, trackR - trackL, 4, kInk, 2);
  double t = hi > lo ? (at - lo) / (hi - lo) : 0.5;
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  const int mx = trackL + static_cast<int>(t * (trackR - trackL));
  disc(screen, mx, cy, 9, fui::Color::White);
  disc(screen, mx, cy, 7, fui::Color::Black);
  disc(screen, mx, cy, 3, fui::Color::White);
}

void statCell(toybox::Screen& screen, const int x, const int y, const int w, const char* name, const char* value) {
  caption(screen, x, y, w, name, fui::TextAlign::Center);
  label(screen, rect(x, y + toybox::kTileCut.inkHeight + 10, w, toybox::kUiCut.inkHeight), value, toybox::kUiFont,
        toybox::kUiCut);
}

}  // namespace

void buildDetail(toybox::Screen& screen, const DetailModel& model) {
  const mk::AssetInfo& info = mk::infoOf(model.asset);
  chrome(screen, info.name);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int x = toybox::kMargin;
  {
    // In the band: swap this card for another instrument. Two arrows passing.
    const int side = 48;
    const fui::Rect r = rect(width - toybox::kMargin - side, (toybox::kChromeHeight - side) / 2 - 2, side, side);
    fui::ButtonProps b;
    b.label = "";
    b.action = ActionChange;
    b.styles = toybox::rowStyles();
    b.styles.normal.borderWidth = 0;
    b.minTouchSize = 0;
    screen.button(b, r);
    box(screen, r.x, r.y, side, side, fui::Paint::solid(fui::Color::White), kRadius);
    const int cx = r.x + side / 2;
    const int cy = r.y + side / 2;
    box(screen, cx - 13, cy - 8, 22, 3, kInk);
    box(screen, cx - 9, cy + 5, 22, 3, kInk);
    for (int i = 0; i < 6; ++i) {
      box(screen, cx + 9 + i, cy - 12 + i, 1, 11 - i * 2 > 0 ? 11 - i * 2 : 1, kInk);
      box(screen, cx - 10 - i, cy + 1 + i, 1, 11 - i * 2 > 0 ? 11 - i * 2 : 1, kInk);
    }
  }
  if (!model.quote || !model.quote->valid) {
    label(screen, rect(x, device.height / 2 - 40, inner, toybox::kDisplayCut.inkHeight), "NO DATA YET",
          toybox::kDisplayFont, toybox::kDisplayCut);
    return;
  }
  const mk::Quote& q = *model.quote;

  int y = toybox::kChromeHeight + 18;
  char status[40];
  statusLine(model.asset, q, status, sizeof(status));
  caption(screen, x, y, inner, info.caption);
  caption(screen, x, y, inner, status, fui::TextAlign::Right);
  y += toybox::kTileCut.inkHeight + 18;

  char price[32];
  mk::formatPrice(q.price, price, sizeof(price));
  const int inkH = priceText(screen, model.faces, Face::Huge, x, y, inner, price);
  y += inkH + 16;
  const int pillW = changePill(screen, x, y, 36, mk::dayChangePercent(q));
  char change[40];
  char abs[32];
  mk::formatChange(mk::dayChange(q), abs, sizeof(abs));
  std::snprintf(change, sizeof(change), "%s %s %s", abs, q.currency, q.dayAgo > 0 ? "IN 24H" : "TODAY");
  caption(screen, x + pillW + 12, y + (36 - toybox::kTileCut.inkHeight) / 2, inner - pillW - 12, change);
  y += 36 + 18;

  // The span tabs and the chart they slice.
  static const char* const kTabs[4] = {"1 WEEK", "1 MONTH", "3 MON", "1 YEAR"};
  const int gap = 6;
  const int tabW = (inner - gap * 3) / 4;
  for (int i = 0; i < 4; ++i) {
    fui::ButtonProps t;
    t.label = kTabs[i];
    t.action = ActionSpan;
    t.value = static_cast<int16_t>(i);
    t.styles = toybox::rowStyles();
    t.text = toybox::buttonText(screen.theme());
    t.radius = kRadius;
    t.state = i == static_cast<int>(model.span) ? fui::StateSelected : fui::StateNormal;
    screen.button(t, rect(x + i * (tabW + gap), y, tabW, 40));
  }
  y += 40 + 12;

  const int sessions = sessionsIn(model.span, model.asset, q);
  const int statsTop = device.height - kBottomReserve - 222;
  const int chartH = statsTop - 34 - y;
  const fui::Rect plot = rect(x, y, inner, chartH);
  chart(screen, plot, q, sessions, true, 3);
  box(screen, x, y + chartH, inner, toybox::kHairline, kInk);
  // The span's extremes at the chart's corners, its dates under it.
  float lo = 0;
  float hi = 0;
  mk::closeRange(q, sessions, lo, hi);
  if (static_cast<float>(q.price) > hi) hi = static_cast<float>(q.price);
  if (static_cast<float>(q.price) < lo) lo = static_cast<float>(q.price);
  char hiText[40];
  char loText[40];
  char num[32];
  mk::formatPriceShort(hi, num, sizeof(num));
  std::snprintf(hiText, sizeof(hiText), "HIGH %s", num);
  mk::formatPriceShort(lo, num, sizeof(num));
  std::snprintf(loText, sizeof(loText), "LOW %s", num);
  const int n = sessions > 0 && sessions < q.count ? sessions : q.count;
  int yy = 0;
  int mm = 0;
  int dd = 0;
  char fromText[24];
  char toText[24];
  mk::civilOfDay(q.days[q.count - n], yy, mm, dd);
  std::snprintf(fromText, sizeof(fromText), "%d %s", dd, mk::monthName(mm));
  mk::civilOfDay(q.days[q.count - 1], yy, mm, dd);
  std::snprintf(toText, sizeof(toText), "%d %s", dd, mk::monthName(mm));
  const int axisY = y + chartH + 9;
  caption(screen, x, axisY, inner / 3, fromText);
  caption(screen, x + inner - inner / 3, axisY, inner / 3, toText, fui::TextAlign::Right);
  char spanText[96];
  std::snprintf(spanText, sizeof(spanText), "%s   %s", loText, hiText);
  caption(screen, x + inner / 4, axisY, inner / 2, spanText, fui::TextAlign::Center);

  // Where the price sits in the day and in the year, then the numbers.
  y = statsTop;
  box(screen, x, y, inner, toybox::kHairline, kInk);
  y += 12;
  if (q.dayHigh > 0 && q.dayLow > 0) rangeRow(screen, x, y, inner, "TODAY", q.dayLow, q.dayHigh, q.price);
  y += 34;
  if (q.yearHigh > 0 && q.yearLow > 0) {
    // Coinbase hands over 300 daily candles, not a year of them.
    rangeRow(screen, x, y, inner, info.always ? "300 DAYS" : "52 WEEKS", q.yearLow, q.yearHigh, q.price);
  }
  y += 38;
  box(screen, x, y, inner, toybox::kHairline, kInk);
  y += 14;
  const int cellW = inner / 3;
  char prev[32];
  mk::formatPrice(mk::previousClose(q), prev, sizeof(prev));
  const bool rolling = q.dayAgo > 0;
  char volume[24];
  if (q.volume > 0) {
    mk::formatVolume(q.volume, volume, sizeof(volume));
  } else {
    std::snprintf(volume, sizeof(volume), "-");
  }
  char week[16];
  char month[16];
  char quarter[16];
  signedPercent(q, sessionsIn(Span::Week, model.asset, q), week, sizeof(week));
  signedPercent(q, sessionsIn(Span::Month, model.asset, q), month, sizeof(month));
  signedPercent(q, q.count, quarter, sizeof(quarter));
  statCell(screen, x, y, cellW, rolling ? "24H AGO" : "PREV CLOSE", prev);
  statCell(screen, x + cellW, y, cellW, "VOLUME", volume);
  statCell(screen, x + cellW * 2, y, cellW, "1 WEEK", week);
  box(screen, x + cellW, y, toybox::kHairline, 48, kInk);
  box(screen, x + cellW * 2, y, toybox::kHairline, 48, kInk);
  y += 70;
  statCell(screen, x, y, cellW, "1 MONTH", month);
  statCell(screen, x + cellW, y, cellW, "3 MONTHS", quarter);
  // How far under the year's high it stands: the one number the range bar
  // above shows but does not say.
  char fromHigh[16];
  if (q.yearHigh > 0) {
    mk::formatPercent((q.price - q.yearHigh) / q.yearHigh * 100.0, fromHigh, sizeof(fromHigh));
  } else {
    std::snprintf(fromHigh, sizeof(fromHigh), "-");
  }
  statCell(screen, x + cellW * 2, y, cellW, "FROM HIGH", fromHigh);
  box(screen, x + cellW, y, toybox::kHairline, 48, kInk);
  box(screen, x + cellW * 2, y, toybox::kHairline, 48, kInk);
}

void buildAutoPicker(toybox::Screen& screen, const int currentMinutes) {
  chrome(screen, "AUTO REFRESH");
  const fui::DeviceContext device = screen.device();
  const int inner = device.width - toybox::kMargin * 2;
  int y = toybox::kBodyTop - 12;
  caption(screen, toybox::kMargin, y, inner, "HOW OFTEN SHOULD IT FETCH BY ITSELF?");
  y += toybox::kTileCut.inkHeight + 16;
  for (int i = 0; i < kAutoChoiceCount; ++i) {
    const int minutes = kAutoChoices[i];
    char text[toybox::kIntChars + 16];
    if (minutes == 0) {
      std::snprintf(text, sizeof(text), "OFF");
    } else if (minutes == 60) {
      std::snprintf(text, sizeof(text), "EVERY HOUR");
    } else {
      std::snprintf(text, sizeof(text), "EVERY %d MINUTES", minutes);
    }
    fui::ButtonProps b;
    b.label = text;
    b.action = ActionChooseAuto;
    b.value = static_cast<int16_t>(minutes);
    b.styles = toybox::rowStyles();
    b.radius = kRadius;
    b.state = minutes == currentMinutes ? fui::StateSelected : fui::StateNormal;
    screen.button(b, rect(toybox::kMargin, y, inner, 60));
    y += 60 + 8;
  }
  y += 10;
  fui::TextStyle s;
  s.font = toybox::kTileFont;
  s.align = fui::TextAlign::Left;
  s.maxLines = 4;
  screen.target().text(rect(toybox::kMargin, y, inner, toybox::kTileCut.lineHeight * 4),
                       "WHILE IT IS ON, THE DEVICE STAYS AWAKE AND ON WI-FI, SO KEEP IT PLUGGED IN. OFF FETCHES ONLY "
                       "WHEN YOU TAP REFRESH.",
                       s);
}

void buildStatus(toybox::Screen& screen, const StatusModel& model) {
  chrome(screen, "MARKETS");
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int cy = device.height / 2 - 70;
  label(screen, rect(toybox::kMargin, cy, inner, toybox::kDisplayCut.inkHeight), model.headline, toybox::kDisplayFont,
        toybox::kDisplayCut);
  fui::TextStyle s;
  s.font = toybox::kTileFont;
  s.align = fui::TextAlign::Center;
  s.maxLines = 4;
  screen.target().text(rect(toybox::kMargin * 3, cy + toybox::kDisplayCut.inkHeight + 24, width - toybox::kMargin * 6,
                            toybox::kTileCut.lineHeight * 4),
                       model.detail, s);
  if (model.dismissable) {
    fui::ButtonProps ok;
    ok.label = "OK";
    ok.action = ActionDismiss;
    ok.styles = toybox::rowStyles();
    ok.radius = kRadius;
    screen.button(ok, rect(width / 2 - 90, cy + 170, 180, toybox::kPillHeight));
  }
}

void buildPortfolio(toybox::Screen& screen, const PortfolioModel& model) {
  chrome(screen, "PORTFOLIO");
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int x = toybox::kMargin;
  const int bottom = device.height - kBottomReserve - 6;
  const mk::Portfolio& pf = *model.portfolio;
  char a[40];
  char b[72];

  int y = toybox::kChromeHeight + 18;
  if (pf.count == 0) {
    label(screen, rect(x, y + 40, inner, toybox::kDisplayCut.inkHeight), "NOTHING YET", toybox::kDisplayFont,
          toybox::kDisplayCut, fui::TextAlign::Left);
    fui::TextStyle s;
    s.font = toybox::kTileFont;
    s.align = fui::TextAlign::Left;
    s.maxLines = 4;
    screen.target().text(rect(x, y + 40 + toybox::kDisplayCut.inkHeight + 22, inner, toybox::kTileCut.lineHeight * 4),
                         "ADD WHAT YOU HOLD AND HOW MUCH OF IT. THIS PAGE THEN SHOWS WHAT IT IS ALL WORTH AND HOW "
                         "TODAY WENT. IT STAYS ON THE CARD AND GOES NOWHERE ELSE.",
                         s);
  } else {
    const mk::Totals t = mk::totalsOf(pf, model.priced);
    caption(screen, x, y, inner, "TOTAL VALUE, USD");
    if (t.priced < pf.count) {
      std::snprintf(a, sizeof(a), "%d OF %d PRICED", t.priced % 100, pf.count % 100);
      caption(screen, x, y, inner, a, fui::TextAlign::Right);
    }
    y += toybox::kTileCut.inkHeight + 16;
    mk::formatPriceShort(t.value, a, sizeof(a));
    y += priceText(screen, model.faces, Face::Huge, x, y, inner, a) + 18;
    if (t.priced > 0) {
      const int pillW = changePill(screen, x, y, 40, mk::percentOf(t.value, t.previous));
      mk::formatChange(t.value - t.previous, a, sizeof(a));
      static const char* const kOver[mk::kPeriods] = {"TODAY", "IN A WEEK", "IN A MONTH", "THIS YEAR", "IN A YEAR"};
      std::snprintf(b, sizeof(b), "%s USD %s", a, kOver[static_cast<int>(model.period) % mk::kPeriods]);
      caption(screen, x + pillW + 14, y + (40 - toybox::kTileCut.inkHeight) / 2, inner - pillW - 14, b);
    }
    y += 40 + 12;
    {
      // What the change is counted over, here and on every row.
      static const char* const kTabs[mk::kPeriods] = {"DAY", "WEEK", "MON", "YTD", "YEAR"};
      const int tabGap = 6;
      const int tabW = (inner - tabGap * (mk::kPeriods - 1)) / mk::kPeriods;
      for (int i = 0; i < mk::kPeriods; ++i) {
        fui::ButtonProps tab;
        tab.label = kTabs[i];
        tab.action = ActionPeriod;
        tab.value = static_cast<int16_t>(i);
        tab.styles = toybox::rowStyles();
        tab.text = toybox::buttonText(screen.theme());
        tab.radius = kRadius;
        tab.minTouchSize = 0;
        tab.state = i == static_cast<int>(model.period) ? fui::StateSelected : fui::StateNormal;
        screen.button(tab, rect(x + i * (tabW + tabGap), y, tabW, 36));
      }
    }
    y += 36 + 12;
    if (t.priced > 0 && model.euro > 0) {
      // The same total in euros, at the rate fetched with the quotes.
      mk::formatPriceShort(t.value / model.euro, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s EUR", a);
      label(screen, rect(x, y, inner / 2, toybox::kUiCut.inkHeight), b, toybox::kUiFont, toybox::kUiCut,
            fui::TextAlign::Left);
      char rate[24];
      std::snprintf(rate, sizeof(rate), "1 EUR = %.4f USD", model.euro);
      caption(screen, x + inner / 2, y + (toybox::kUiCut.inkHeight - toybox::kTileCut.inkHeight) / 2, inner / 2, rate,
              fui::TextAlign::Right, fui::Color::DarkGray);
      y += toybox::kUiCut.inkHeight + 16;
    }
    box(screen, x, y, inner, toybox::kHairline, kInk);
    y += 10;

    // A row a holding: what and how much on the left, its worth and its day
    // on the right. The whole row opens the keypad.
    int rowH = (bottom - 48 - 8 - y) / (pf.count > 0 ? pf.count : 1);
    if (rowH > 62) rowH = 62;
    for (int i = 0; i < pf.count; ++i) {
      const mk::AssetInfo& info = mk::instrumentAt(pf.positions[i].instrument);
      const fui::Rect r = rect(x, y, inner, rowH);
      fui::ButtonProps row;
      row.label = "";
      row.action = ActionPosition;
      row.value = static_cast<int16_t>(i);
      row.styles = toybox::rowStyles();
      row.styles.normal.borderWidth = 0;
      row.minTouchSize = 0;
      screen.button(row, r);
      const int ty = y + (rowH >= 60 ? 8 : 5);
      const int cy = ty + toybox::kUiCut.inkHeight + (rowH >= 60 ? 8 : 6);
      label(screen, rect(x, ty, inner / 2, toybox::kUiCut.inkHeight), info.name, toybox::kUiFont, toybox::kUiCut,
            fui::TextAlign::Left);
      mk::formatAmount(pf.positions[i].amount, a, sizeof(a));
      const mk::Priced& pr = model.priced[i];
      if (pr.valid) {
        char price[24];
        mk::formatPrice(pr.price, price, sizeof(price));
        std::snprintf(b, sizeof(b), "%s AT %s", a, price);
        caption(screen, x, cy, inner * 2 / 3, b, fui::TextAlign::Left, fui::Color::DarkGray);
        mk::formatPriceShort(pf.positions[i].amount * pr.price, a, sizeof(a));
        label(screen, rect(x + inner / 2, ty, inner / 2, toybox::kUiCut.inkHeight), a, toybox::kUiFont, toybox::kUiCut,
              fui::TextAlign::Right);
        mk::formatPercent(mk::percentOf(pr.price, pr.previous), a, sizeof(a));
        caption(screen, x + inner * 2 / 3, cy, inner / 3, a, fui::TextAlign::Right);
      } else {
        caption(screen, x, cy, inner * 2 / 3, a, fui::TextAlign::Left, fui::Color::DarkGray);
        caption(screen, x + inner / 2, ty + 6, inner / 2, "REFRESH TO PRICE IT", fui::TextAlign::Right,
                fui::Color::DarkGray);
      }
      y += rowH;
      box(screen, x, y - 1, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
    }
  }

  // The foot: one more holding, and new prices.
  const int footH = 48;
  const int gap = 10;
  const int half = (inner - gap) / 2;
  fui::ButtonProps add;
  add.label = "ADD";
  add.action = ActionAddPosition;
  add.enabled = pf.count < mk::kMaxPositions;
  add.styles = toybox::rowStyles();
  add.text = toybox::buttonText(screen.theme());
  add.radius = kRadius;
  screen.button(add, rect(x, bottom - footH, half, footH));
  fui::ButtonProps refresh = add;
  refresh.label = "REFRESH";
  refresh.action = ActionRefresh;
  refresh.enabled = true;
  screen.button(refresh, rect(x + half + gap, bottom - footH, half, footH));
}

void buildPicker(toybox::Screen& screen, const PickerModel& model) {
  chrome(screen, model.title);
  const fui::DeviceContext device = screen.device();
  const int inner = device.width - toybox::kMargin * 2;
  const int bottom = device.height - kBottomReserve - 6;
  const int gap = 6;
  const int colW = (inner - gap) / 2;
  auto listed = [&](const int i) {
    if (!mk::present(i)) return false;
    const mk::AssetInfo& info = mk::instrumentAt(i);
    return !(model.ownableOnly && (!info.ownable || (model.held && mk::positionOf(*model.held, i) >= 0)));
  };
  int shown = 0;
  for (int i = 0; i < mk::kInstruments; ++i) shown += listed(i) ? 1 : 0;

  // Anything that is not in the list: by its ticker or by its name.
  int top = toybox::kChromeHeight + 10;
  fui::ButtonProps find;
  find.label = "SEARCH TICKER OR NAME";
  find.action = ActionSearch;
  find.styles = toybox::rowStyles();
  find.text = toybox::buttonText(screen.theme());
  find.radius = kRadius;
  find.state = fui::StateSelected;
  screen.button(find, rect(toybox::kMargin, top, inner, 44));
  top += 44 + 10;

  const int rows = (shown + 1) / 2;
  int rowH = rows > 0 ? (bottom - top - gap * (rows - 1)) / rows : 56;
  if (rowH > 60) rowH = 60;
  int at = 0;
  for (int i = 0; i < mk::kInstruments; ++i) {
    if (!listed(i)) continue;
    fui::ButtonProps b;
    b.label = mk::instrumentAt(i).name;
    b.action = ActionPick;
    b.value = static_cast<int16_t>(i);
    b.styles = toybox::rowStyles();
    b.text = toybox::buttonText(screen.theme());
    b.radius = kRadius;
    b.minTouchSize = 0;
    b.state = i == model.current ? fui::StateSelected : fui::StateNormal;
    screen.button(b, rect(toybox::kMargin + (at % 2) * (colW + gap), top + (at / 2) * (rowH + gap), colW, rowH));
    ++at;
  }
}

void buildResults(toybox::Screen& screen, const ResultsModel& model) {
  chrome(screen, "SEARCH");
  const fui::DeviceContext device = screen.device();
  const int inner = device.width - toybox::kMargin * 2;
  const int x = toybox::kMargin;
  const int bottom = device.height - kBottomReserve - 6;
  int y = toybox::kChromeHeight + 14;
  label(screen, rect(x, y, inner, toybox::kUiCut.inkHeight), model.query, toybox::kUiFont, toybox::kUiCut,
        fui::TextAlign::Left);
  y += toybox::kUiCut.inkHeight + 10;
  const char* said = "TAP ONE TO USE IT.";
  if (model.count < 0) said = "THE SEARCH DID NOT ANSWER.";
  if (model.count == 0) said = "NOTHING FOUND UNDER THAT.";
  caption(screen, x, y, inner, said, fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 12;
  box(screen, x, y, inner, toybox::kHairline, kInk);
  y += 8;

  const int footH = model.asTyped ? 48 + 30 : 0;
  const int rowH = 58;
  for (int i = 0; i < model.count && y + rowH <= bottom - footH; ++i) {
    const mk::Found& f = model.found[i];
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionFound;
    row.value = static_cast<int16_t>(i);
    row.styles = toybox::rowStyles();
    row.styles.normal.borderWidth = 0;
    row.minTouchSize = 0;
    screen.button(row, rect(x, y, inner, rowH));
    label(screen, rect(x, y + 7, inner * 2 / 3, toybox::kUiCut.inkHeight), f.symbol, toybox::kUiFont, toybox::kUiCut,
          fui::TextAlign::Left);
    caption(screen, x + inner / 2, y + 13, inner / 2, f.exchange, fui::TextAlign::Right, fui::Color::DarkGray);
    caption(screen, x, y + 7 + toybox::kUiCut.inkHeight + 7, inner, f.company);
    y += rowH;
    box(screen, x, y - 1, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
  }

  if (model.asTyped) {
    // For when the lookup is no help: a symbol CNBC quotes, exactly as typed.
    caption(screen, x, bottom - 48 - 22, inner, "CNBC SYMBOLS: MADRID SAN-ES, XETRA SAP-DE.", fui::TextAlign::Left,
            fui::Color::DarkGray);
    char use[40];
    std::snprintf(use, sizeof(use), "USE %s AS TYPED", model.query);
    fui::ButtonProps typed;
    typed.label = use;
    typed.action = ActionFound;
    typed.value = -1;
    typed.styles = toybox::rowStyles();
    typed.text = toybox::buttonText(screen.theme());
    typed.radius = kRadius;
    screen.button(typed, rect(x, bottom - 48, inner, 48));
  }
}

void buildAmount(toybox::Screen& screen, const AmountModel& model) {
  const mk::AssetInfo& info = mk::instrumentAt(model.instrument);
  chrome(screen, info.name);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int x = toybox::kMargin;
  const int bottom = device.height - kBottomReserve - 6;

  int y = toybox::kChromeHeight + 18;
  caption(screen, x, y, inner, info.always ? "HOW MANY COINS DO YOU HOLD?" : "HOW MANY SHARES DO YOU HOLD?");
  y += toybox::kTileCut.inkHeight + 14;
  // The figure being typed, in a well.
  const int wellH = 96;
  screen.target().stroke(rect(x, y, inner, wellH), kInk, 3, kRadius);
  const bool empty = model.text[0] == 0;
  bind(model.faces, Face::Large);
  label(screen, rect(x + 18, y, inner - 36, wellH), empty ? "0" : model.text, toybox::kDisplayFont, toybox::kLargeCut,
        fui::TextAlign::Right, empty ? fui::Color::DarkGray : fui::Color::Black);
  bind(model.faces, Face::Display);
  y += wellH + 14;

  // Twelve keys, then what to do with the figure.
  const int footH = 56;
  const int gap = 8;
  const int keysBottom = bottom - footH - 14;
  const int keyW = (inner - gap * 2) / 3;
  const int keyH = (keysBottom - y - gap * 3) / 4;
  static const char kKeys[12] = {'7', '8', '9', '4', '5', '6', '1', '2', '3', '.', '0', '<'};
  for (int i = 0; i < 12; ++i) {
    const char text[2] = {kKeys[i], 0};
    fui::ButtonProps b;
    b.label = kKeys[i] == '<' ? "DEL" : text;
    b.action = ActionKey;
    b.value = static_cast<int16_t>(kKeys[i]);
    b.styles = toybox::rowStyles();
    b.radius = kRadius;
    if (kKeys[i] == '<') b.text = toybox::buttonText(screen.theme());
    screen.button(b, rect(x + (i % 3) * (keyW + gap), y + (i / 3) * (keyH + gap), keyW, keyH));
  }
  const int half = (inner - gap) / 2;
  fui::ButtonProps save;
  save.label = "SAVE";
  save.action = ActionSaveAmount;
  save.styles = toybox::rowStyles();
  save.radius = kRadius;
  save.state = fui::StateSelected;
  if (model.held) {
    fui::ButtonProps remove = save;
    remove.label = "REMOVE";
    remove.action = ActionRemovePosition;
    remove.state = fui::StateNormal;
    remove.text = toybox::buttonText(screen.theme());
    screen.button(remove, rect(x, bottom - footH, half, footH));
    screen.button(save, rect(x + half + gap, bottom - footH, half, footH));
  } else {
    screen.button(save, rect(x, bottom - footH, inner, footH));
  }
}

}  // namespace marketsui

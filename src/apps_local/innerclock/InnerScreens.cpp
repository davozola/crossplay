#include "InnerScreens.h"

#include <cstdio>

#include "../ui/ToyboxFormat.h"

namespace innerui {

namespace {

namespace ic = innerclock;

constexpr int16_t kTabsTop = toybox::kChromeHeight + toybox::kGutter * 2;

// Seven segments, a..g. Bit 0 is a.
constexpr uint8_t kSegments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// Seven-segment numbers with a decimal point and a sign, centred in `box`, at
// height `want` or smaller if the text would not fit across: "10.00" is a
// glyph wider than "5.23".
void segments(toybox::Screen& screen, const fui::Rect& box, const char* text, const int want, const fui::Color color) {
  int h = want;
  int w = 0, t = 0, gap = 0, dotW = 0, total = 0;
  for (;;) {
    w = h * 55 / 100;
    t = h / 9 > 4 ? h / 9 : 4;
    gap = t * 2;
    dotW = t * 2;
    total = 0;
    for (const char* c = text; *c; ++c) total += (*c == '.' ? dotW : w) + gap;
    total -= gap;
    if (total <= box.width || h <= 20) break;
    h -= 6;
  }
  int x = box.x + (box.width - total) / 2;
  const int y = box.y + (box.height - h) / 2;
  auto& target = screen.target();
  const fui::Paint ink = fui::Paint::solid(color);
  auto rect = [&](const int rx, const int ry, const int rw, const int rh) {
    target.fill(fui::makeRect(static_cast<int16_t>(rx), static_cast<int16_t>(ry), static_cast<int16_t>(rw),
                              static_cast<int16_t>(rh)),
                ink);
  };
  const int mid = h / 2;
  const int arm = mid - t - t / 2;
  for (const char* c = text; *c; ++c) {
    if (*c == '.') {
      rect(x + (dotW - t) / 2, y + h - t, t, t);
      x += dotW + gap;
      continue;
    }
    if (*c == '-' || *c == '+') {
      rect(x + t, y + mid - t / 2, w - 2 * t, t);
      if (*c == '+') rect(x + (w - t) / 2, y + mid - (w - 2 * t) / 2, t, w - 2 * t);
      x += w + gap;
      continue;
    }
    if (*c >= '0' && *c <= '9') {
      const uint8_t s = kSegments[*c - '0'];
      if (s & 0x01) rect(x + t, y, w - 2 * t, t);
      if (s & 0x02) rect(x + w - t, y + t, t, arm);
      if (s & 0x04) rect(x + w - t, y + mid + t / 2, t, arm);
      if (s & 0x08) rect(x + t, y + h - t, w - 2 * t, t);
      if (s & 0x10) rect(x, y + mid + t / 2, t, arm);
      if (s & 0x20) rect(x, y + t, t, arm);
      if (s & 0x40) rect(x + t, y + mid - t / 2, w - 2 * t, t);
    }
    x += w + gap;
  }
}

void line(toybox::Screen& screen, const fui::Rect& box, const char* text, const fui::FontId font,
          const toybox::CutMetrics& cut, const fui::Color color, const uint8_t maxLines = 1) {
  fui::TextStyle s;
  s.font = font;
  s.align = fui::TextAlign::Center;
  s.color = color;
  s.maxLines = maxLines;
  screen.target().text(maxLines == 1 ? toybox::inkCentred(box, cut) : box, text, s);
}

}  // namespace

fui::Rect padRect(const fui::DeviceContext& device) {
  const int16_t top = static_cast<int16_t>(kTabsTop + toybox::kPillHeight + toybox::kGutter * 2);
  const int16_t bottom = static_cast<int16_t>(device.height - toybox::kMargin - 40);
  return fui::makeRect(toybox::kMargin, top, static_cast<int16_t>(device.width - toybox::kMargin * 2),
                       static_cast<int16_t>(bottom - top));
}

void build(toybox::Screen& screen, const Model& model) {
  const ic::Record empty{};
  const ic::Record& rec = model.record ? *model.record : empty;
  const int target = ic::kTargets[model.targetIndex];

  char right[toybox::kIntChars + 3];
  std::snprintf(right, sizeof(right), "%d S", target);
  fui::HeaderProps header;
  header.title = "INNER CLOCK";
  header.rightLabel = right;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);

  const fui::DeviceContext device = screen.device();
  auto& t = screen.target();

  // The two targets. Fixed while a round runs: changing the question halfway
  // through an answer would be a strange thing to allow.
  const int16_t tabW = static_cast<int16_t>((device.width - toybox::kMargin * 2 - toybox::kGutter) / 2);
  static const char* const kTabs[ic::kTargetCount] = {"5 SECONDS", "10 SECONDS"};
  for (int i = 0; i < ic::kTargetCount; ++i) {
    fui::ButtonProps b;
    b.label = kTabs[i];
    b.action = ActionTarget;
    b.value = static_cast<int16_t>(i);
    b.styles = toybox::rowStyles();
    b.enabled = model.state != ic::State::Running;
    b.state = i == model.targetIndex ? fui::StateSelected : fui::StateNormal;
    screen.button(b, fui::makeRect(static_cast<int16_t>(toybox::kMargin + i * (tabW + toybox::kGutter)), kTabsTop, tabW,
                                   toybox::kPillHeight));
  }

  const fui::Rect pad = padRect(device);
  const bool running = model.state == ic::State::Running;
  if (running) {
    t.fill(pad, fui::Paint::solid(fui::Color::Black), 18);
  } else {
    t.stroke(pad, fui::Paint::solid(fui::Color::Black), 4, 18);
  }
  const fui::Color ink = running ? fui::Color::White : fui::Color::Black;
  const int16_t px = static_cast<int16_t>(pad.x + 20);
  const int16_t pw = static_cast<int16_t>(pad.width - 40);

  char best[48];
  const int32_t b = rec.bestMs[model.targetIndex];
  if (b < 0) {
    std::snprintf(best, sizeof(best), "NO BEST YET AT %d", target);
  } else {
    char diff[16];
    ic::formatSeconds(static_cast<uint32_t>(b), diff, sizeof(diff));
    std::snprintf(best, sizeof(best), "BEST AT %d: OFF BY %s", target, diff);
  }

  switch (model.state) {
    case ic::State::Ready: {
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + 70), pw, 70), "TAP TO START", toybox::kDisplayFont,
           toybox::kDisplayCut, ink);
      char how[96];
      std::snprintf(how, sizeof(how), "THEN TAP AGAIN WHEN YOU THINK %d SECONDS HAVE PASSED. NOTHING COUNTS ON SCREEN.",
                    target);
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + 170), pw, 170), how, toybox::kBodyFont,
           toybox::kDisplayCut, ink, 4);
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.bottom() - 70), pw, 40), best, toybox::kTileFont,
           toybox::kTileCut, ink);
      break;
    }
    case ic::State::Running: {
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + pad.height / 2 - 80), pw, 70), "COUNTING",
           toybox::kDisplayFont, toybox::kDisplayCut, ink);
      char at[32];
      std::snprintf(at, sizeof(at), "TAP AT %d SECONDS", target);
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + pad.height / 2 + 10), pw, 40), at, toybox::kTileFont,
           toybox::kTileCut, ink);
      break;
    }
    case ic::State::Result: {
      char elapsed[16];
      ic::formatSeconds(model.elapsedMs, elapsed, sizeof(elapsed));
      segments(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + 24), pw, 140), elapsed, 130, ink);
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + 170), pw, 30), "SECONDS", toybox::kTileFont,
           toybox::kTileCut, ink);
      char diff[16];
      ic::formatDiff(model.diffMs, diff, sizeof(diff));
      segments(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + 214), pw, 80), diff, 70, ink);
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + 300), pw, 60),
           model.newBest ? "NEW BEST" : ic::verdict(model.diffMs), toybox::kDisplayFont, toybox::kDisplayCut, ink);
      if (model.newBest) {
        line(screen, fui::makeRect(px, static_cast<int16_t>(pad.y + 356), pw, 30), ic::verdict(model.diffMs),
             toybox::kTileFont, toybox::kTileCut, ink);
      }
      // The last few tries at this target, newest first.
      char recent[80] = "LAST ";
      int at = 5;
      int shown = 0;
      for (int i = 0; i < rec.recentCount && i < ic::kRecent; ++i) {
        if (rec.recent[i].target != target) continue;
        char d[16];
        ic::formatDiff(rec.recent[i].diffMs, d, sizeof(d));
        at += std::snprintf(recent + at, sizeof(recent) - static_cast<size_t>(at), "%s%s", shown++ ? "  " : "", d);
        if (at >= static_cast<int>(sizeof(recent)) - 1) break;
      }
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.bottom() - 110), pw, 36), recent, toybox::kTileFont,
           toybox::kTileCut, ink);
      line(screen, fui::makeRect(px, static_cast<int16_t>(pad.bottom() - 70), pw, 40), "TAP TO GO AGAIN",
           toybox::kTileFont, toybox::kTileCut, ink);
      break;
    }
  }
}

}  // namespace innerui

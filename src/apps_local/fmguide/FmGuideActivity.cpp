#include "FmGuideActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"
#include "SynthFigures.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace fg = fmguide;

namespace {

enum : fui::ActionId {
  ActionGuide = 1,  // value: the guide
  ActionChapter,    // value: the chapter
  ActionResume,
  ActionPart,  // value: the part
};

constexpr char kSavePath[] = "/.crosspoint/fm1guias.sav";
constexpr int kBottomReserve = 40;
constexpr int kBackSwipeEdge = 28;
constexpr uint8_t kCleanAfterPaints = 8;

// The faces a page is set in. The serif carries the accents; the pixel face is
// for what is printed on the synth and reads as a label.
struct Face {
  int font;
  int lineHeight;
  int ascender;
  int inkHeight;
};
constexpr Face kBody{toybox::kReadingSmallFontId, toybox::kReadingSmallCut.lineHeight,
                     toybox::kReadingSmallCut.ascender, toybox::kReadingSmallCut.inkHeight};
constexpr Face kBold{toybox::kReadingBoldSmallFontId, toybox::kReadingBoldSmallCut.lineHeight,
                     toybox::kReadingBoldSmallCut.ascender, toybox::kReadingBoldSmallCut.inkHeight};
constexpr Face kHead{toybox::kReadingBoldFontId, toybox::kReadingBoldCut.lineHeight, toybox::kReadingBoldCut.ascender,
                     toybox::kReadingBoldCut.inkHeight};
constexpr Face kTiny{toybox::kTileFontId, toybox::kTileCut.lineHeight, toybox::kTileCut.ascender,
                     toybox::kTileCut.inkHeight};
// Body lines are set a little closer than the cut's own advance: a page of a
// manual is denser than a page of a novel.
constexpr int kBodyLine = 28;

// One line of `face`, its top at `y`.
void line(GfxRenderer& r, const Face& face, const int x, const int y, const char* text, const bool black = true) {
  r.drawText(face.font, x, y, text, black);
}

// One line with its ink centred on `cy`.
void centred(GfxRenderer& r, const Face& face, const int cx, const int cy, const char* text, const bool black = true) {
  const int w = r.getTextWidth(face.font, text);
  r.drawText(face.font, cx - w / 2, cy - face.inkHeight / 2 - (face.ascender - face.inkHeight), text, black);
}

// Wraps `text` to `w` at spaces and, when `draw`, sets it from (x, y). Returns
// the height it takes. The same walk measures and draws, so a page is never
// laid out one way and painted another.
int flow(GfxRenderer& r, const Face& face, const int lineHeight, const int x, const int y, const int w,
         const char* text, const bool draw, const bool black = true) {
  char out[160];
  int used = 0;  // bytes of the line being built
  int lines = 0;
  const char* p = text;
  auto flush = [&]() {
    if (used == 0) return;
    out[used] = 0;
    if (draw) line(r, face, x, y + lines * lineHeight, out, black);
    ++lines;
    used = 0;
  };
  while (*p) {
    while (*p == ' ') ++p;
    const char* end = p;
    while (*end && *end != ' ') ++end;
    const int len = static_cast<int>(end - p);
    if (len == 0) break;
    // The line with this word added.
    const int need = used + (used ? 1 : 0) + len;
    if (need < static_cast<int>(sizeof(out))) {
      const int before = used;
      if (used) out[used++] = ' ';
      std::memcpy(out + used, p, static_cast<size_t>(len));
      used += len;
      out[used] = 0;
      if (before > 0 && r.getTextWidth(face.font, out) > w) {
        // It does not fit: end the line before it and start the next with it.
        used = before;
        flush();
        std::memcpy(out, p, static_cast<size_t>(len));
        used = len;
      }
    } else {
      flush();
      const int take = len < static_cast<int>(sizeof(out)) - 1 ? len : static_cast<int>(sizeof(out)) - 1;
      std::memcpy(out, p, static_cast<size_t>(take));
      used = take;
    }
    p = end;
  }
  flush();
  return lines * lineHeight;
}

void disc(GfxRenderer& r, const int cx, const int cy, const int radius, const bool ink) {
  r.fillRoundedRect(cx - radius, cy - radius, radius * 2, radius * 2, radius, ink ? Color::Black : Color::White);
}

// --- The FM-1's panel, as a drawing that lights what the text is about. ---

constexpr int kPanelH = 232;

void knob(GfxRenderer& r, const int cx, const int cy, const bool lit, const char* inside) {
  if (lit) {
    disc(r, cx, cy, 20, true);
    disc(r, cx, cy, 18, false);
    disc(r, cx, cy, 15, true);
  } else {
    disc(r, cx, cy, 15, true);
    disc(r, cx, cy, 13, false);
  }
  if (inside) {
    centred(r, kTiny, cx, cy, inside, !lit);
  } else {
    // A pointer, as on the unit's own knobs.
    r.drawLine(cx, cy - 3, cx, cy - 12, 3, !lit);
  }
}

void key(GfxRenderer& r, const int x, const int y, const int w, const int h, const bool lit, const char* text) {
  if (lit) {
    r.fillRoundedRect(x - 2, y - 2, w + 4, h + 4, 7, Color::Black);
    r.fillRoundedRect(x, y, w, h, 5, Color::White);
    r.fillRoundedRect(x + 2, y + 2, w - 4, h - 4, 4, Color::Black);
  } else {
    r.drawRoundedRect(x, y, w, h, 1, 5, true);
  }
  centred(r, kTiny, x + w / 2, y + h / 2, text, !lit);
}

// The 27 keys in a strip: 16 white, 11 black, numbered when `numbers`.
void keyboard(GfxRenderer& r, const int x, const int y, const int w, const int h, const uint32_t mask,
              const bool numbers) {
  const int keyW = w / fg::kWhiteKeys;
  const int blackW = keyW * 2 / 3;
  const int blackH = h * 58 / 100;
  char text[toybox::kIntChars + 1];
  for (int i = 0; i < fg::kWhiteKeys; ++i) {
    const int kx = x + i * keyW;
    const bool lit = (mask >> i) & 1u;
    r.drawRect(kx, y, keyW + 1, h, 1, true);
    if (lit) r.fillRectDither(kx + 2, y + 2, keyW - 3, h - 4, Color::DarkGray);
    if (numbers) {
      std::snprintf(text, sizeof(text), "%d", (i + 1) % 100);
      const int cy = y + h - 14;
      if (lit) r.fillRect(kx + 3, cy - 10, keyW - 5, 20, false);
      centred(r, kTiny, kx + keyW / 2 + 1, cy, text);
    }
  }
  for (int i = 0; i < fg::kBlackKeys; ++i) {
    const int bx = x + (fg::kBlackAfter[i] + 1) * keyW - blackW / 2;
    const bool lit = (mask >> (16 + i)) & 1u;
    r.fillRect(bx, y, blackW, blackH, true);
    if (lit) {
      // A lit black key is hollowed out, with a mark above it.
      r.fillRect(bx + 3, y + 3, blackW - 6, blackH - 6, false);
      r.fillRectDither(bx + 3, y + 3, blackW - 6, blackH - 6, Color::DarkGray);
    }
    if (numbers) {
      std::snprintf(text, sizeof(text), "%d", (i + 1) % 100);
      centred(r, kTiny, bx + blackW / 2, y - 12, text);
      if (lit) r.fillRect(bx, y - 3, blackW, 3, true);
    }
  }
}

void panel(GfxRenderer& r, const int x, const int y, const int w, const uint32_t mask) {
  auto lit = [&](const uint32_t control) { return (mask & control) != 0; };
  r.drawRoundedRect(x, y, w, kPanelH, 2, 12, true);

  // The four knobs on the left, their names under them, and the two octave
  // buttons below.
  const int lx[2] = {x + 36, x + 102};
  const int ly[2] = {y + 30, y + 92};
  const uint32_t left[4] = {fg::cMaster, fg::cSelect, fg::cPresets, fg::cAlgorithm};
  for (int i = 0; i < 4; ++i) {
    knob(r, lx[i % 2], ly[i / 2], lit(left[i]), nullptr);
    centred(r, kTiny, lx[i % 2], ly[i / 2] + 31, fg::controlLabel(i));
  }
  key(r, x + 8, y + 134, 58, 26, lit(fg::cOctDown), fg::controlLabel(4));
  key(r, x + 72, y + 134, 58, 26, lit(fg::cOctUp), fg::controlLabel(5));

  // To the right of them: the screen, the four knobs beside it, and the two
  // rows of six buttons underneath. On the unit the buttons sit under the
  // knobs alone; here they run under the screen too, so their names fit.
  const int rx = x + 142;
  const int rw = w - 142 - 8;
  if (lit(fg::cScreen)) {
    r.fillRoundedRect(rx, y + 8, 66, 50, 6, Color::Black);
    r.fillRoundedRect(rx + 5, y + 13, 56, 40, 3, Color::White);
    r.fillRectDither(rx + 7, y + 15, 52, 36, Color::DarkGray);
  } else {
    r.drawRoundedRect(rx, y + 8, 66, 50, 2, 6, true);
    r.drawRect(rx + 6, y + 14, 54, 38, 1, true);
  }
  const int kx = rx + 76;
  const int kw = rw - 76;
  for (int i = 0; i < 4; ++i) {
    knob(r, kx + kw * (2 * i + 1) / 8, y + 33, lit(fg::cKnob1 << i), fg::controlLabel(6 + i));
  }
  const int gap = 4;
  const int bw = (rw - gap * 5) / 6;
  for (int i = 0; i < 12; ++i) {
    key(r, rx + (i % 6) * (bw + gap), y + 68 + (i / 6) * 34, bw, 28, lit(fg::cFx << i), fg::controlLabel(10 + i));
  }

  // The keys along the foot.
  const int ky = y + 176;
  keyboard(r, x + 12, ky, w - 24, 44, 0, false);
  if (lit(fg::cKeys)) r.drawRoundedRect(x + 7, ky - 6, w - 14, 56, 3, 6, true);
}

// --- The other figures. ---

constexpr int kKeysH = 118;
constexpr int kFlowRowH = 46;
constexpr int kGridRowH = 26;

void flowDiagram(GfxRenderer& r, const int x, const int y, const int w, const char* spec) {
  const int rows = fg::rowsOf(spec);
  for (int row = 0; row < rows; ++row) {
    char text[96];
    if (!fg::rowAt(spec, row, text, sizeof(text))) continue;
    // Count the boxes, then lay them out evenly with arrows between.
    int boxes = 1;
    for (const char* p = text; *p; ++p) boxes += *p == '>' ? 1 : 0;
    const int arrow = 22;
    const int bw = (w - arrow * (boxes - 1)) / boxes;
    const int by = y + row * (kFlowRowH + 10);
    char* start = text;
    for (int i = 0; i < boxes; ++i) {
      char* end = std::strchr(start, '>');
      if (end) *end = 0;
      const int bx = x + i * (bw + arrow);
      r.drawRoundedRect(bx, by, bw, kFlowRowH, 2, 8, true);
      centred(r, kTiny, bx + bw / 2, by + kFlowRowH / 2, start);
      if (i + 1 < boxes) {
        const int ax = bx + bw + 3;
        const int ay = by + kFlowRowH / 2;
        r.drawLine(ax, ay, ax + arrow - 8, ay, 2, true);
        for (int k = 0; k < 6; ++k) r.drawLine(ax + arrow - 6 - k, ay - k, ax + arrow - 6 - k, ay + k, 1, true);
      }
      if (!end) break;
      start = end + 1;
    }
  }
}

void stepGrid(GfxRenderer& r, const int x, const int y, const int w, const char* spec) {
  const int rows = fg::rowsOf(spec);
  const int nameW = 62;
  const int cell = (w - nameW) / 16;
  char text[toybox::kIntChars + 1];
  // The beats, numbered over the columns they start.
  for (int beat = 0; beat < 4; ++beat) {
    std::snprintf(text, sizeof(text), "%d", beat * 4 + 1);
    centred(r, kTiny, x + nameW + beat * 4 * cell + cell / 2, y + 8, text);
  }
  for (int row = 0; row < rows; ++row) {
    char line[40];
    if (!fg::rowAt(spec, row, line, sizeof(line))) continue;
    char* colon = std::strchr(line, ':');
    if (!colon) continue;
    *colon = 0;
    const int ry = y + 20 + row * kGridRowH;
    r.drawText(kTiny.font, x, ry + (kGridRowH - 4 - kTiny.inkHeight) / 2 - (kTiny.ascender - kTiny.inkHeight), line,
               true);
    for (int i = 0; i < 16; ++i) {
      const int cx = x + nameW + i * cell;
      const char c = colon[1 + i];
      const int side = cell - 4;
      if (c == '-') {
        r.drawRect(cx, ry, side, kGridRowH - 5, 1, true);
        if (i % 4 == 0) r.fillRect(cx, ry, side, 3, true);
      } else {
        r.fillRect(cx, ry, side, kGridRowH - 5, true);
        // An accent: a notch of paper in the corner.
        if (c == 'X') r.fillRect(cx + side - 8, ry + 3, 5, 5, false);
      }
    }
  }
}

// The teaching figures' canvas: the renderer, with the pixel face for labels.
class GfxCanvas final : public synthfig::Canvas {
 public:
  explicit GfxCanvas(GfxRenderer& r) : r_(r) {}
  void line(const int x0, const int y0, const int x1, const int y1, const int width) override {
    r_.drawLine(x0, y0, x1, y1, width, true);
  }
  void fill(const int x, const int y, const int w, const int h, const bool ink) override {
    r_.fillRect(x, y, w, h, ink);
  }
  void gray(const int x, const int y, const int w, const int h) override {
    r_.fillRectDither(x, y, w, h, Color::DarkGray);
  }
  void frame(const int x, const int y, const int w, const int h, const int width) override {
    r_.drawRect(x, y, w, h, width, true);
  }
  void text(const int x, const int cy, const char* text, const synthfig::Align align, const bool ink) override {
    const int w = r_.getTextWidth(kTiny.font, text);
    int left = x;
    if (align == synthfig::Align::Centre) left = x - w / 2;
    if (align == synthfig::Align::Right) left = x - w;
    r_.drawText(kTiny.font, left, cy - kTiny.inkHeight / 2 - (kTiny.ascender - kTiny.inkHeight), text, ink);
  }
  int textWidth(const char* text) override { return r_.getTextWidth(kTiny.font, text); }

 private:
  GfxRenderer& r_;
};

// A right-pointing arrowhead, solid, its tip at (x, cy).
void arrowHead(GfxRenderer& r, const int x, const int cy, const int size, const bool ink) {
  for (int k = 0; k < size; ++k) r.drawLine(x - size + k, cy - (size - k), x - size + k, cy + (size - k), 1, ink);
}

// One block at (x, y) in a column `w` wide; measures when not `draw`. Returns
// its height, the space after it included.
int block(GfxRenderer& r, const fg::Block& b, const int x, const int y, const int w, const bool draw) {
  switch (b.kind) {
    case fg::Kind::Head: {
      // White on a band of ink: the one thing on a page that is not body text.
      const int bandH = 34;
      if (draw) {
        r.fillRoundedRect(x, y + 10, w, bandH, 6, Color::Black);
        r.drawText(kBold.font, x + 12, y + 10 + (bandH - kBold.inkHeight) / 2 - (kBold.ascender - kBold.inkHeight), b.a,
                   false);
      }
      return 10 + bandH + 12;
    }
    case fg::Kind::Para:
      return flow(r, kBody, kBodyLine, x, y, w, b.a, draw) + 12;
    case fg::Kind::Bullet: {
      if (draw) r.fillRect(x + 6, y + 11, 7, 7, true);
      return flow(r, kBody, kBodyLine, x + 24, y, w - 24, b.a, draw) + 6;
    }
    case fg::Kind::Step: {
      if (draw) {
        disc(r, x + 14, y + 14, 14, true);
        centred(r, kTiny, x + 14, y + 14, b.a, false);
      }
      const int h = flow(r, kBody, kBodyLine, x + 38, y, w - 38, b.b, draw);
      return (h < 30 ? 30 : h) + 8;
    }
    case fg::Kind::Note: {
      const int textH = flow(r, kBody, kBodyLine, x + 14, y + 12 + 22, w - 28, b.b, false);
      const int h = 12 + 22 + textH + 10;
      if (draw) {
        r.drawRoundedRect(x, y, w, h, 2, 10, true);
        const int labelW = r.getTextWidth(kTiny.font, b.a) + 16;
        r.fillRoundedRect(x + 12, y + 9, labelW, 21, 5, Color::Black);
        centred(r, kTiny, x + 12 + labelW / 2, y + 19, b.a, false);
        flow(r, kBody, kBodyLine, x + 14, y + 12 + 22, w - 28, b.b, true);
      }
      return h + 14;
    }
    case fg::Kind::Row: {
      const int leftW = 170;
      const int leftH = flow(r, kBold, kBodyLine, x, y + 2, leftW, b.a, draw);
      const int rightH = flow(r, kBody, kBodyLine, x + leftW + 10, y + 2, w - leftW - 10, b.b, draw);
      const int h = (leftH > rightH ? leftH : rightH) + 8;
      if (draw) r.fillRectDither(x, y + h - 2, w, 1, Color::DarkGray);
      return h + 2;
    }
    case fg::Kind::Panel: {
      if (draw) panel(r, x, y + 4, w, b.mask);
      return 4 + kPanelH + 8 + flow(r, kBody, kBodyLine, x, y + 4 + kPanelH + 8, w, b.a, draw) + 12;
    }
    case fg::Kind::Keys: {
      if (draw) keyboard(r, x + 4, y + 26, w - 8, kKeysH - 30, b.mask, true);
      return kKeysH + 6 + flow(r, kBody, kBodyLine, x, y + kKeysH + 6, w, b.a, draw) + 12;
    }
    case fg::Kind::Flow: {
      const int rows = fg::rowsOf(b.a);
      const int h = rows * (kFlowRowH + 10);
      if (draw) flowDiagram(r, x, y + 4, w, b.a);
      return 4 + h + flow(r, kBody, kBodyLine, x, y + 4 + h, w, b.b, draw) + 12;
    }
    case fg::Kind::Grid: {
      const int rows = fg::rowsOf(b.a);
      const int h = 20 + rows * kGridRowH + 4;
      if (draw) stepGrid(r, x, y + 4, w, b.a);
      return 4 + h + flow(r, kBody, kBodyLine, x, y + 4 + h, w, b.b, draw) + 12;
    }
    case fg::Kind::Figure: {
      const int h = synthfig::height(static_cast<int>(b.mask), w);
      if (draw) {
        GfxCanvas canvas(r);
        synthfig::draw(canvas, static_cast<int>(b.mask), x, y + 6, w);
      }
      return 6 + h + 6 + flow(r, kBody, kBodyLine, x, y + 6 + h + 6, w, b.a, draw) + 12;
    }
    case fg::Kind::Danger: {
      // Heavier than a note, and headed by a band of ink: what it describes
      // destroys something.
      const int bandH = 26;
      const int textH = flow(r, kBody, kBodyLine, x + 16, y + bandH + 10, w - 32, b.b, false);
      const int h = bandH + 10 + textH + 10;
      if (draw) {
        r.fillRect(x, y, w, h, true);
        r.fillRect(x + 5, y + bandH, w - 10, h - bandH - 5, false);
        char label[40];
        std::snprintf(label, sizeof(label), "! %s", b.a);
        r.drawText(kTiny.font, x + 12, y + bandH / 2 - kTiny.inkHeight / 2 - (kTiny.ascender - kTiny.inkHeight), label,
                   false);
        flow(r, kBody, kBodyLine, x + 16, y + bandH + 10, w - 32, b.b, true);
      }
      return h + 14;
    }
    case fg::Kind::Link: {
      // A row to tap, the way a chapter is in the list: the place it opens and
      // an arrow.
      const int textH = flow(r, kBold, kBodyLine, x + 14, y + 10, w - 60, b.a, false);
      const int h = 10 + textH + 8;
      if (draw) {
        r.drawRoundedRect(x, y, w, h, 2, 8, true);
        flow(r, kBold, kBodyLine, x + 14, y + 10, w - 60, b.a, true);
        r.drawLine(x + w - 40, y + h / 2, x + w - 22, y + h / 2, 3, true);
        arrowHead(r, x + w - 14, y + h / 2, 8, true);
      }
      return h + 10;
    }
  }
  return 0;
}

}  // namespace

std::unique_ptr<Activity> FmGuideActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<FmGuideActivity>(renderer, mappedInput);
}

void FmGuideActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  load();
  view_ = View::Home;
  cleanNext_ = true;
  requestUpdate();
}

void FmGuideActivity::load() {
  for (fg::Place& place : places_) place = fg::Place{};
  last_ = -1;
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  uint8_t bytes[fg::kSaveBytes];
  if (!Storage.openFileForRead("FMG", kSavePath, file)) return;
  const int got = file.read(bytes, sizeof(bytes));
  fg::unpack(bytes, got, places_, last_);
}

void FmGuideActivity::save() {
  HalFile file;
  if (!Storage.openFileForWrite("FMG", kSavePath, file)) return;
  uint8_t bytes[fg::kSaveBytes];
  fg::pack(places_, last_, bytes);
  file.write(bytes, sizeof(bytes));
  file.flush();
}

int FmGuideActivity::bodyTop() const { return toybox::kChromeHeight + 46; }
int FmGuideActivity::bodyBottom() const { return renderer.getScreenHeight() - kBottomReserve - 22; }

// Fills pages greedily. A heading never ends a page: it goes over with what it
// heads.
void FmGuideActivity::paginate() {
  const fg::Chapter& chapter = fg::guideAt(guide_).chapters[chapter_];
  const int w = renderer.getScreenWidth() - toybox::kMargin * 2;
  const int room = bodyBottom() - bodyTop();
  pageCount_ = 0;
  pageStart_[0] = 0;
  int used = 0;
  for (int i = 0; i < chapter.count; ++i) {
    int h = block(renderer, chapter.blocks[i], 0, 0, w, false);
    int need = h;
    if (chapter.blocks[i].kind == fg::Kind::Head && i + 1 < chapter.count) {
      need += block(renderer, chapter.blocks[i + 1], 0, 0, w, false);
    }
    if (used > 0 && used + need > room && pageCount_ + 1 < kMaxPages) {
      pageStart_[++pageCount_] = static_cast<uint8_t>(i);
      used = 0;
    }
    used += h;
  }
  ++pageCount_;
  pageStart_[pageCount_] = static_cast<uint8_t>(chapter.count);
  if (page_ >= pageCount_) page_ = pageCount_ - 1;
  if (page_ < 0) page_ = 0;
  paged_ = true;
}

void FmGuideActivity::openChapter(const int chapter, const int page) {
  chapter_ = chapter;
  part_ = fg::partOf(fg::guideAt(guide_), chapter);
  page_ = page;
  paged_ = false;
  view_ = View::Read;
  cleanNext_ = true;
  last_ = guide_;
  places_[guide_].chapter = static_cast<uint8_t>(chapter_);
  places_[guide_].page = static_cast<uint8_t>(page_ < 0 ? 0 : page_);
  save();
}

FmGuideActivity::View FmGuideActivity::guideTop() const {
  return fg::guideAt(guide_).parts ? View::Parts : View::Chapters;
}

void FmGuideActivity::turn(const int direction) {
  const fg::Guide& guide = fg::guideAt(guide_);
  // A guide with parts is read a part at a time: its last page leads back to
  // the part's chapters, not on into the next part.
  int first = 0;
  int end = guide.count;
  if (part_ >= 0) {
    first = guide.parts[part_].first;
    end = first + guide.parts[part_].count;
  }
  if (direction > 0) {
    if (page_ + 1 < pageCount_) {
      ++page_;
    } else if (chapter_ + 1 < end) {
      openChapter(chapter_ + 1, 0);
      return;
    } else {
      view_ = View::Chapters;
      cleanNext_ = true;
      return;
    }
  } else {
    if (page_ > 0) {
      --page_;
    } else if (chapter_ > first) {
      // The last page of the chapter before; paginate() pulls it into range.
      openChapter(chapter_ - 1, kMaxPages);
      return;
    } else {
      return;
    }
  }
  places_[guide_].page = static_cast<uint8_t>(page_);
  save();
}

void FmGuideActivity::loop() {
  int px = 0;
  int py = 0;
  if (mappedInput.wasScreenTouchPressed(px, py)) pressX_ = px;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // A swipe to the right from the left quarter is the system's Back; while
    // reading it is the page before, unless it began at the very edge.
    if (view_ == View::Read && mappedInput.wasBackGesture() && pressX_ >= kBackSwipeEdge) {
      pressX_ = -1;
      turn(-1);
      requestUpdate();
      return;
    }
    switch (view_) {
      case View::Home:
        shelf::leave(renderer, mappedInput);
        return;
      case View::Read:
        view_ = View::Chapters;
        break;
      case View::Chapters:
        view_ = fg::guideAt(guide_).parts ? View::Parts : View::Home;
        break;
      case View::Parts:
        view_ = View::Home;
        break;
    }
    cleanNext_ = true;
    requestUpdate();
    return;
  }

  if (view_ == View::Read) {
    const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
    // Left or up is the next page, the way the page moves under a finger;
    // right or down is the one before.
    if (swipe != MappedInputManager::SwipeDir::None) {
      turn(swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Up ? 1 : -1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      turn(1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      turn(-1);
      requestUpdate();
      return;
    }
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  if (view_ == View::Read) {
    // The band with the firmware's name, and the chapter line under it, lead
    // back to the chapters this one is listed with.
    if (tapY < bodyTop()) {
      view_ = View::Chapters;
      cleanNext_ = true;
      requestUpdate();
      return;
    }
    // A link on the page opens the chapter it names.
    for (int i = 0; i < linkCount_; ++i) {
      if (tapY >= links_[i].top && tapY < links_[i].bottom) {
        openChapter(links_[i].chapter, 0);
        requestUpdate();
        return;
      }
    }
    // The left third goes back a page, the rest goes on.
    turn(tapX < renderer.getScreenWidth() / 3 ? -1 : 1);
    requestUpdate();
    return;
  }
  if (!interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);
  switch (event.action) {
    case ActionGuide:
      if (event.value < 0 || event.value >= fg::kGuides) return;
      guide_ = event.value;
      part_ = -1;
      view_ = guideTop();
      cleanNext_ = true;
      break;
    case ActionPart: {
      const fg::Guide& guide = fg::guideAt(guide_);
      if (event.value < 0 || event.value >= guide.partCount) return;
      part_ = event.value;
      view_ = View::Chapters;
      cleanNext_ = true;
      break;
    }
    case ActionChapter:
      if (event.value < 0 || event.value >= fg::guideAt(guide_).count) return;
      openChapter(event.value, 0);
      break;
    case ActionResume:
      openChapter(places_[guide_].chapter, places_[guide_].page);
      break;
    default:
      return;
  }
  requestUpdate();
}

void FmGuideActivity::buildHome(toybox::Screen& screen) {
  lab::chrome(screen, "FM-1 GUIAS");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int bottom = screen.device().height - kBottomReserve - 6;
  int y = toybox::kChromeHeight + 10;
  const int gap = fg::kGuides > 6 ? 4 : 6;
  const int tileH = (bottom - y - gap * (fg::kGuides - 1)) / fg::kGuides;
  // A tagline takes two lines. Under a name in the display cut that needs a
  // tile about 120 tall, which five guides had on an 800-row panel and six do
  // not (105). A shorter tile sets the name in the UI cut and moves the
  // version up beside it, so the second line of the tagline keeps clear of
  // both the version and the tile's edge. Seven guides leave 91: the name
  // moves up and the tagline sits straight under it, its second line's
  // descenders ending at 88.
  const bool roomy = tileH >= 120;
  const bool tight = tileH < 100;
  const toybox::CutMetrics& nameCut = roomy ? toybox::kDisplayCut : toybox::kUiCut;
  const fui::FontId nameFont = roomy ? toybox::kDisplayFont : toybox::kUiFont;
  const int top = roomy ? 14 : (tight ? 5 : 10);
  const int under = roomy ? 8 : (tight ? 0 : 3);
  for (int i = 0; i < fg::kGuides; ++i) {
    const fg::Guide& guide = fg::guideAt(i);
    const fui::Rect r = lab::rect(x, y, inner, tileH);
    lab::button(screen, r, "", ActionGuide, i);
    lab::label(screen, lab::rect(r.x + 16, r.y + top, inner - 32, nameCut.inkHeight), guide.name, nameFont, nameCut,
               fui::TextAlign::Left);
    const int versionY = roomy ? r.y + tileH - 12 - toybox::kTileCut.inkHeight
                               : r.y + top + nameCut.inkHeight - toybox::kTileCut.inkHeight;
    lab::caption(screen, r.x + 16, versionY, inner - 32, guide.version, fui::TextAlign::Right, fui::Color::DarkGray);
    flow(renderer, kBody, kBodyLine, r.x + 16, r.y + top + nameCut.inkHeight + under, inner - 32, guide.tagline, true);
    // A mark on the one being read.
    if (i == last_) renderer.fillRect(r.x + 4, r.y + top, 5, tileH - top * 2, true);
    y += tileH + gap;
  }
}

// A guide with parts opens on them: what to read first, the manual, the
// lab, the questions, the reference. Each row its name and one line on it.
void FmGuideActivity::buildParts(toybox::Screen& screen) {
  const fg::Guide& guide = fg::guideAt(guide_);
  lab::chrome(screen, guide.name);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int bottom = screen.device().height - kBottomReserve - 6;
  int y = toybox::kChromeHeight + 8;

  // Which version this is and when it was checked, before anything else.
  char text[96];
  std::snprintf(text, sizeof(text), "%s  \xC2\xB7  revisada %s", guide.version, guide.revised);
  renderer.drawText(kBody.font, x, y, text, true);
  y += kBodyLine + 6;

  const fg::Place& place = places_[guide_];
  const bool resume = place.chapter > 0 || place.page > 0;
  const int rows = guide.partCount + (resume ? 1 : 0);
  const int gap = 5;
  int rowH = (bottom - y - gap * (rows - 1)) / rows;
  if (rowH > 84) rowH = 84;

  if (resume) {
    const fui::Rect r = lab::rect(x, y, inner, rowH);
    lab::button(screen, r, "", ActionResume, 0, true);
    const int part = fg::partOf(guide, place.chapter);
    const int number = part >= 0 ? place.chapter - guide.parts[part].first + 1 : place.chapter + 1;
    std::snprintf(text, sizeof(text), "Seguir: %s, %d. %s", part >= 0 ? guide.parts[part].title : "", number % 100,
                  guide.chapters[place.chapter].title);
    // Bold, in white on the selected row; two lines at most.
    const int textH = flow(renderer, kBold, kBodyLine, r.x + 18, 0, inner - 36, text, false);
    flow(renderer, kBold, kBodyLine, r.x + 18, r.y + (rowH - textH) / 2, inner - 36, text, true, false);
    y += rowH + gap;
  }
  for (int i = 0; i < guide.partCount; ++i) {
    const fg::Part& part = guide.parts[i];
    const fui::Rect r = lab::rect(x, y, inner, rowH);
    lab::button(screen, r, "", ActionPart, i);
    const int top = r.y + (rowH - kBodyLine - 26) / 2;
    renderer.drawText(kBold.font, r.x + 16, top, part.title, true);
    renderer.drawText(kBody.font, r.x + 16, top + 26, part.blurb, true);
    arrowHead(renderer, r.x + inner - 14, r.y + rowH / 2, 7, true);
    y += rowH + gap;
  }
}

void FmGuideActivity::buildChapters(toybox::Screen& screen) {
  const fg::Guide& guide = fg::guideAt(guide_);
  const bool inPart = guide.parts && part_ >= 0 && part_ < guide.partCount;
  lab::chrome(screen, inPart ? guide.parts[part_].tab : guide.name);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int bottom = screen.device().height - kBottomReserve - 6;
  int y = toybox::kChromeHeight + 10;
  const int first = inPart ? guide.parts[part_].first : 0;
  const int count = inPart ? guide.parts[part_].count : guide.count;
  // The way back in to where the reader was; in a guide with parts it is on
  // the parts' screen instead.
  const fg::Place& place = places_[guide_];
  const bool resume = !guide.parts && (place.chapter > 0 || place.page > 0);
  const int rows = count + (resume ? 1 : 0);
  const int gap = 6;
  int rowH = (bottom - y - gap * (rows - 1)) / rows;
  if (rowH > 58) rowH = 58;

  if (resume) {
    const fui::Rect r = lab::rect(x, y, inner, rowH);
    lab::button(screen, r, "", ActionResume, 0, true);
    char text[48];
    std::snprintf(text, sizeof(text), "Seguir: cap. %d, p\xC3\xA1g. %d", (place.chapter + 1) % 100,
                  (place.page + 1) % 100);
    renderer.drawText(kBold.font, r.x + 18, r.y + rowH / 2 - kBold.inkHeight / 2 - (kBold.ascender - kBold.inkHeight),
                      text, false);
    y += rowH + gap;
  }
  char number[toybox::kIntChars + 1];
  for (int i = 0; i < count; ++i) {
    const fui::Rect r = lab::rect(x, y, inner, rowH);
    lab::button(screen, r, "", ActionChapter, first + i);
    disc(renderer, r.x + 30, r.y + rowH / 2, 15, true);
    std::snprintf(number, sizeof(number), "%d", (i + 1) % 100);
    centred(renderer, kTiny, r.x + 30, r.y + rowH / 2, number, false);
    renderer.drawText(kBold.font, r.x + 58, r.y + rowH / 2 - kBold.inkHeight / 2 - (kBold.ascender - kBold.inkHeight),
                      guide.chapters[first + i].title, true);
    y += rowH + gap;
  }
}

void FmGuideActivity::drawPage(toybox::Screen& screen) {
  const fg::Guide& guide = fg::guideAt(guide_);
  const fg::Chapter& chapter = guide.chapters[chapter_];
  if (!paged_) paginate();
  const bool inPart = part_ >= 0 && part_ < guide.partCount;
  lab::chrome(screen, inPart ? guide.parts[part_].tab : guide.name);
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;

  // Under the band: which chapter, and which page of it. Numbered within its
  // part, as the list shows it.
  char text[64];
  const int number = inPart ? chapter_ - guide.parts[part_].first + 1 : chapter_ + 1;
  std::snprintf(text, sizeof(text), "%d. %s", number % 100, chapter.title);
  const int subY = toybox::kChromeHeight + 8;
  renderer.drawText(kBold.font, x, subY, text, true);
  std::snprintf(text, sizeof(text), "%d / %d", (page_ + 1) % 100, pageCount_ % 100);
  const int w = renderer.getTextWidth(kTiny.font, text);
  renderer.drawText(kTiny.font, x + inner - w, subY + 8, text, true);
  renderer.fillRect(x, bodyTop() - 8, inner, 1, true);

  int y = bodyTop();
  linkCount_ = 0;
  for (int i = pageStart_[page_]; i < pageStart_[page_ + 1]; ++i) {
    const fg::Block& b = chapter.blocks[i];
    const int h = block(renderer, b, x, y, inner, true);
    if (b.kind == fg::Kind::Link && linkCount_ < kMaxLinks) {
      links_[linkCount_++] = LinkSpot{y, y + h - 10, static_cast<int>(b.mask)};
    }
    y += h;
  }

  // How far through the chapter this is.
  const int barY = bodyBottom() + 10;
  renderer.fillRectDither(x, barY, inner, 3, Color::LightGray);
  renderer.fillRect(x, barY, inner * (page_ + 1) / pageCount_, 3, true);
}

void FmGuideActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::Home:
      buildHome(screen);
      break;
    case View::Parts:
      buildParts(screen);
      break;
    case View::Chapters:
      buildChapters(screen);
      break;
    case View::Read:
      drawPage(screen);
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "FmGuide");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!cleanNext_ && ++fastPaints_ >= kCleanAfterPaints) cleanNext_ = true;
  if (cleanNext_) fastPaints_ = 0;
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}

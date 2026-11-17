#include "HandwritingActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;

namespace {

constexpr char kDir[] = "/handwriting";
constexpr char kIndexPath[] = "/handwriting/.index";
constexpr uint8_t kIndexVersion = 1;
constexpr char kInkMagic[4] = {'I', 'N', 'K', '1'};
constexpr int kMaxPages = 99;

constexpr int16_t kBarH = 56;
constexpr int16_t kToolH = 56;
constexpr int16_t kNibPanelH = 150;
constexpr int16_t kNibRowH = 96;
constexpr char kInkSpeedPath[] = "/.crosspoint/handwriting-ink.cfg";
constexpr int kInkSpeeds = 4;
// Frames of drive for each fast-ink level; level 0 is the panel's own waveform.
constexpr uint8_t kInkFrames[kInkSpeeds] = {0, 20, 10, 5};

// The controller's waveform table for fast ink, built for a number of frames.
// One phase of one group: a pixel going black is driven black for `frames`, one
// going white is driven white, and a pixel that does not change is left alone,
// so the paper around a stroke does not flash. The frame rate and the voltages
// are the ones this panel's grayscale table already uses.
unsigned char inkLut[110];
void buildInkLut(const uint8_t frames) {
  std::memset(inkLut, 0, sizeof(inkLut));
  inkLut[10] = 0x80;  // black to white: phase A at VSL
  inkLut[20] = 0x40;  // white to black: phase A at VSH1
  inkLut[50] = frames;
  for (int i = 100; i < 105; ++i) inkLut[i] = 0x22;
  inkLut[105] = 0x17;
  inkLut[106] = 0x41;
  inkLut[107] = 0xA8;
  inkLut[108] = 0x32;
  inkLut[109] = 0x30;
}
// Strokes that start this close to the left edge are the back swipe.
constexpr int kBackSwipeEdge = 24;
constexpr uint32_t kSaveAfterMs = 4000;
constexpr uint32_t kClearArmMs = 3000;
constexpr uint32_t kSavedShownMs = 2500;
constexpr int16_t kRuleEvery = 48;

// The notes grid: three across, three down, each a thumbnail of a page.
constexpr int kGridCols = 3;
constexpr int kGridRows = 3;
constexpr int kGridTiles = kGridCols * kGridRows;
constexpr int kThumbW = 136;
constexpr int kTileGap = 12;
constexpr int kTileCaption = 26;

constexpr ink::Tool kNibs[4] = {ink::Tool::Fine, ink::Tool::Pen, ink::Tool::Marker, ink::Tool::Pencil};

enum : fui::ActionId {
  ActionPrev = 1,
  ActionNext,
  ActionNib,
  ActionEraser,
  ActionUndo,
  ActionClear,
  ActionLines,
  ActionSave,
  ActionNotes,
  ActionPickNib,  // value: ink::Tool
  ActionOpen,     // value: page number
  ActionNew,
  ActionInkSpeed,
};

int16_t areaTop() { return kBarH; }
int16_t areaBottom(const int16_t height) { return static_cast<int16_t>(height - kToolH - 4); }

void pagePath(const int number, const char* ext, char* out, const size_t size) {
  std::snprintf(out, size, "%s/page-%02d.%s", kDir, number, ext);
}

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

// A chevron pointing left (dir -1) or right (+1), or a plus (dir 0).
void glyph(fui::DrawTarget& t, const int cx, const int cy, const int dir, const fui::Color colour) {
  const fui::Paint p = fui::Paint::solid(colour);
  if (dir == 0) {
    t.fill(rect(cx - 8, cy - 1, 17, 3), p);
    t.fill(rect(cx - 1, cy - 8, 3, 17), p);
    return;
  }
  for (int i = 0; i <= 8; ++i) {
    const int x = cx + dir * (4 - i);
    t.fill(rect(x - 1, cy - i, 3, 1), p);
    t.fill(rect(x - 1, cy + i, 3, 1), p);
  }
}

// A bar button on the black band: outlined in white, its glyph drawn on top.
void bandButton(toybox::Screen& screen, const fui::Rect& r, const fui::ActionId action, const bool enabled) {
  fui::ButtonProps b;
  b.label = "";
  b.action = action;
  b.enabled = enabled;
  b.styles = toybox::bandOutlineStyles();
  b.radius = toybox::kPillRadius / 2;
  screen.button(b, r);
}

// The sample stroke a nib is shown by.
void nibSample(fui::DrawTarget& t, const ink::Tool tool, const int x, const int cy, const int w, const bool onBlack) {
  const int h = ink::widthOf(tool);
  const fui::Color ink = onBlack ? fui::Color::White : fui::Color::Black;
  const fui::Paint paint = tool == ink::Tool::Pencil
                               ? fui::Paint::dither(onBlack ? fui::Color::LightGray : fui::Color::DarkGray)
                               : fui::Paint::solid(ink);
  t.fill(rect(x, cy - h / 2, w, h), paint, static_cast<uint8_t>(h >= 6 ? h / 2 : 0));
}

}  // namespace

std::unique_ptr<Activity> HandwritingActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<HandwritingActivity>(renderer, mappedInput);
}

void HandwritingActivity::repaint() {
  painting = true;
  requestUpdate();
}

void HandwritingActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadInkSpeed();
  inkWaveformOn = false;
  page = makeUniqueNoThrow<ink::Page>();
  const int16_t width = static_cast<int16_t>(renderer.getScreenWidth());
  const int16_t height = static_cast<int16_t>(areaBottom(static_cast<int16_t>(renderer.getScreenHeight())) - areaTop());
  if (page) {
    page->width = width;
    page->height = height;
    raster = makeUniqueNoThrow<uint8_t[]>(static_cast<size_t>((width + 7) / 8) * static_cast<size_t>(height));
    const int thumbH = kThumbW * height / width;
    thumb = makeUniqueNoThrow<uint8_t[]>(static_cast<size_t>((kThumbW + 7) / 8) * static_cast<size_t>(thumbH));
  }
  if (!page || !raster || !thumb) {
    LOG_ERR("INK", "OOM: page buffers");
  } else {
    Storage.ensureDirectoryExists(kDir);
    loadIndex();
    loadPage(current);
  }
  view = View::Page;
  repaint();
}

void HandwritingActivity::onExit() {
  endInkWaveform();
  if (dirty && view == View::Page) savePage();
  saveIndex();
  thumb.reset();
  raster.reset();
  page.reset();
  Activity::onExit();
}

void HandwritingActivity::goToPage(const int number) {
  if (dirty && view == View::Page) savePage();
  current = number;
  if (current > pageCount) pageCount = current;
  loadPage(current);
  saveIndex();
  clearArmedMs = 0;
  nibOpen = false;
  view = View::Page;
  repaint();
}

void HandwritingActivity::openNotes() {
  if (dirty) savePage();
  saveIndex();
  nibOpen = false;
  // The grid opens on the screenful that holds the page you were on.
  notesFirst = ((current - 1) / kGridTiles) * kGridTiles + 1;
  view = View::Notes;
  repaint();
}

void HandwritingActivity::loop() {
  const uint32_t now = static_cast<uint32_t>(millis());
  // The back swipe is any left-to-right swipe that starts in the left quarter
  // of the panel, which is also how half of all handwriting starts. So a
  // swipe that drew a stroke is a stroke; Back is a swipe from the very edge
  // (where no stroke starts) or a long way from any ink.
  if (!drawing && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (painting) return;
    if (view == View::Notes) {
      goToPage(current);
      return;
    }
    if (nibOpen) {
      nibOpen = false;
      repaint();
      return;
    }
    if (dirty) savePage();
    saveIndex();
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (!page || !raster || !thumb) return;

  if (view == View::Notes) {
    int tapX = 0;
    int tapY = 0;
    // The grid's own painting reads pages into the page buffer; nothing may
    // load another one under it.
    if (painting || !mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady) return;
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
    routeNotes(interactions.route(input));
    return;
  }

  const int16_t top = areaTop();
  const int16_t bottom = areaBottom(static_cast<int16_t>(renderer.getScreenHeight()));
  int x = 0;
  int y = 0;
  if (!drawing && mappedInput.wasScreenTouchPressed(x, y) && x >= kBackSwipeEdge && y >= top && y < bottom) {
    if (nibOpen) {
      // A touch on the page puts the nib panel away; one on the panel is a
      // tap for the router below.
      if (y < bottom - kNibPanelH) {
        nibOpen = false;
        repaint();
      }
      return;
    }
    if (page->begin(tool, static_cast<int16_t>(x), static_cast<int16_t>(y - top))) {
      drawing = true;
      full = false;
      clearArmedMs = 0;
      savedShownMs = 0;
    } else {
      full = true;
      repaint();
    }
    return;
  }
  if (drawing) {
    if (mappedInput.isScreenTouchHeld(x, y)) {
      const int cy = y < top ? top : (y >= bottom ? bottom - 1 : y);
      if (!page->extend(static_cast<int16_t>(x), static_cast<int16_t>(cy - top))) full = true;
      // No timer: the panel is the clock. New ink is shown the moment the
      // last repaint has come back.
      if (!painting && page->count() != paintedCount) repaint();
      return;
    }
    // The finger is up: the stroke is done.
    drawing = false;
    dirty = true;
    lastStrokeEndMs = now;
    repaint();
    return;
  }

  if (!painting && (saveWanted || (dirty && now - lastStrokeEndMs >= kSaveAfterMs))) {
    const bool asked = saveWanted;
    saveWanted = false;
    savePage();
    if (asked) {
      savedShownMs = now;
      repaint();
    }
  }
  if (savedShownMs != 0 && now - savedShownMs >= kSavedShownMs && !painting) {
    savedShownMs = 0;
    repaint();
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  routePage(interactions.route(input), now);
}

void HandwritingActivity::routePage(const fui::ActionEvent& event, const uint32_t now) {
  switch (event.action) {
    case ActionPrev:
      if (current > 1) goToPage(current - 1);
      return;
    case ActionNext:
      // Past the last page, a fresh one -- but never a second blank one.
      if (current < pageCount) {
        goToPage(current + 1);
      } else if (!page->empty() && pageCount < kMaxPages) {
        goToPage(current + 1);
      }
      return;
    case ActionNotes:
      if (!painting) openNotes();
      return;
    case ActionNib:
      // From the eraser, back to the nib in hand; on the nib, its choices.
      if (tool == ink::Tool::Eraser) {
        tool = nib;
      } else {
        nibOpen = !nibOpen;
      }
      break;
    case ActionInkSpeed:
      inkSpeed = (inkSpeed + 1) % kInkSpeeds;
      saveInkSpeed();
      break;
    case ActionPickNib:
      if (event.value >= 0 && event.value < ink::kToolCount && ink::isNib(static_cast<ink::Tool>(event.value))) {
        nib = static_cast<ink::Tool>(event.value);
        tool = nib;
        saveIndex();
      }
      nibOpen = false;
      break;
    case ActionEraser:
      tool = ink::Tool::Eraser;
      nibOpen = false;
      break;
    case ActionUndo:
      if (!page->undo()) return;
      rastered = -1;
      dirty = true;
      lastStrokeEndMs = now;
      break;
    case ActionClear:
      if (page->empty()) return;
      if (clearArmedMs != 0 && now - clearArmedMs < kClearArmMs) {
        page->clear();
        rastered = -1;
        clearArmedMs = 0;
        dirty = true;
        lastStrokeEndMs = now;
      } else {
        clearArmedMs = now;
      }
      break;
    case ActionLines:
      ruled = !ruled;
      saveIndex();
      break;
    case ActionSave:
      saveWanted = true;
      return;
    default:
      return;
  }
  repaint();
}

void HandwritingActivity::routeNotes(const fui::ActionEvent& event) {
  switch (event.action) {
    case ActionOpen:
      if (event.value >= 1 && event.value <= pageCount) goToPage(event.value);
      return;
    case ActionNew: {
      // A new page after the last one, unless the last one is still blank.
      const bool lastHasInk = readPage(pageCount) && !page->empty();
      goToPage(lastHasInk && pageCount < kMaxPages ? pageCount + 1 : pageCount);
      return;
    }
    case ActionPrev:
      if (notesFirst > 1) {
        notesFirst -= kGridTiles;
        repaint();
      }
      return;
    case ActionNext:
      if (notesFirst + kGridTiles <= pageCount + 1) {
        notesFirst += kGridTiles;
        repaint();
      }
      return;
    case ActionNotes:
      goToPage(current);
      return;
    default:
      return;
  }
}

// New ink of the stroke in progress, set straight into the framebuffer that
// already shows the page, and only its bounding box sent to the panel. The
// eraser takes ink away, which only a full repaint can show.
bool HandwritingActivity::renderStroke() {
  if (!drawing || !pageOnPanel || view != View::Page || nibOpen || full || tool == ink::Tool::Eraser) return false;
  if (!page || !raster || rastered < 0) return false;
  const int count = page->count();
  if (rastered > count) return false;
  if (rastered == count) return true;
  const ink::Point* p = page->points();
  int x0 = page->width;
  int y0 = page->height;
  int x1 = -1;
  int y1 = -1;
  for (int i = rastered > 0 ? rastered - 1 : 0; i < count; ++i) {
    if (p[i].x == ink::kStrokeMark) continue;
    if (p[i].x < x0) x0 = p[i].x;
    if (p[i].x > x1) x1 = p[i].x;
    if (p[i].y < y0) y0 = p[i].y;
    if (p[i].y > y1) y1 = p[i].y;
  }
  const int rowBytes = (page->width + 7) / 8;
  ink::rasteriseMore(*page, raster.get(), rowBytes, rastered, count);
  rastered = count;
  paintedCount = count;
  if (x1 < 0) return true;
  const int reach = ink::widthOf(tool) + 2;
  x0 = x0 - reach < 0 ? 0 : x0 - reach;
  y0 = y0 - reach < 0 ? 0 : y0 - reach;
  x1 = x1 + reach >= page->width ? page->width - 1 : x1 + reach;
  y1 = y1 + reach >= page->height ? page->height - 1 : y1 + reach;
  const int top = areaTop();
  for (int y = y0; y <= y1; ++y) {
    const uint8_t* row = raster.get() + y * rowBytes;
    for (int x = x0; x <= x1; ++x) {
      if (row[x / 8] & (0x80 >> (x % 8))) renderer.drawPixel(x, top + y, true);
    }
  }
  if (inkSpeed > 0 && !inkWaveformOn) {
    buildInkLut(kInkFrames[inkSpeed]);
#if !defined(SIMULATOR)
    display.setInkWaveform(inkLut);
#endif
    inkWaveformOn = true;
  }
  const uint32_t began = static_cast<uint32_t>(millis());
  renderer.displayRegion(x0, top + y0, x1 - x0 + 1, y1 - y0 + 1);
  lastInkMs = static_cast<uint32_t>(millis()) - began;
  return true;
}

void HandwritingActivity::render(RenderLock&&) {
  if (renderStroke()) {
    painting = false;
    return;
  }
  // Anything but new ink is drawn with the panel's own waveform.
  endInkWaveform();
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen screen(frame);
  toybox::absoluteChrome(screen);

  if (view == View::Notes && page && raster && thumb) {
    renderNotes(screen);
  } else {
    renderPage(screen);
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Handwriting");
  renderer.displayBuffer();
  pageOnPanel = view == View::Page && page && raster && !nibOpen;
  painting = false;
}

void HandwritingActivity::renderPage(toybox::Screen& screen) {
  auto& t = screen.target();
  const fui::DeviceContext device = screen.device();
  const int16_t w = device.width;
  const int16_t top = areaTop();
  const int16_t bottom = areaBottom(device.height);
  const int bandH = kBarH - 4;

  // The bar: every note, where you are, and page turning.
  t.fill(rect(0, 0, w, bandH), fui::Paint::solid(fui::Color::Black));
  fui::ButtonProps all;
  all.label = "NOTES";
  all.action = ActionNotes;
  all.styles = toybox::bandOutlineStyles();
  all.text = toybox::buttonText(screen.theme());
  all.text.color = fui::Color::White;
  all.radius = toybox::kPillRadius / 2;
  screen.button(all, rect(6, 6, 84, bandH - 12));
  char where[48];
  if (!page || !raster) {
    std::snprintf(where, sizeof(where), "NO MEMORY");
  } else if (full) {
    std::snprintf(where, sizeof(where), "PAGE IS FULL");
  } else if (savedShownMs != 0) {
    std::snprintf(where, sizeof(where), "SAVED");
  } else {
    std::snprintf(where, sizeof(where), "PAGE %d OF %d", current, pageCount);
  }
  fui::TextStyle title;
  title.font = toybox::kUiFont;
  title.color = fui::Color::White;
  title.align = fui::TextAlign::Center;
  t.text(toybox::inkCentred(rect(96, 0, w - 96 - 120, bandH), toybox::kUiCut), where, title);
  const fui::Rect prevR = rect(w - 114, 6, 52, bandH - 12);
  const fui::Rect nextR = rect(w - 58, 6, 52, bandH - 12);
  bandButton(screen, prevR, ActionPrev, current > 1);
  bandButton(screen, nextR, ActionNext, true);
  glyph(t, prevR.x + prevR.width / 2, prevR.y + prevR.height / 2, -1,
        current > 1 ? fui::Color::White : fui::Color::DarkGray);
  glyph(t, nextR.x + nextR.width / 2, nextR.y + nextR.height / 2, current < pageCount ? 1 : 0, fui::Color::White);

  // The ruling, under the ink and never part of it.
  if (ruled) {
    for (int16_t y = static_cast<int16_t>(top + kRuleEvery); y < bottom; y = static_cast<int16_t>(y + kRuleEvery)) {
      t.fill(rect(12, y, w - 24, 2), fui::Paint::dither(fui::Color::LightGray));
    }
  }

  // The ink. Only what was added since the last repaint is drawn into the
  // page's bitmap, so a page full of writing costs no more per stroke than an
  // empty one; then the bitmap is set pixel by pixel, skipping blank bytes.
  if (page && raster) {
    const int rowBytes = (page->width + 7) / 8;
    const int count = page->count();
    if (rastered < 0 || rastered > count) {
      std::memset(raster.get(), 0, static_cast<size_t>(rowBytes) * static_cast<size_t>(page->height));
      rastered = 0;
    }
    ink::rasteriseMore(*page, raster.get(), rowBytes, rastered, count);
    rastered = count;
    paintedCount = count;
    for (int y = 0; y < page->height; ++y) {
      const uint8_t* row = raster.get() + y * rowBytes;
      for (int b = 0; b < rowBytes; ++b) {
        if (!row[b]) continue;
        for (int bit = 0; bit < 8; ++bit)
          if (row[b] & (0x80 >> bit)) renderer.drawPixel(b * 8 + bit, top + y, true);
      }
    }
  }

  // The nib choices, over the foot of the page while they are open.
  if (nibOpen) {
    const int py = bottom - kNibPanelH;
    t.fill(rect(0, py, w, kNibPanelH), fui::Paint::solid(fui::Color::White));
    t.fill(rect(0, py, w, 2), fui::Paint::solid(fui::Color::Black));
    const int gap = 8;
    const int cw = (w - 8 - gap * 3) / 4;
    for (int i = 0; i < 4; ++i) {
      const bool on = kNibs[i] == nib;
      const fui::Rect r = rect(4 + i * (cw + gap), py + 10, cw, kNibRowH - 20);
      fui::ButtonProps b;
      b.label = "";
      b.action = ActionPickNib;
      b.value = static_cast<int16_t>(kNibs[i]);
      b.styles = toybox::rowStyles();
      b.state = on ? fui::StateSelected : fui::StateNormal;
      b.radius = toybox::kPillRadius / 2;
      screen.button(b, r);
      nibSample(t, kNibs[i], r.x + 18, r.y + 26, r.width - 36, on);
      fui::TextStyle s;
      s.font = toybox::kTileFont;
      s.align = fui::TextAlign::Center;
      s.color = on ? fui::Color::White : fui::Color::Black;
      t.text(toybox::inkCentred(rect(r.x, r.y + 44, r.width, 24), toybox::kTileCut), ink::toolName(kNibs[i]), s);
    }
  }

  // Under the nibs: how new ink is put on the panel, and how long the last
  // stroke refresh took, which is the number to tune it by.
  if (nibOpen) {
    char label[toybox::kIntChars * 2 + 40];
    if (inkSpeed == 0) {
      std::snprintf(label, sizeof(label), "INK: NORMAL  (LAST %u MS)", static_cast<unsigned>(lastInkMs % 100000u));
    } else {
      std::snprintf(label, sizeof(label), "INK: FAST %d, %d FRAMES  (LAST %u MS)", inkSpeed, kInkFrames[inkSpeed],
                    static_cast<unsigned>(lastInkMs % 100000u));
    }
    fui::ButtonProps b;
    b.label = label;
    b.action = ActionInkSpeed;
    b.styles = toybox::rowStyles();
    b.text = toybox::buttonText(screen.theme());
    b.state = inkSpeed > 0 ? fui::StateSelected : fui::StateNormal;
    b.radius = toybox::kPillRadius / 2;
    screen.button(b, rect(4, bottom - kNibPanelH + kNibRowH - 4, w - 8, kNibPanelH - kNibRowH - 6));
  }

  // The tools.
  t.fill(rect(0, bottom + 1, w, 2), fui::Paint::solid(fui::Color::Black));
  const int toolY = bottom + 7;
  const int gap = 5;
  constexpr int kTools = 6;
  const int bw = (w - 8 - gap * (kTools - 1)) / kTools;
  const bool armed = clearArmedMs != 0;
  const bool erasing = tool == ink::Tool::Eraser;
  struct ToolButton {
    const char* label;
    fui::ActionId action;
    bool on;
  };
  const ToolButton tools[kTools] = {
      {ink::toolName(nib), ActionNib, !erasing},       {"ERASE", ActionEraser, erasing}, {"UNDO", ActionUndo, false},
      {armed ? "SURE?" : "CLEAR", ActionClear, armed}, {"LINES", ActionLines, ruled},    {"SAVE", ActionSave, false},
  };
  fui::TextStyle small;
  small.font = toybox::kTileFont;
  small.align = fui::TextAlign::Center;
  for (int i = 0; i < kTools; ++i) {
    // Six across is too narrow for the button face, so each button is drawn
    // bare and labelled in the small cut on top.
    const fui::Rect box = rect(4 + i * (bw + gap), toolY, bw, kToolH - 12);
    fui::ButtonProps b;
    b.label = "";
    b.action = tools[i].action;
    b.styles = toybox::rowStyles();
    b.state = tools[i].on ? fui::StateSelected : fui::StateNormal;
    b.radius = toybox::kPillRadius / 2;
    screen.button(b, box);
    small.color = tools[i].on ? fui::Color::White : fui::Color::Black;
    if (i == 0) {
      // The nib button carries its own stroke, so the thickness in hand shows
      // without opening anything.
      nibSample(t, nib, box.x + 14, box.y + 12, box.width - 28, !erasing);
      t.text(toybox::inkCentred(rect(box.x, box.y + 18, box.width, box.height - 18), toybox::kTileCut), tools[i].label,
             small);
    } else {
      t.text(toybox::inkCentred(box, toybox::kTileCut), tools[i].label, small);
    }
  }
}

void HandwritingActivity::renderNotes(toybox::Screen& screen) {
  auto& t = screen.target();
  const fui::DeviceContext device = screen.device();
  const int16_t w = device.width;
  const int bandH = kBarH - 4;

  t.fill(rect(0, 0, w, bandH), fui::Paint::solid(fui::Color::Black));
  char where[toybox::kIntChars + 16];
  std::snprintf(where, sizeof(where), "%d %s", pageCount, pageCount == 1 ? "NOTE" : "NOTES");
  fui::TextStyle title;
  title.font = toybox::kUiFont;
  title.color = fui::Color::White;
  title.align = fui::TextAlign::Left;
  t.text(toybox::inkCentred(rect(16, 0, w - 150, bandH), toybox::kUiCut), where, title);
  const bool more = notesFirst + kGridTiles <= pageCount + 1;
  if (notesFirst > 1 || more) {
    const fui::Rect prevR = rect(w - 114, 6, 52, bandH - 12);
    const fui::Rect nextR = rect(w - 58, 6, 52, bandH - 12);
    bandButton(screen, prevR, ActionPrev, notesFirst > 1);
    bandButton(screen, nextR, ActionNext, more);
    glyph(t, prevR.x + prevR.width / 2, prevR.y + prevR.height / 2, -1,
          notesFirst > 1 ? fui::Color::White : fui::Color::DarkGray);
    glyph(t, nextR.x + nextR.width / 2, nextR.y + nextR.height / 2, 1, more ? fui::Color::White : fui::Color::DarkGray);
  }

  const int rowBytes = (page->width + 7) / 8;
  const int thumbH = kThumbW * page->height / page->width;
  const int thumbRow = (kThumbW + 7) / 8;
  const int tileW = kThumbW + 4;
  const int tileH = thumbH + 4;
  const int left = (w - (kGridCols * tileW + (kGridCols - 1) * kTileGap)) / 2;
  const int top = kBarH + 10;
  fui::TextStyle cap;
  cap.font = toybox::kTileFont;
  cap.align = fui::TextAlign::Center;
  for (int i = 0; i < kGridTiles; ++i) {
    const int number = notesFirst + i;
    if (number > pageCount + 1 || number > kMaxPages) break;
    const int x = left + (i % kGridCols) * (tileW + kTileGap);
    const int y = top + (i / kGridCols) * (tileH + kTileCaption + 6);
    const fui::Rect r = rect(x, y, tileW, tileH);
    const bool isNew = number == pageCount + 1;
    fui::ButtonProps b;
    b.label = "";
    b.action = isNew ? ActionNew : ActionOpen;
    b.value = static_cast<int16_t>(number);
    b.styles = toybox::rowStyles();
    screen.button(b, r);
    if (isNew) {
      glyph(t, x + tileW / 2, y + tileH / 2, 0, fui::Color::Black);
      t.text(toybox::inkCentred(rect(x, y + tileH + 4, tileW, kTileCaption - 6), toybox::kTileCut), "NEW", cap);
      break;
    }
    // The page as it stands on the card, shrunk.
    if (readPage(number)) {
      ink::rasterise(*page, raster.get(), rowBytes);
      ink::thumbnail(raster.get(), rowBytes, page->width, page->height, thumb.get(), kThumbW, thumbH);
      for (int ty = 0; ty < thumbH; ++ty) {
        const uint8_t* row = thumb.get() + ty * thumbRow;
        for (int bx = 0; bx < thumbRow; ++bx) {
          if (!row[bx]) continue;
          for (int bit = 0; bit < 8; ++bit)
            if (row[bx] & (0x80 >> bit)) renderer.drawPixel(x + 2 + bx * 8 + bit, y + 2 + ty, true);
        }
      }
    }
    if (number == current) t.stroke(r, fui::Paint::solid(fui::Color::Black), 4);
    char label[toybox::kIntChars + 8];
    std::snprintf(label, sizeof(label), "PAGE %d", number);
    t.text(toybox::inkCentred(rect(x, y + tileH + 4, tileW, kTileCaption - 6), toybox::kTileCut), label, cap);
  }
  // The page buffer now holds the last thumbnail's page, not the open one.
  rastered = -1;
}

void HandwritingActivity::loadIndex() {
  current = 1;
  pageCount = 1;
  uint8_t data[5] = {};
  HalFile file;
  if (!Storage.exists(kIndexPath) || !Storage.openFileForRead("INK", kIndexPath, file)) return;
  const int got = file.read(data, sizeof(data));
  if (got < 4 || data[0] != kIndexVersion) return;
  pageCount = data[1] >= 1 && data[1] <= kMaxPages ? data[1] : 1;
  current = data[2] >= 1 && data[2] <= pageCount ? data[2] : 1;
  ruled = data[3] != 0;
  // The nib was added to the file later; an index without it means the pen.
  if (got >= 5 && data[4] < ink::kToolCount && ink::isNib(static_cast<ink::Tool>(data[4]))) {
    nib = static_cast<ink::Tool>(data[4]);
    tool = nib;
  }
}

void HandwritingActivity::loadInkSpeed() {
  inkSpeed = 0;
  if (!Storage.exists(kInkSpeedPath)) return;
  HalFile file;
  uint8_t value = 0;
  if (Storage.openFileForRead("INK", kInkSpeedPath, file) && file.read(&value, 1) == 1 && value < kInkSpeeds) {
    inkSpeed = value;
  }
}

void HandwritingActivity::saveInkSpeed() {
  HalFile file;
  if (!Storage.openFileForWrite("INK", kInkSpeedPath, file)) return;
  const uint8_t value = static_cast<uint8_t>(inkSpeed);
  file.write(&value, 1);
  file.flush();
}

// Hands the panel back to its own waveform. Only a flag in the driver: the
// next ordinary refresh reloads the factory table by itself.
void HandwritingActivity::endInkWaveform() {
  if (!inkWaveformOn) return;
#if !defined(SIMULATOR)
  display.setInkWaveform(nullptr);
#endif
  inkWaveformOn = false;
}

void HandwritingActivity::saveIndex() {
  HalFile file;
  if (!Storage.openFileForWrite("INK", kIndexPath, file)) return;
  const uint8_t data[5] = {kIndexVersion, static_cast<uint8_t>(pageCount), static_cast<uint8_t>(current),
                           static_cast<uint8_t>(ruled ? 1 : 0), static_cast<uint8_t>(nib)};
  file.write(data, sizeof(data));
  file.flush();
}

bool HandwritingActivity::readPage(const int number) {
  page->clear();
  char path[40];
  pagePath(number, "ink", path, sizeof(path));
  HalFile file;
  if (!Storage.exists(path) || !Storage.openFileForRead("INK", path, file)) return false;
  char magic[4] = {};
  uint32_t count = 0;
  if (file.read(reinterpret_cast<uint8_t*>(magic), 4) != 4 || std::memcmp(magic, kInkMagic, 4) != 0) return false;
  if (file.read(reinterpret_cast<uint8_t*>(&count), 4) != 4 || count > static_cast<uint32_t>(ink::kMaxPoints)) {
    return false;
  }
  // Read straight into a scratch copy of the right size, then hand it over.
  auto points = makeUniqueNoThrow<ink::Point[]>(count == 0 ? 1 : count);
  if (!points) {
    LOG_ERR("INK", "OOM: %u points", static_cast<unsigned>(count));
    return false;
  }
  const size_t bytes = sizeof(ink::Point) * count;
  if (file.read(reinterpret_cast<uint8_t*>(points.get()), bytes) != static_cast<int>(bytes)) return false;
  return page->assign(points.get(), static_cast<int>(count));
}

void HandwritingActivity::loadPage(const int number) {
  dirty = false;
  full = false;
  rastered = -1;
  paintedCount = 0;
  readPage(number);
}

void HandwritingActivity::savePage() {
  dirty = false;
  if (!page || !raster) return;
  char path[40];
  pagePath(current, "ink", path, sizeof(path));
  {
    HalFile file;
    if (!Storage.openFileForWrite("INK", path, file)) return;
    const uint32_t count = static_cast<uint32_t>(page->count());
    file.write(reinterpret_cast<const uint8_t*>(kInkMagic), 4);
    file.write(reinterpret_cast<const uint8_t*>(&count), 4);
    file.write(reinterpret_cast<const uint8_t*>(page->points()), sizeof(ink::Point) * count);
    file.flush();
  }
  // The picture, for a computer or the wallpapers folder.
  pagePath(current, "bmp", path, sizeof(path));
  HalFile bmp;
  if (!Storage.openFileForWrite("INK", path, bmp)) return;
  const int width = page->width;
  const int height = page->height;
  const int rowBytes = (width + 7) / 8;
  ink::rasterise(*page, raster.get(), rowBytes);
  rastered = page->count();
  uint8_t header[ink::kBmpHeaderSize];
  ink::bmpHeader(width, height, header);
  bmp.write(header, sizeof(header));
  const int bmpBytes = ink::bmpRowBytes(width);
  auto row = makeUniqueNoThrow<uint8_t[]>(static_cast<size_t>(bmpBytes));
  if (!row) return;
  for (int y = height - 1; y >= 0; --y) {
    ink::bmpRow(raster.get(), rowBytes, width, y, row.get());
    bmp.write(row.get(), static_cast<size_t>(bmpBytes));
  }
  bmp.flush();
}

#include "HandwritingActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
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
// Strokes that start this close to the left edge are the back swipe.
constexpr int kBackSwipeEdge = 24;
constexpr uint32_t kPaintEveryMs = 250;
constexpr uint32_t kSaveAfterMs = 4000;
constexpr uint32_t kClearArmMs = 3000;
constexpr int16_t kRuleEvery = 48;

enum : fui::ActionId {
  ActionPrev = 1,
  ActionNext = 2,
  ActionPen = 3,
  ActionEraser = 4,
  ActionUndo = 5,
  ActionClear = 6,
  ActionLines = 7,
};

int16_t areaTop() { return kBarH; }
int16_t areaBottom(const int16_t height) { return static_cast<int16_t>(height - kToolH - 4); }

void pagePath(const int number, const char* ext, char* out, const size_t size) {
  std::snprintf(out, size, "%s/page-%02d.%s", kDir, number, ext);
}

}  // namespace

std::unique_ptr<Activity> HandwritingActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<HandwritingActivity>(renderer, mappedInput);
}

void HandwritingActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  page = makeUniqueNoThrow<ink::Page>();
  const int16_t width = static_cast<int16_t>(renderer.getScreenWidth());
  const int16_t height = static_cast<int16_t>(areaBottom(static_cast<int16_t>(renderer.getScreenHeight())) - areaTop());
  if (page) {
    page->width = width;
    page->height = height;
    raster = makeUniqueNoThrow<uint8_t[]>(static_cast<size_t>((width + 7) / 8) * static_cast<size_t>(height));
  }
  if (!page || !raster) {
    LOG_ERR("INK", "OOM: page buffers");
  } else {
    Storage.ensureDirectoryExists(kDir);
    loadIndex();
    loadPage(current);
  }
  requestUpdate();
}

void HandwritingActivity::onExit() {
  if (dirty) savePage();
  saveIndex();
  raster.reset();
  page.reset();
  Activity::onExit();
}

void HandwritingActivity::goToPage(const int number) {
  if (dirty) savePage();
  current = number;
  if (current > pageCount) pageCount = current;
  loadPage(current);
  saveIndex();
  clearArmedMs = 0;
  requestUpdate();
}

void HandwritingActivity::loop() {
  const uint32_t now = static_cast<uint32_t>(millis());
  // The back swipe is any left-to-right swipe that starts in the left quarter
  // of the panel, which is also how half of all handwriting starts. So a
  // swipe that drew a stroke is a stroke; Back is a swipe from the very edge
  // (where no stroke starts) or a long way from any ink.
  if (!drawing && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (dirty) savePage();
    saveIndex();
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (!page || !raster) return;

  const int16_t top = areaTop();
  const int16_t bottom = areaBottom(static_cast<int16_t>(renderer.getScreenHeight()));
  int x = 0;
  int y = 0;
  if (!drawing && mappedInput.wasScreenTouchPressed(x, y) && x >= kBackSwipeEdge && y >= top && y < bottom) {
    if (page->begin(tool, static_cast<int16_t>(x), static_cast<int16_t>(y - top))) {
      drawing = true;
      full = false;
      clearArmedMs = 0;
      lastPaintMs = now;
    } else {
      full = true;
      requestUpdate();
    }
    return;
  }
  if (drawing) {
    if (mappedInput.isScreenTouchHeld(x, y)) {
      const int cy = y < top ? top : (y >= bottom ? bottom - 1 : y);
      if (!page->extend(static_cast<int16_t>(x), static_cast<int16_t>(cy - top))) full = true;
      if (now - lastPaintMs >= kPaintEveryMs) {
        lastPaintMs = now;
        requestUpdate();
      }
      return;
    }
    // The finger is up: the stroke is done.
    drawing = false;
    dirty = true;
    lastStrokeEndMs = now;
    requestUpdate();
    return;
  }

  if (dirty && now - lastStrokeEndMs >= kSaveAfterMs) savePage();

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;
  const fui::ActionEvent event = interactions.route(input);
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
    case ActionPen:
    case ActionEraser:
      tool = event.action == ActionPen ? ink::Tool::Pen : ink::Tool::Eraser;
      requestUpdate();
      return;
    case ActionUndo:
      if (page->undo()) {
        dirty = true;
        lastStrokeEndMs = now;
        requestUpdate();
      }
      return;
    case ActionClear:
      if (page->empty()) return;
      if (clearArmedMs != 0 && now - clearArmedMs < kClearArmMs) {
        page->clear();
        clearArmedMs = 0;
        dirty = true;
        lastStrokeEndMs = now;
      } else {
        clearArmedMs = now;
      }
      requestUpdate();
      return;
    case ActionLines:
      ruled = !ruled;
      saveIndex();
      requestUpdate();
      return;
    default:
      return;
  }
}

void HandwritingActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen screen(frame);
  toybox::absoluteChrome(screen);
  auto& t = screen.target();
  const int16_t w = device.width;
  const int16_t top = areaTop();
  const int16_t bottom = areaBottom(device.height);

  // The bar: page turning and where you are.
  t.fill(fui::makeRect(0, 0, w, static_cast<int16_t>(kBarH - 4)), fui::Paint::solid(fui::Color::Black));
  char where[48];
  if (!page || !raster) {
    std::snprintf(where, sizeof(where), "NOT ENOUGH MEMORY");
  } else if (full) {
    std::snprintf(where, sizeof(where), "PAGE IS FULL");
  } else {
    std::snprintf(where, sizeof(where), "PAGE %d OF %d", current, pageCount);
  }
  fui::TextStyle title;
  title.font = toybox::kUiFont;
  title.color = fui::Color::White;
  title.align = fui::TextAlign::Center;
  t.text(toybox::inkCentred(fui::makeRect(80, 0, static_cast<int16_t>(w - 160), static_cast<int16_t>(kBarH - 4)),
                            toybox::kUiCut),
         where, title);
  fui::ButtonProps prev;
  prev.label = "<";
  prev.action = ActionPrev;
  prev.enabled = current > 1;
  screen.button(prev, fui::makeRect(4, 4, 70, static_cast<int16_t>(kBarH - 12)));
  fui::ButtonProps next;
  next.label = current < pageCount ? ">" : "+";
  next.action = ActionNext;
  screen.button(next, fui::makeRect(static_cast<int16_t>(w - 74), 4, 70, static_cast<int16_t>(kBarH - 12)));

  // The ruling, under the ink and never part of it.
  if (ruled) {
    for (int16_t y = static_cast<int16_t>(top + kRuleEvery); y < bottom; y = static_cast<int16_t>(y + kRuleEvery)) {
      t.fill(fui::makeRect(12, y, static_cast<int16_t>(w - 24), 2), fui::Paint::dither(fui::Color::LightGray));
    }
  }

  // The ink, rasterised from the strokes so the eraser rubs out exactly what
  // was under it, then set pixel by pixel. Most of a page is blank, so whole
  // empty bytes are skipped.
  if (page && raster) {
    const int rowBytes = (page->width + 7) / 8;
    ink::rasterise(*page, raster.get(), rowBytes);
    for (int y = 0; y < page->height; ++y) {
      const uint8_t* row = raster.get() + y * rowBytes;
      for (int b = 0; b < rowBytes; ++b) {
        if (!row[b]) continue;
        for (int bit = 0; bit < 8; ++bit)
          if (row[b] & (0x80 >> bit)) renderer.drawPixel(b * 8 + bit, top + y, true);
      }
    }
  }

  // The tools.
  t.fill(fui::makeRect(0, static_cast<int16_t>(bottom + 1), w, 2), fui::Paint::solid(fui::Color::Black));
  const int16_t toolY = static_cast<int16_t>(bottom + 6);
  const int16_t gap = 6;
  const int16_t bw = static_cast<int16_t>((w - 8 - gap * 4) / 5);
  const bool armed = clearArmedMs != 0;
  struct Tool {
    const char* label;
    fui::ActionId action;
    bool on;
  };
  const Tool tools[5] = {
      {"PEN", ActionPen, tool == ink::Tool::Pen},
      {"ERASE", ActionEraser, tool == ink::Tool::Eraser},
      {"UNDO", ActionUndo, false},
      {armed ? "SURE?" : "CLEAR", ActionClear, armed},
      {"LINES", ActionLines, ruled},
  };
  fui::TextStyle small;
  small.font = toybox::kTileFont;
  small.align = fui::TextAlign::Center;
  for (int i = 0; i < 5; ++i) {
    // Five across leaves 88px a button, too narrow for the button face, so the
    // button is drawn bare and labelled in the small face on top.
    const fui::Rect box =
        fui::makeRect(static_cast<int16_t>(4 + i * (bw + gap)), toolY, bw, static_cast<int16_t>(kToolH - 10));
    fui::ButtonProps b;
    b.label = "";
    b.action = tools[i].action;
    b.styles = toybox::rowStyles();
    b.state = tools[i].on ? fui::StateSelected : fui::StateNormal;
    screen.button(b, box);
    small.color = tools[i].on ? fui::Color::White : fui::Color::Black;
    t.text(toybox::inkCentred(box, toybox::kTileCut), tools[i].label, small);
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Handwriting");
  renderer.displayBuffer();
}

void HandwritingActivity::loadIndex() {
  current = 1;
  pageCount = 1;
  uint8_t data[4] = {};
  HalFile file;
  if (!Storage.exists(kIndexPath) || !Storage.openFileForRead("INK", kIndexPath, file)) return;
  if (file.read(data, sizeof(data)) != sizeof(data) || data[0] != kIndexVersion) return;
  pageCount = data[1] >= 1 && data[1] <= kMaxPages ? data[1] : 1;
  current = data[2] >= 1 && data[2] <= pageCount ? data[2] : 1;
  ruled = data[3] != 0;
}

void HandwritingActivity::saveIndex() {
  HalFile file;
  if (!Storage.openFileForWrite("INK", kIndexPath, file)) return;
  const uint8_t data[4] = {kIndexVersion, static_cast<uint8_t>(pageCount), static_cast<uint8_t>(current),
                           static_cast<uint8_t>(ruled ? 1 : 0)};
  file.write(data, sizeof(data));
  file.flush();
}

void HandwritingActivity::loadPage(const int number) {
  page->clear();
  dirty = false;
  full = false;
  char path[40];
  pagePath(number, "ink", path, sizeof(path));
  HalFile file;
  if (!Storage.exists(path) || !Storage.openFileForRead("INK", path, file)) return;
  char magic[4] = {};
  uint32_t count = 0;
  if (file.read(reinterpret_cast<uint8_t*>(magic), 4) != 4 || std::memcmp(magic, kInkMagic, 4) != 0) return;
  if (file.read(reinterpret_cast<uint8_t*>(&count), 4) != 4 || count > static_cast<uint32_t>(ink::kMaxPoints)) return;
  // Read straight into a scratch copy of the right size, then hand it over.
  auto points = makeUniqueNoThrow<ink::Point[]>(count == 0 ? 1 : count);
  if (!points) return;
  const size_t bytes = sizeof(ink::Point) * count;
  if (file.read(reinterpret_cast<uint8_t*>(points.get()), bytes) != bytes) return;
  page->assign(points.get(), static_cast<int>(count));
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

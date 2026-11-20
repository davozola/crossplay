#include "PaintActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../kids/KidsPictures.h"
#include "../ui/ToyboxFonts.h"

namespace {

constexpr char kSavePath[] = "/.crosspoint/pinta.bin";
constexpr int kHintsH = 40;  // the button hints along the bottom
constexpr int kToolbarH = 100;
constexpr int kButtons = 5;  // thin, fat, stamp, rubber, clean sheet
constexpr int kButtonGap = 8;
constexpr int kBackSwipeEdge = 28;
constexpr int kPictures = sizeof(kidspictures::kAll) / sizeof(kidspictures::kAll[0]);

}  // namespace

std::unique_ptr<Activity> PaintActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<PaintActivity>(renderer, mappedInput);
}

int PaintActivity::toolbarTop() const { return renderer.getScreenHeight() - kHintsH - kToolbarH; }
int PaintActivity::sheetHeight() const { return toolbarTop() - 4; }

void PaintActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  const int w = renderer.getScreenWidth();
  const int h = sheetHeight();
  // One allocation for the life of the activity: the sheet is ~39 KB.
  const size_t bytes = static_cast<size_t>((w + 7) / 8) * static_cast<size_t>(h);
  bits_ = makeUniqueNoThrow<uint8_t[]>(bytes);
  if (bits_) sheet_ = makeUniqueNoThrow<paint::Sheet>(bits_.get(), w, h);
  if (!bits_ || !sheet_) {
    LOG_ERR("PINT", "OOM: %u bytes for the sheet", static_cast<unsigned>(bytes));
    shelf::leave(renderer, mappedInput);
    return;
  }
  sheet_->clear();
  loadSheet();
  picture_ = static_cast<int>(millis() % kPictures);
  drawing_ = false;
  repaint();
}

void PaintActivity::onExit() {
  if (dirty_) saveSheet();
  sheet_.reset();
  bits_.reset();
  Activity::onExit();
}

void PaintActivity::repaint() {
  painting_ = true;
  requestUpdate();
}

void PaintActivity::loop() {
  if (!sheet_) return;
  // A stroke that starts near the left edge and runs right is also the
  // system's Back swipe. While a stroke is being drawn it is a stroke.
  if (!drawing_ && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (painting_) return;
    if (dirty_) saveSheet();
    shelf::leave(renderer, mappedInput);
    return;
  }

  const int bottom = sheetHeight();
  int x = 0;
  int y = 0;
  if (!drawing_ && mappedInput.wasScreenTouchPressed(x, y) && y < bottom && x >= kBackSwipeEdge) {
    if (tool_ == Tool::Stamp) {
      const freeink::Icon& icon = *kidspictures::kAll[picture_];
      sheet_->stamp(icon.bits, icon.w, icon.h, x, y, 1);
      // A different picture next time, so a row of taps is a row of animals.
      picture_ = (picture_ + 1 + static_cast<int>(millis() % 7)) % kPictures;
      dirty_ = true;
      repaint();
      return;
    }
    drawing_ = true;
    lastX_ = x;
    lastY_ = y;
    const bool ink = tool_ != Tool::Rubber;
    sheet_->dot(x, y, tool_ == Tool::Thin ? paint::kThinRadius : (ink ? paint::kFatRadius : paint::kRubberRadius), ink);
    moved_ = true;
    dirty_ = true;
    return;
  }
  if (drawing_) {
    if (mappedInput.isScreenTouchHeld(x, y)) {
      const int cy = y >= bottom ? bottom - 1 : y;
      if (x != lastX_ || cy != lastY_) {
        const bool ink = tool_ != Tool::Rubber;
        sheet_->line(lastX_, lastY_, x, cy,
                     tool_ == Tool::Thin ? paint::kThinRadius : (ink ? paint::kFatRadius : paint::kRubberRadius), ink);
        lastX_ = x;
        lastY_ = cy;
        moved_ = true;
      }
      if (!painting_ && moved_) repaint();
      return;
    }
    drawing_ = false;
    repaint();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  const int top = toolbarTop();
  if (tapY < top || tapY >= top + kToolbarH) return;
  const int w = renderer.getScreenWidth();
  const int index = tapX * kButtons / w;
  switch (index) {
    case 0:
      tool_ = Tool::Thin;
      break;
    case 1:
      tool_ = Tool::Fat;
      break;
    case 2:
      // Tapping the stamp again turns to the next picture.
      if (tool_ == Tool::Stamp) picture_ = (picture_ + 1) % kPictures;
      tool_ = Tool::Stamp;
      break;
    case 3:
      tool_ = Tool::Rubber;
      break;
    default:
      sheet_->clear();
      dirty_ = true;
      if (tool_ == Tool::Rubber) tool_ = Tool::Fat;  // a clean sheet wants paint, not a rubber
      break;
  }
  repaint();
}

void PaintActivity::drawButton(const int index, const bool selected) const {
  const int w = renderer.getScreenWidth();
  const int bw = (w - kButtonGap * (kButtons + 1)) / kButtons;
  const int x = kButtonGap + index * (bw + kButtonGap);
  const int y = toolbarTop() + 6;
  const int bh = kToolbarH - 12;
  renderer.fillRoundedRect(x, y, bw, bh, 16, Color::Black);
  if (!selected) renderer.fillRoundedRect(x + 4, y + 4, bw - 8, bh - 8, 12, Color::White);
  const bool ink = !selected;  // the glyph is black on a white button, white on the chosen one
  const int cx = x + bw / 2;
  const int cy = y + bh / 2;
  auto disc = [&](const int radius, const bool black) {
    for (int dy = -radius; dy <= radius; ++dy) {
      int half = 0;
      while ((half + 1) * (half + 1) + dy * dy <= radius * radius) ++half;
      renderer.fillRect(cx - half, cy + dy, half * 2 + 1, 1, black);
    }
  };
  switch (index) {
    case 0:
      disc(paint::kThinRadius, ink);
      break;
    case 1:
      disc(paint::kFatRadius, ink);
      break;
    case 2: {
      // The picture the stamp will leave, at half size.
      const freeink::Icon& icon = *kidspictures::kAll[picture_];
      const int rowBytes = (icon.w + 7) / 8;
      for (int sy = 0; sy < icon.h; sy += 2) {
        for (int sx = 0; sx < icon.w; sx += 2) {
          const bool a = !((icon.bits[sy * rowBytes + sx / 8] >> (7 - sx % 8)) & 1);
          const bool b = sx + 1 < icon.w && !((icon.bits[sy * rowBytes + (sx + 1) / 8] >> (7 - (sx + 1) % 8)) & 1);
          if (a || b) renderer.drawPixel(cx - icon.w / 4 + sx / 2, cy - icon.h / 4 + sy / 2, ink);
        }
      }
      break;
    }
    case 3:
      // The rubber: a ring, the brush with nothing in it.
      disc(26, ink);
      disc(20, !ink);
      break;
    default:
      // A clean sheet: a blank page with its corner turned down.
      renderer.fillRect(cx - 20, cy - 26, 40, 52, ink);
      renderer.fillRect(cx - 16, cy - 22, 32, 44, !ink);
      {
        const int xs[3] = {cx + 6, cx + 20, cx + 20};
        const int ys[3] = {cy - 26, cy - 26, cy - 12};
        renderer.fillPolygon(xs, ys, 3, !ink);
        renderer.drawLine(cx + 6, cy - 26, cx + 20, cy - 12, 4, ink);
      }
      break;
  }
}

void PaintActivity::render(RenderLock&&) {
  moved_ = false;
  renderer.setClipRect(0, 0, 32767, 32767);  // open, as the renderer starts: a screen-sized clip set upright cuts a page drawn wide
  renderer.clearScreen();
  if (sheet_) {
    const uint8_t* bits = sheet_->bits();
    const int rowBytes = sheet_->rowBytes();
    for (int y = 0; y < sheet_->height(); ++y) {
      const uint8_t* row = bits + y * rowBytes;
      for (int b = 0; b < rowBytes; ++b) {
        if (!row[b]) continue;
        for (int bit = 0; bit < 8; ++bit) {
          if (row[b] & (0x80 >> bit)) renderer.drawPixel(b * 8 + bit, y, true);
        }
      }
    }
  }
  renderer.fillRect(0, toolbarTop() - 4, renderer.getScreenWidth(), 4, true);
  for (int i = 0; i < kButtons; ++i) {
    const bool selected = (i == 0 && tool_ == Tool::Thin) || (i == 1 && tool_ == Tool::Fat) ||
                          (i == 2 && tool_ == Tool::Stamp) || (i == 3 && tool_ == Tool::Rubber);
    drawButton(i, selected);
  }
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
  painting_ = false;
}

void PaintActivity::saveSheet() {
  if (!sheet_) return;
  HalFile file;
  if (!Storage.openFileForWrite("PINT", kSavePath, file)) return;
  const uint8_t header[4] = {1, static_cast<uint8_t>(sheet_->width() / 8),
                             static_cast<uint8_t>(sheet_->height() & 0xFF),
                             static_cast<uint8_t>(sheet_->height() >> 8)};
  file.write(header, sizeof(header));
  file.write(sheet_->bits(), static_cast<size_t>(sheet_->rowBytes()) * static_cast<size_t>(sheet_->height()));
  file.flush();
  dirty_ = false;
}

void PaintActivity::loadSheet() {
  if (!sheet_ || !Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("PINT", kSavePath, file)) return;
  uint8_t header[4] = {};
  if (file.read(header, sizeof(header)) != sizeof(header) || header[0] != 1) return;
  // Only a sheet of this exact size: another panel's would not line up.
  if (header[1] != sheet_->width() / 8 || (header[2] | (header[3] << 8)) != sheet_->height()) return;
  const size_t bytes = static_cast<size_t>(sheet_->rowBytes()) * static_cast<size_t>(sheet_->height());
  if (file.read(bits_.get(), bytes) != static_cast<int>(bytes)) sheet_->clear();
}

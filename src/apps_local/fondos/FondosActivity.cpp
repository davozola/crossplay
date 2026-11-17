#include "FondosActivity.h"

#include <Bitmap.h>
#include <FontCacheManager.h>
#include <HalDisplay.h>
#include <Logging.h>
#include <Memory.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../../CrossPointSettings.h"
#include "../../components/UITheme.h"
#include "../../fontIds.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"

#if defined(FONDOS_EMBEDDED)
extern "C" {
extern const uint8_t fondos_pack_start[];
extern const uint8_t fondos_pack_end[];
}
#endif

namespace fui = freeink::ui;
namespace lab = toyboxlab;
using fondos::Mode;

namespace {

enum : fui::ActionId {
  ActionPrevious = 1,
  ActionNext,
  ActionUse,
  ActionEvery,
};

constexpr char kTag[] = "FONDOS";
// Where the pack is read from when the firmware does not carry it.
constexpr char kPackOnCard[] = "/fondos.pack";
constexpr char kStatePath[] = "/.crosspoint/fondos.cfg";
// The picture on the page, copied out so the card's BMP reader can draw it.
constexpr char kViewPath[] = "/.crosspoint/fondos-view.bmp";
// The sleep screen's own places: one picture that wins, and a folder it picks from.
constexpr char kPinned[] = "/sleep.bmp";
constexpr char kRotation[] = "/.sleep";
constexpr char kRotationPrefix[] = "fondo-";
constexpr int kPanel = 196;  // the white foot of the page, where the words and buttons are
constexpr uint8_t kCleanAfterPaints = 6;

// A Latin-1 string as the UTF-8 the faces are drawn from.
void toUtf8(const uint8_t* latin, const size_t cap, char* out, const size_t outCap) {
  size_t at = 0;
  for (size_t i = 0; i < cap && latin[i] != 0 && at + 3 < outCap; ++i) {
    const uint8_t c = latin[i];
    if (c < 0x80) {
      out[at++] = static_cast<char>(c);
    } else {
      out[at++] = static_cast<char>(0xC0 | (c >> 6));
      out[at++] = static_cast<char>(0x80 | (c & 0x3F));
    }
  }
  out[at] = 0;
}

}  // namespace

std::unique_ptr<Activity> FondosActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<FondosActivity>(renderer, mappedInput);
}

// --- The pack ------------------------------------------------------------------

bool FondosActivity::openPack() {
#if defined(FONDOS_EMBEDDED)
  embedded_ = fondos_pack_start;
  header_ = fondos::parseHeader(embedded_, static_cast<size_t>(fondos_pack_end - fondos_pack_start));
  if (header_.ok()) return true;
  embedded_ = nullptr;
#endif
  HalFile file;
  if (!Storage.exists(kPackOnCard) || !Storage.openFileForRead(kTag, kPackOnCard, file)) return false;
  uint8_t top[fondos::kHeaderBytes];
  if (file.read(top, sizeof(top)) != static_cast<int>(sizeof(top))) return false;
  header_ = fondos::parseHeader(top, static_cast<size_t>(file.size()));
  return header_.ok();
}

bool FondosActivity::readPack(const size_t at, uint8_t* out, const size_t bytes) {
  if (embedded_ != nullptr) {
    std::memcpy(out, embedded_ + at, bytes);
    return true;
  }
  HalFile file;
  if (!Storage.openFileForRead(kTag, kPackOnCard, file) || !file.seek(at)) return false;
  return file.read(out, bytes) == static_cast<int>(bytes);
}

void FondosActivity::readText(const int index) {
  uint8_t text[fondos::kTextBytes] = {};
  title_[0] = artist_[0] = 0;
  if (!readPack(header_.textAt(index), text, sizeof(text))) return;
  toUtf8(text, fondos::kTitleBytes, title_, sizeof(title_));
  toUtf8(text + fondos::kTitleBytes, fondos::kArtistBytes, artist_, sizeof(artist_));
}

// One picture of the pack as a file on the card.
bool FondosActivity::writePicture(const int index, const char* path) {
  HalFile out;
  if (!Storage.openFileForWrite(kTag, path, out)) return false;
  const size_t from = header_.pictureAt(index);
  if (embedded_ != nullptr) return out.write(embedded_ + from, header_.pictureBytes) == header_.pictureBytes;
  // From the card to the card, a piece at a time.
  constexpr size_t kChunk = 2048;
  auto chunk = makeUniqueNoThrow<uint8_t[]>(kChunk);
  if (!chunk) {
    LOG_ERR(kTag, "OOM: %u bytes", static_cast<unsigned>(kChunk));
    return false;
  }
  HalFile in;
  if (!Storage.openFileForRead(kTag, kPackOnCard, in) || !in.seek(from)) return false;
  for (size_t left = header_.pictureBytes; left > 0;) {
    const size_t want = std::min(left, kChunk);
    if (in.read(chunk.get(), want) != static_cast<int>(want) || out.write(chunk.get(), want) != want) return false;
    left -= want;
  }
  return true;
}

// --- What has been chosen --------------------------------------------------------

void FondosActivity::loadState() {
  mode_ = Mode::Off;
  chosen_ = 0;
  HalFile file;
  uint8_t bytes[fondos::kStateBytes] = {};
  if (!Storage.exists(kStatePath) || !Storage.openFileForRead(kTag, kStatePath, file)) return;
  const int got = file.read(bytes, sizeof(bytes));
  fondos::unpackState(bytes, got, header_.count, mode_, chosen_);
}

void FondosActivity::saveState() {
  HalFile file;
  if (!Storage.openFileForWrite(kTag, kStatePath, file)) return;
  uint8_t bytes[fondos::kStateBytes];
  fondos::packState(mode_, chosen_, bytes);
  file.write(bytes, sizeof(bytes));
  file.flush();
}

// The sleep screen only draws pictures in its custom mode.
void FondosActivity::useCustomSleep() {
  if (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM) return;
  SETTINGS.sleepScreen = CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM;
  SETTINGS.saveToFile();
}

// Takes this app's pictures out of the folder the sleep screen picks from.
// Anything else in there is somebody else's and stays.
void FondosActivity::clearRotation() {
  char path[48];
  for (int i = 0; i < header_.count; ++i) {
    std::snprintf(path, sizeof(path), "%s/%s%03d.bmp", kRotation, kRotationPrefix, i % 1000);
    if (Storage.exists(path)) Storage.remove(path);
  }
}

void FondosActivity::say(const char* message) {
  note_ = message;
  busy_ = true;
  requestUpdateAndWait();
  busy_ = false;
}

void FondosActivity::useThisOne() {
  say("SAVING IT");
  clearRotation();
  if (!writePicture(index_, kPinned)) {
    note_ = "THE CARD DID NOT TAKE IT";
    return;
  }
  useCustomSleep();
  mode_ = Mode::One;
  chosen_ = index_;
  saveState();
  note_ = "THIS IS YOUR SLEEP SCREEN NOW";
}

void FondosActivity::toggleEvery() {
  if (mode_ == Mode::Every) {
    // Back to one picture: the one on the page.
    useThisOne();
    return;
  }
  say("COPYING THE PICTURES TO THE CARD");
  Storage.mkdir(kRotation);
  char path[48];
  for (int i = 0; i < header_.count; ++i) {
    std::snprintf(path, sizeof(path), "%s/%s%03d.bmp", kRotation, kRotationPrefix, i % 1000);
    if (!writePicture(i, path)) {
      clearRotation();
      note_ = "THE CARD DID NOT TAKE THEM";
      return;
    }
  }
  // A single pinned picture would win over the folder every time.
  if (Storage.exists(kPinned)) Storage.remove(kPinned);
  useCustomSleep();
  mode_ = Mode::Every;
  saveState();
  note_ = "A DIFFERENT ONE EVERY TIME IT SLEEPS";
}

// --- Lifecycle -----------------------------------------------------------------

void FondosActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  shownOnCard_ = -1;
  note_ = "";
  if (openPack()) {
    loadState();
    // A new order every visit, led by the picture in use when there is one.
    const uint32_t seed =
        (static_cast<uint32_t>(millis()) * 2654435761u) ^ static_cast<uint32_t>(esp_random()) ^ 0x9E3779B9u;
    fondos::shuffled(order_, header_.count, seed | 1u, mode_ == Mode::One ? chosen_ : -1);
    place_ = 0;
    index_ = order_[0];
    readText(index_);
  }
  requestUpdate();
}

void FondosActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    Storage.remove(kViewPath);
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (!header_.ok()) return;

  fui::ActionId action = 0;
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Down) || mappedInput.wasReleased(Button::Right)) action = ActionNext;
  if (mappedInput.wasReleased(Button::Up) || mappedInput.wasReleased(Button::Left)) action = ActionPrevious;
  if (mappedInput.wasReleased(Button::Confirm)) action = ActionUse;

  int tapX = 0;
  int tapY = 0;
  if (action == 0 && mappedInput.wasScreenTapped(tapX, tapY) && interactionsReady_) {
    if (tapY < renderer.getScreenHeight() - kPanel) {
      // The picture itself turns the page: its left third back, the rest on.
      action = tapX < renderer.getScreenWidth() / 3 ? ActionPrevious : ActionNext;
    } else {
      fui::InputSnapshot input;
      input.touchReleased = true;
      input.touchX = static_cast<int16_t>(tapX);
      input.touchY = static_cast<int16_t>(tapY);
      action = interactions_.route(input).action;
    }
  }

  switch (action) {
    case ActionPrevious:
    case ActionNext:
      place_ = fondos::step(place_, action == ActionNext ? 1 : -1, header_.count);
      index_ = order_[place_];
      readText(index_);
      note_ = "";
      break;
    case ActionUse:
      useThisOne();
      break;
    case ActionEvery:
      toggleEvery();
      break;
    default:
      return;
  }
  requestUpdate();
}

void FondosActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();

  // The picture first, whole; the foot of the page is drawn over its bottom.
  bool drawn = false;
  if (header_.ok()) {
    if (shownOnCard_ != index_) shownOnCard_ = writePicture(index_, kViewPath) ? index_ : -1;
    const FontCacheManager* fonts = renderer.getFontCacheManager();
    HalFile file;
    if (shownOnCard_ == index_ && (fonts == nullptr || !fonts->isScanning()) &&
        Storage.openFileForRead(kTag, kViewPath, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        renderer.drawBitmap1Bit(bitmap, 0, 0, width, height);
        drawn = true;
      }
    }
  }

  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  const int x = toybox::kMargin;
  const int inner = width - 2 * toybox::kMargin;

  if (!header_.ok()) {
    lab::chrome(screen, "WALLPAPERS");
    lab::paragraph(
        screen, x, toybox::kChromeHeight + 24, inner,
        "THE PICTURES ARE NOT ON THIS READER. COPY FONDOS.PACK FROM THE PROJECT'S SRC/APPS_LOCAL/FONDOS FOLDER "
        "TO THE TOP OF THE MEMORY CARD.",
        6);
  } else {
    const int top = height - kPanel;
    renderer.fillRect(0, top, width, kPanel, false);
    renderer.fillRect(0, top, width, 3, true);
    if (!drawn) lab::caption(screen, x, top / 2, inner, "THE PICTURE COULD NOT BE DRAWN", fui::TextAlign::Center);

    // What it is, and where in the pack; or what just happened.
    char place[toybox::kIntChars * 2 + 8];
    std::snprintf(place, sizeof(place), "%d / %d", (place_ + 1) % 1000, header_.count % 1000);
    const int placeW = renderer.getTextWidth(NOTOSANS_12_FONT_ID, place);
    renderer.drawText(NOTOSANS_12_FONT_ID, x + inner - placeW, top + 10, place, true);
    if (note_[0] != 0) {
      lab::caption(screen, x, top + 18, inner - placeW - 12, note_);
    } else {
      renderer.drawText(
          NOTOSANS_12_FONT_ID, x, top + 8,
          renderer.truncatedText(NOTOSANS_12_FONT_ID, title_, inner - placeW - 12, EpdFontFamily::BOLD).c_str(), true,
          EpdFontFamily::BOLD);
      renderer.drawText(NOTOSANS_12_FONT_ID, x, top + 32,
                        renderer.truncatedText(NOTOSANS_12_FONT_ID, artist_, inner).c_str(), true);
    }
    if (!busy_) {
      const int rowY = top + 64;
      const int rowH = 58;
      const int arrow = 72;
      const int gap = 8;
      const bool isChosen = mode_ == Mode::One && chosen_ == index_;
      lab::button(screen, lab::rect(x, rowY, arrow, rowH), "<", ActionPrevious, 0, false, false);
      lab::button(screen, lab::rect(x + arrow + gap, rowY, inner - 2 * (arrow + gap), rowH),
                  isChosen ? "IN USE" : "USE THIS ONE", ActionUse, 0, isChosen, false);
      lab::button(screen, lab::rect(x + inner - arrow, rowY, arrow, rowH), ">", ActionNext, 0, false, false);
      lab::button(screen, lab::rect(x, rowY + rowH + gap, inner, 52),
                  mode_ == Mode::Every ? "A NEW ONE EVERY SLEEP: ON" : "A NEW ONE EVERY SLEEP: OFF", ActionEvery, 0,
                  mode_ == Mode::Every, true);
    }
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Fondos");
  // A whole picture changing leaves a ghost of the last on a fast paint.
  const bool clean = ++paints_ >= kCleanAfterPaints;
  if (clean) paints_ = 0;
  renderer.displayBuffer(clean ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
}

#include "LogicActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cmath>
#include <cstdio>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxHowTo.h"
#include "../ui/ToyboxTheme.h"

namespace lg = logic;
namespace fui = freeink::ui;

namespace {

constexpr uint8_t kSaveVersion = 1;
constexpr int kBlobMax = 512;
constexpr uint32_t kResetArmMs = 3000;
// Presses starting this close to the left edge are the back swipe.
constexpr int kBackSwipeEdge = 12;

struct SaveHeader {
  uint8_t version;
  uint8_t size;
  uint8_t number;
  int8_t savedSize;
  int8_t savedNumber;
  uint8_t reserved[3];
  uint32_t solved[lg::kSizes];
  uint16_t blobLength;
} __attribute__((packed));

constexpr const char* kSlitherPages[] = {
    "DRAW ONE SINGLE LOOP ALONG THE LINES BETWEEN THE DOTS. IT NEVER CROSSES ITSELF, NEVER BRANCHES, AND HAS NO "
    "LOOSE ENDS.",
    "A NUMBER SAYS HOW MANY OF THE FOUR SIDES OF ITS SQUARE THE LOOP USES. A SQUARE WITH NO NUMBER MAY HAVE ANY.",
    "TAP BETWEEN TWO DOTS TO DRAW A LINE. TAP AGAIN FOR A CROSS, WHICH IS YOUR NOTE THAT NO LINE GOES THERE, AND "
    "ONCE MORE TO CLEAR IT. BEGIN WITH THE 0S AND THE 3S: A 0 RULES OUT FOUR SIDES AT ONCE, AND A 3 BESIDE A 0 "
    "HAS ONLY ONE WAY TO GO.",
};
constexpr const char* kShikakuPages[] = {
    "CUT THE WHOLE GRID INTO RECTANGLES. EACH RECTANGLE HOLDS EXACTLY ONE NUMBER, AND THAT NUMBER IS HOW MANY "
    "SQUARES IT COVERS.",
    "A 6 CAN BE 1 BY 6, 2 BY 3, 3 BY 2 OR 6 BY 1. A 5 OR A 7 CAN ONLY BE A STRAIGHT STRIP. NO SQUARE IS LEFT OVER "
    "AND NO TWO RECTANGLES OVERLAP.",
    "TAP ONE CORNER OF A RECTANGLE, THEN THE OPPOSITE CORNER. TAP A RECTANGLE TO TAKE IT AWAY. BEGIN WITH NUMBERS "
    "IN CORNERS AND WITH THE BIG ONES: THEY HAVE THE FEWEST PLACES TO FIT.",
};
constexpr const char* kAkariPages[] = {
    "PUT BULBS ON WHITE SQUARES UNTIL EVERY WHITE SQUARE IS LIT. A BULB LIGHTS ITS ROW AND ITS COLUMN, AS FAR AS "
    "THE NEXT BLACK SQUARE.",
    "NO BULB MAY SHINE ON ANOTHER BULB. A NUMBER ON A BLACK SQUARE SAYS HOW MANY BULBS TOUCH ITS FOUR SIDES; "
    "CORNERS DO NOT COUNT.",
    "TAP A SQUARE FOR A BULB. TAP AGAIN FOR A DOT, YOUR NOTE THAT NO BULB GOES THERE, AND ONCE MORE TO CLEAR IT. "
    "BEGIN WITH THE 4S AND THE 0S, THEN LOOK FOR A SQUARE ONLY ONE BULB COULD EVER LIGHT.",
};
constexpr const char* kHashiPages[] = {
    "JOIN THE ISLANDS WITH BRIDGES. THE NUMBER ON AN ISLAND IS HOW MANY BRIDGES LEAVE IT.",
    "BRIDGES RUN STRAIGHT, ACROSS OR UP AND DOWN. THEY NEVER CROSS EACH OTHER OR PASS OVER AN ISLAND, AND AT MOST "
    "TWO JOIN THE SAME PAIR. AT THE END YOU MUST BE ABLE TO WALK FROM ANY ISLAND TO ANY OTHER.",
    "TAP BETWEEN TWO ISLANDS: ONE BRIDGE, TWO, THEN NONE. BEGIN WITH ISLANDS THAT HAVE NO CHOICE: AN 8 IN THE "
    "MIDDLE, A 6 ON AN EDGE OR A 4 IN A CORNER NEEDS TWO BRIDGES EVERY WAY IT CAN GO.",
};
constexpr toyboxhowto::Guide kGuides[lg::kGames] = {
    {kSlitherPages, 3},
    {kShikakuPages, 3},
    {kAkariPages, 3},
    {kHashiPages, 3},
};

}  // namespace

std::unique_ptr<Activity> LogicActivity::createSlither(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<LogicActivity>(renderer, mappedInput, lg::Game::Slither);
}
std::unique_ptr<Activity> LogicActivity::createShikaku(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<LogicActivity>(renderer, mappedInput, lg::Game::Shikaku);
}
std::unique_ptr<Activity> LogicActivity::createAkari(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<LogicActivity>(renderer, mappedInput, lg::Game::Akari);
}
std::unique_ptr<Activity> LogicActivity::createHashi(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<LogicActivity>(renderer, mappedInput, lg::Game::Hashi);
}

void LogicActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  view_ = View::Picker;
  requestUpdate();
}

void LogicActivity::onExit() {
  if (view_ == View::Board) saveState();
  Activity::onExit();
}

void LogicActivity::savePath(char* out, const size_t cap) const {
  std::snprintf(out, cap, "/.crosspoint/logic-%d.sav", static_cast<int>(game_));
}

void LogicActivity::loadBoard() {
  const lg::Board board = lg::boardOf(game_, size_, number_);
  switch (game_) {
    case lg::Game::Slither:
      slither_.load(board);
      break;
    case lg::Game::Shikaku:
      shikaku_.load(board);
      break;
    case lg::Game::Akari:
      akari_.load(board);
      break;
    case lg::Game::Hashi:
      hashi_.load(board);
      break;
  }
  solvedNow_ = false;
  touched_ = false;
  resetArmedMs_ = 0;
}

bool LogicActivity::isSolved() const {
  switch (game_) {
    case lg::Game::Slither:
      return slither_.solved();
    case lg::Game::Shikaku:
      return shikaku_.solved();
    case lg::Game::Akari:
      return akari_.solved();
    case lg::Game::Hashi:
      return hashi_.solved();
  }
  return false;
}

void LogicActivity::openBoard(const int number, const bool resume) {
  number_ = number;
  loadBoard();
  if (resume && savedSize_ == size_ && savedNumber_ == number_) {
    // The marks follow the header in the same file.
    char path[48];
    savePath(path, sizeof(path));
    HalFile file;
    SaveHeader header{};
    auto blob = makeUniqueNoThrow<uint8_t[]>(kBlobMax);
    if (blob && Storage.exists(path) && Storage.openFileForRead("LOGC", path, file) &&
        file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
        header.blobLength <= kBlobMax && file.read(blob.get(), header.blobLength) == header.blobLength) {
      bool ok = false;
      switch (game_) {
        case lg::Game::Slither:
          ok = slither_.restore(blob.get(), header.blobLength);
          break;
        case lg::Game::Shikaku:
          ok = shikaku_.restore(blob.get(), header.blobLength);
          break;
        case lg::Game::Akari:
          ok = akari_.restore(blob.get(), header.blobLength);
          break;
        case lg::Game::Hashi:
          ok = hashi_.restore(blob.get(), header.blobLength);
          break;
      }
      if (!ok) loadBoard();
      solvedNow_ = isSolved();
    }
  }
  view_ = View::Board;
}

void LogicActivity::loadState() {
  char path[48];
  savePath(path, sizeof(path));
  if (!Storage.exists(path)) return;
  HalFile file;
  if (!Storage.openFileForRead("LOGC", path, file)) return;
  SaveHeader header{};
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) return;
  if (header.version != kSaveVersion) return;
  size_ = header.size < lg::kSizes ? header.size : 0;
  number_ = header.number < lg::boardsPerSize() ? header.number : 0;
  savedSize_ = header.savedSize;
  savedNumber_ = header.savedNumber;
  for (int i = 0; i < lg::kSizes; ++i) solved_[i] = header.solved[i];
}

void LogicActivity::saveState() {
  auto blob = makeUniqueNoThrow<uint8_t[]>(kBlobMax);
  if (!blob) {
    LOG_ERR("LOGC", "OOM: %d bytes for the save", kBlobMax);
    return;
  }
  int length = 0;
  if (view_ == View::Board) {
    switch (game_) {
      case lg::Game::Slither:
        length = slither_.save(blob.get(), kBlobMax);
        break;
      case lg::Game::Shikaku:
        length = shikaku_.save(blob.get(), kBlobMax);
        break;
      case lg::Game::Akari:
        length = akari_.save(blob.get(), kBlobMax);
        break;
      case lg::Game::Hashi:
        length = hashi_.save(blob.get(), kBlobMax);
        break;
    }
    savedSize_ = size_;
    savedNumber_ = number_;
  } else {
    // From the picker only the header changes; the marks already on the card
    // are read back so they can be written again behind it.
    char path[48];
    savePath(path, sizeof(path));
    HalFile old;
    SaveHeader header{};
    if (Storage.exists(path) && Storage.openFileForRead("LOGC", path, old) &&
        old.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
        header.blobLength <= kBlobMax && old.read(blob.get(), header.blobLength) == header.blobLength) {
      length = header.blobLength;
    }
  }
  SaveHeader header{};
  header.version = kSaveVersion;
  header.size = static_cast<uint8_t>(size_);
  header.number = static_cast<uint8_t>(number_);
  header.savedSize = static_cast<int8_t>(savedSize_);
  header.savedNumber = static_cast<int8_t>(savedNumber_);
  for (int i = 0; i < lg::kSizes; ++i) header.solved[i] = solved_[i];
  header.blobLength = static_cast<uint16_t>(length);
  char path[48];
  savePath(path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForWrite("LOGC", path, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  if (length > 0) file.write(blob.get(), static_cast<size_t>(length));
  file.flush();
}

// A tap on the board, in panel pixels. True when it changed something.
bool LogicActivity::boardTap(const int x, const int y) {
  const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
  const logicui::Geometry g = logicui::boardGeometry(device, lg::sideOf(game_, size_));
  const float fx = static_cast<float>(x - g.x) / static_cast<float>(g.cell);
  const float fy = static_cast<float>(y - g.y) / static_cast<float>(g.cell);
  const float side = static_cast<float>(g.side);
  if (game_ == lg::Game::Slither) {
    // The nearest side of the cell under the finger; the middle of a cell is
    // nobody's edge, and a tap just outside the frame still finds the frame.
    if (fx < -0.35f || fy < -0.35f || fx > side + 0.35f || fy > side + 0.35f) return false;
    int cx = static_cast<int>(std::floor(fx));
    int cy = static_cast<int>(std::floor(fy));
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx >= g.side) cx = g.side - 1;
    if (cy >= g.side) cy = g.side - 1;
    const float u = fx - static_cast<float>(cx);
    const float v = fy - static_cast<float>(cy);
    const float toLeft = std::fabs(u);
    const float toRight = std::fabs(1 - u);
    const float toTop = std::fabs(v);
    const float toBottom = std::fabs(1 - v);
    float best = toLeft;
    int which = 0;
    if (toRight < best) {
      best = toRight;
      which = 1;
    }
    if (toTop < best) {
      best = toTop;
      which = 2;
    }
    if (toBottom < best) {
      best = toBottom;
      which = 3;
    }
    if (best > 0.36f) return false;
    if (which == 0) slither_.cycleV(cx, cy);
    if (which == 1) slither_.cycleV(cx + 1, cy);
    if (which == 2) slither_.cycleH(cx, cy);
    if (which == 3) slither_.cycleH(cx, cy + 1);
    return true;
  }
  if (game_ == lg::Game::Hashi) {
    const int edge = hashi_.edgeNear(fx, fy);
    return edge >= 0 && hashi_.cycle(edge);
  }
  if (fx < 0 || fy < 0 || fx >= side || fy >= side) return false;
  const int cx = static_cast<int>(fx);
  const int cy = static_cast<int>(fy);
  if (game_ == lg::Game::Shikaku) {
    shikaku_.tap(cx, cy);
    return true;
  }
  if (!akari_.white(cx, cy)) return false;
  akari_.tap(cx, cy);
  return true;
}

void LogicActivity::loop() {
  const uint32_t now = static_cast<uint32_t>(millis());
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (howTo_.open) {
      howTo_.back();
      requestUpdate();
      return;
    }
    if (view_ == View::Board) {
      saveState();
      view_ = View::Picker;
      requestUpdate();
      return;
    }
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (resetArmedMs_ != 0 && now - resetArmedMs_ >= kResetArmMs) {
    resetArmedMs_ = 0;
    requestUpdate();
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);
  if (howTo_.open) {
    if (howTo_.tap(event, guide())) requestUpdate();
    return;
  }
  if (event.action == toyboxhowto::kOpen) {
    howTo_.start();
    requestUpdate();
    return;
  }
  if (event.action != fui::NO_ACTION) {
    route(event);
    return;
  }
  if (view_ == View::Picker) {
    const int board = logicui::pickerBoardAt(toybox::makeTarget(renderer).deviceContext(), tapX, tapY);
    if (board < 0) return;
    openBoard(board, true);
    requestUpdate();
    return;
  }
  if (solvedNow_ || tapX < kBackSwipeEdge) return;
  if (!boardTap(tapX, tapY)) return;
  touched_ = true;
  resetArmedMs_ = 0;
  if (isSolved()) {
    solvedNow_ = true;
    solved_[size_] |= 1u << number_;
    haptic_feedback::touchAction(true);
    saveState();
  }
  requestUpdate();
}

void LogicActivity::route(const fui::ActionEvent& event) {
  namespace ui = logicui;
  const uint32_t now = static_cast<uint32_t>(millis());
  switch (event.action) {
    case ui::ActionSize:
      if (event.value < 0 || event.value >= lg::kSizes) return;
      size_ = event.value;
      break;
    case ui::ActionReset:
      if (resetArmedMs_ != 0 && now - resetArmedMs_ < kResetArmMs) {
        loadBoard();
        saveState();
      } else {
        resetArmedMs_ = now == 0 ? 1 : now;
      }
      break;
    case ui::ActionNext: {
      if (view_ != View::Board) return;
      saveState();
      // The next board in this size that is not done yet; round to the start.
      int next = number_;
      for (int k = 1; k <= lg::boardsPerSize(); ++k) {
        const int candidate = (number_ + k) % lg::boardsPerSize();
        if (!((solved_[size_] >> candidate) & 1u)) {
          next = candidate;
          break;
        }
      }
      if (next == number_) next = (number_ + 1) % lg::boardsPerSize();
      openBoard(next, false);
      break;
    }
    default:
      return;
  }
  requestUpdate();
}

void LogicActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen surface(frame);

  if (howTo_.open) {
    toyboxhowto::build(surface, guide(), howTo_);
  } else if (view_ == View::Picker) {
    logicui::PickerModel m;
    m.game = game_;
    m.sizeIndex = size_;
    m.lastOpened = savedSize_ == size_ ? savedNumber_ : -1;
    m.solved = &solved_[size_];
    logicui::buildPicker(surface, m);
  } else {
    logicui::BoardModel m;
    m.game = game_;
    m.sizeIndex = size_;
    m.number = number_;
    m.solved = solvedNow_;
    m.resetArmed = resetArmedMs_ != 0;
    m.slither = &slither_;
    m.shikaku = &shikaku_;
    m.akari = &akari_;
    m.hashi = &hashi_;
    logicui::buildBoard(surface, m);
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Logic");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

const toyboxhowto::Guide& LogicActivity::guide() const { return kGuides[static_cast<int>(game_)]; }

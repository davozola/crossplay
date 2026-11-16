#include "KakuroActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "KakuroScreens.h"

namespace kk = kakuro;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/kakuro.sav";
// Bumped whenever the saved layout changes, or the bank is regenerated: a
// saved index means nothing against a different bank.
constexpr uint8_t kSaveVersion = 1;
// Progress is written every this many edits, and always on leaving or solving.
constexpr int kEditsPerSave = 8;

}  // namespace

std::unique_ptr<Activity> KakuroActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<KakuroActivity>(renderer, mappedInput);
}

void KakuroActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  screen = Screen::Menu;
  requestUpdate();
}

void KakuroActivity::onExit() {
  saveState();
  Activity::onExit();
}

void KakuroActivity::goTo(const Screen next) {
  screen = next;
  requestUpdate();
}

int KakuroActivity::solvedCount(const Slot& slot) const { return __builtin_popcountll(slot.solved); }

void KakuroActivity::open(const kk::Size size) {
  current = size;
  Slot& slot = slots[static_cast<int>(size)];
  if (!slot.started) {
    // The first puzzle of this size not yet solved.
    const int count = kk::countOf(size);
    int index = 0;
    while (index < count && (slot.solved & (1ull << index))) ++index;
    slot.index = static_cast<uint16_t>(index < count ? index : 0);
    slot.progress = kk::Progress{};
    slot.started = true;
  }
  kk::load(size, slot.index, layout);
  anyStarted = true;
  selected = -1;
  notes = false;
  saveState();
  goTo(Screen::Board);
}

void KakuroActivity::openNext() {
  Slot& slot = slots[static_cast<int>(current)];
  const int count = kk::countOf(current);
  int index = slot.index;
  for (int step = 1; step <= count; ++step) {
    const int candidate = (slot.index + step) % count;
    if (!(slot.solved & (1ull << candidate))) {
      index = candidate;
      break;
    }
    // Everything solved: just go round again.
    if (step == count) index = (slot.index + 1) % count;
  }
  slot.index = static_cast<uint16_t>(index);
  slot.progress = kk::Progress{};
  slot.started = true;
  kk::load(current, slot.index, layout);
  selected = -1;
  notes = false;
  saveState();
  requestUpdate();
}

void KakuroActivity::pressKey(const int key) {
  if (key == kakuroui::kKeyNotes) {
    notes = !notes;
    requestUpdate();
    return;
  }
  if (selected < 0 || !kk::white(layout, selected)) return;
  Slot& slot = slots[static_cast<int>(current)];
  kk::Progress& p = slot.progress;
  if (key == kakuroui::kKeyErase) {
    if (p.entry[selected] == 0 && p.notes[selected] == 0) return;
    kk::clearCell(p, selected);
  } else if (notes) {
    if (p.entry[selected] != 0) return;
    kk::toggleNote(p, selected, key);
  } else {
    // The same digit again takes it out.
    kk::setDigit(p, selected, p.entry[selected] == key ? 0 : key);
  }
  if (kk::solved(layout, p)) {
    slot.solved |= 1ull << slot.index;
    selected = -1;
    saveState();
  } else if (++unsavedEdits >= kEditsPerSave) {
    saveState();
  }
  requestUpdate();
}

void KakuroActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (screen == Screen::Menu) {
      saveState();
      shelf::leave(renderer, mappedInput);
      return;
    }
    saveState();
    goTo(Screen::Menu);
    return;
  }

  if (screen == Screen::HowTo) {
    const bool forward = mappedInput.wasReleased(MappedInputManager::Button::Down);
    const bool backward = mappedInput.wasReleased(MappedInputManager::Button::Up);
    if (forward || backward) {
      const int pages = kakuroui::howToPages();
      howToPage = (howToPage + (forward ? 1 : pages - 1)) % pages;
      requestUpdate();
      return;
    }
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  if (screen == Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    const bool done = kk::solved(layout, slots[static_cast<int>(current)].progress);
    const int cell = kakuroui::cellAt(device, layout.side, tapX, tapY);
    if (cell >= 0) {
      if (done || !kk::white(layout, cell) || cell == selected) return;
      selected = cell;
      requestUpdate();
      return;
    }
    if (!done) {
      const int key = kakuroui::keyAt(device, tapX, tapY);
      if (key > 0) {
        pressKey(key);
        return;
      }
    }
  }

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case kakuroui::ActionMenuRow:
      switch (static_cast<kakuroui::MenuRow>(event.value)) {
        case kakuroui::MenuRow::Continue:
          open(current);
          return;
        case kakuroui::MenuRow::Small:
          open(kk::Size::Small);
          return;
        case kakuroui::MenuRow::Medium:
          open(kk::Size::Medium);
          return;
        case kakuroui::MenuRow::Large:
          open(kk::Size::Large);
          return;
        case kakuroui::MenuRow::HowTo:
          howToPage = 0;
          goTo(Screen::HowTo);
          return;
        case kakuroui::MenuRow::Count:
          return;
      }
      return;
    case kakuroui::ActionHowToNext:
      if (howToPage + 1 < kakuroui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(Screen::Menu);
      return;
    case kakuroui::ActionNext:
      openNext();
      return;
    default:
      return;
  }
}

void KakuroActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case Screen::Menu: {
      kakuroui::MenuModel model;
      model.canContinue = anyStarted;
      model.continueSize = current;
      for (int i = 0; i < kk::kSizeCount; ++i) {
        model.solved[i] = solvedCount(slots[i]);
        model.total[i] = kk::countOf(static_cast<kk::Size>(i));
      }
      kakuroui::buildMenu(surface, model);
      break;
    }
    case Screen::HowTo: {
      kakuroui::HowToModel model;
      model.page = howToPage;
      kakuroui::buildHowTo(surface, model);
      break;
    }
    case Screen::Board: {
      const Slot& slot = slots[static_cast<int>(current)];
      kakuroui::BoardModel model;
      model.layout = &layout;
      model.progress = &slot.progress;
      model.selected = selected;
      model.notes = notes;
      model.solved = kk::solved(layout, slot.progress);
      model.number = slot.index + 1;
      model.size = current;
      kakuroui::buildBoard(surface, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Kakuro");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void KakuroActivity::saveState() {
  unsavedEdits = 0;
  HalFile file;
  if (!Storage.openFileForWrite("KAKU", kSavePath, file)) return;
  const uint8_t header[3] = {kSaveVersion, static_cast<uint8_t>(current), static_cast<uint8_t>(anyStarted ? 1 : 0)};
  file.write(header, sizeof(header));
  file.write(reinterpret_cast<const uint8_t*>(slots), sizeof(slots));
  file.flush();
}

void KakuroActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("KAKU", kSavePath, file)) return;
  uint8_t header[3] = {};
  if (file.read(header, sizeof(header)) != sizeof(header) || header[0] != kSaveVersion) return;
  auto loaded = makeUniqueNoThrow<Slot[]>(kk::kSizeCount);
  if (!loaded) return;
  if (file.read(reinterpret_cast<uint8_t*>(loaded.get()), sizeof(slots)) != sizeof(slots)) return;
  for (int i = 0; i < kk::kSizeCount; ++i) {
    slots[i] = loaded[i];
    if (slots[i].index >= kk::countOf(static_cast<kk::Size>(i))) slots[i] = Slot{};
  }
  current = header[1] < kk::kSizeCount ? static_cast<kk::Size>(header[1]) : kk::Size::Small;
  anyStarted = header[2] != 0 && slots[static_cast<int>(current)].started;
}

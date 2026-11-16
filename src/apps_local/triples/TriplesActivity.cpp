#include "TriplesActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxHowTo.h"
#include "../ui/ToyboxTheme.h"

namespace tr = triples;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/triples.sav";
constexpr uint8_t kSaveVersion = 1;
constexpr uint32_t kNewArmMs = 3000;
// A swipe that starts this close to the left edge is the way out, not a move.
constexpr int kBackSwipeEdge = 28;

struct SaveState {
  uint16_t cells[tr::kCells];
  uint16_t nextTile;
  uint8_t stack[12];
  uint8_t stackLeft;
  uint8_t over;
  uint32_t rng;
  uint32_t best;
} __attribute__((packed));

constexpr const char* kHowToPages[] = {
    "SWIPE TO SLIDE EVERY TILE ONE SQUARE THAT WAY. A TILE AGAINST THE WALL, OR AGAINST ONE IT CANNOT JOIN, STAYS "
    "WHERE IT IS.",
    "A 1 AND A 2 JOIN INTO A 3. FROM THERE ONLY EQUALS JOIN: 3 AND 3 MAKE 6, 6 AND 6 MAKE 12, AND SO ON. A 1 NEVER "
    "JOINS A 1, NOR A 2 A 2.",
    "EVERY MOVE BRINGS A NEW TILE IN ON THE SIDE YOU SWIPED AWAY FROM; THE ONE COMING NEXT IS SHOWN ABOVE THE "
    "BOARD. THE GAME ENDS WHEN NOTHING CAN MOVE. BIG TILES SCORE FAR MORE THAN SMALL ONES, SO KEEP YOUR BIGGEST IN "
    "A CORNER AND FEED IT.",
};
constexpr toyboxhowto::Guide kHowTo{kHowToPages, 3};

}  // namespace

std::unique_ptr<Activity> TriplesActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TriplesActivity>(renderer, mappedInput);
}

void TriplesActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  game_.start(static_cast<uint32_t>(millis()) * 2654435761u + 7u);
  loadState();
  over_ = game_.over();
  entered_ = -1;
  requestUpdate();
}

void TriplesActivity::onExit() {
  saveState();
  Activity::onExit();
}

void TriplesActivity::play(const tr::Dir dir) {
  if (over_) return;
  int entered = -1;
  if (!game_.move(dir, &entered)) return;
  entered_ = entered;
  newArmedMs_ = 0;
  if (game_.score() > best_) best_ = game_.score();
  if (game_.over()) {
    over_ = true;
    haptic_feedback::touchAction(true);
    saveState();
  }
  requestUpdate();
}

void TriplesActivity::loop() {
  const uint32_t now = static_cast<uint32_t>(millis());
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTouchPressed(x, y)) pressX_ = x;

  if (howTo_.open) {
    // The guide takes Back and taps; the board waits behind it.
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      howTo_.back();
      requestUpdate();
      return;
    }
    int hx = 0;
    int hy = 0;
    if (!mappedInput.wasScreenTapped(hx, hy) || !interactionsReady_) return;
    fui::InputSnapshot tap;
    tap.touchReleased = true;
    tap.touchX = static_cast<int16_t>(hx);
    tap.touchY = static_cast<int16_t>(hy);
    if (howTo_.tap(interactions_.route(tap), kHowTo)) requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // The firmware reads any rightward swipe from the left quarter as Back.
    // Here that is a move, unless it began at the very edge.
    if (mappedInput.wasBackGesture() && pressX_ >= kBackSwipeEdge && !over_) {
      pressX_ = -1;
      play(tr::Dir::Right);
      return;
    }
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }

  switch (mappedInput.wasSwipe()) {
    case MappedInputManager::SwipeDir::Left:
      play(tr::Dir::Left);
      return;
    case MappedInputManager::SwipeDir::Right:
      play(tr::Dir::Right);
      return;
    case MappedInputManager::SwipeDir::Up:
      play(tr::Dir::Up);
      return;
    case MappedInputManager::SwipeDir::Down:
      play(tr::Dir::Down);
      return;
    case MappedInputManager::SwipeDir::None:
      break;
  }
  // The two keys every board has slide the board their way.
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    play(tr::Dir::Up);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    play(tr::Dir::Down);
    return;
  }

  if (newArmedMs_ != 0 && now - newArmedMs_ >= kNewArmMs) {
    newArmedMs_ = 0;
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
  if (event.action == toyboxhowto::kOpen) {
    howTo_.start();
    requestUpdate();
    return;
  }
  if (event.action != triplesui::ActionNew) return;
  // A finished game starts over at once; one in progress asks first.
  if (over_ || (newArmedMs_ != 0 && now - newArmedMs_ < kNewArmMs)) {
    game_.start(now * 2654435761u + 11u);
    over_ = false;
    entered_ = -1;
    newArmedMs_ = 0;
    saveState();
  } else {
    newArmedMs_ = now == 0 ? 1 : now;
  }
  requestUpdate();
}

void TriplesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen surface(frame);

  triplesui::Model m;
  m.game = &game_;
  m.best = best_;
  m.entered = entered_;
  m.over = over_;
  m.newArmed = newArmedMs_ != 0;
  m.faces.ctx = &target;
  m.faces.bindTitle = [](void* ctx, const bool large) {
    static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE,
                                                       large ? toybox::kLargeFontId : toybox::kDisplayFontId);
  };
  if (howTo_.open) {
    toyboxhowto::build(surface, kHowTo, howTo_);
  } else {
    triplesui::build(surface, m);
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Triples");
  const auto labels = mappedInput.mapLabels("Back", "", "Up", "Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void TriplesActivity::saveState() {
  SaveState s{};
  for (int i = 0; i < tr::kCells; ++i) s.cells[i] = game_.cells[i];
  s.nextTile = game_.nextTile;
  for (int i = 0; i < 12; ++i) s.stack[i] = game_.stack[i];
  s.stackLeft = game_.stackLeft;
  s.over = over_ ? 1 : 0;
  s.rng = game_.rng.state;
  s.best = best_;
  HalFile file;
  if (!Storage.openFileForWrite("TRPL", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(&s), sizeof(s));
  file.flush();
}

void TriplesActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("TRPL", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  SaveState s{};
  if (file.read(reinterpret_cast<uint8_t*>(&s), sizeof(s)) != sizeof(s)) return;
  best_ = s.best;
  // A board is only taken back if every tile on it is one the game can make.
  if (s.stackLeft > 12 || s.nextTile == 0) return;
  for (int i = 0; i < tr::kCells; ++i) {
    uint16_t v = s.cells[i];
    while (v > 3 && v % 2 == 0) v = static_cast<uint16_t>(v / 2);
    if (s.cells[i] != 0 && (v < 1 || v > 3)) return;
  }
  for (int i = 0; i < 12; ++i) {
    if (s.stack[i] < 1 || s.stack[i] > 3) return;
  }
  for (int i = 0; i < tr::kCells; ++i) game_.cells[i] = s.cells[i];
  game_.nextTile = s.nextTile;
  for (int i = 0; i < 12; ++i) game_.stack[i] = s.stack[i];
  game_.stackLeft = s.stackLeft;
  game_.rng.state = s.rng;
}

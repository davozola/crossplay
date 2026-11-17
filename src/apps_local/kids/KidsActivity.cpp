#include "KidsActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Memory.h>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "MoleScreens.h"

// The Buzzer library is in the device builds' lib_deps; the browser build does
// not have it, and a board without a buzzer links stubs whose begin() is false.
#if __has_include(<Buzzer.h>)
#include <Buzzer.h>
#define KIDS_HAS_BUZZER 1
#else
#define KIDS_HAS_BUZZER 0
#endif

namespace fui = freeink::ui;

namespace {
// Long enough to see the tile turn black and hear the chirp.
constexpr uint32_t kAdvanceAfterMs = 1600;
constexpr char kMoleConfigPath[] = "/.crosspoint/talp.cfg";
constexpr uint32_t kPartyMs = 6000;
// Fast paints leave a little of the last picture behind; after this many the
// next one is a clean one.
constexpr uint8_t kCleanAfterPaints = 12;
}  // namespace

std::unique_ptr<Activity> KidsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<KidsActivity>(renderer, mappedInput);
}

namespace {
std::unique_ptr<Activity> makeOnly(GfxRenderer& renderer, MappedInputManager& mappedInput, const int number) {
  return makeUniqueNoThrow<KidsActivity>(renderer, mappedInput, static_cast<int8_t>(number));
}
constexpr int numberOf(const kids::Game game) { return static_cast<int>(game); }
constexpr int numberOf(const kidsmoreui::Which which) { return kids::kGames + static_cast<int>(which); }
}  // namespace

std::unique_ptr<Activity> KidsActivity::createCards(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kids::Game::Cards));
}
std::unique_ptr<Activity> KidsActivity::createFind(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kids::Game::Find));
}
std::unique_ptr<Activity> KidsActivity::createMatch(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kids::Game::Match));
}
std::unique_ptr<Activity> KidsActivity::createLetters(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kids::Game::Letters));
}
std::unique_ptr<Activity> KidsActivity::createCount(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kids::Game::Count));
}
std::unique_ptr<Activity> KidsActivity::createMole(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kids::Game::Mole));
}
std::unique_ptr<Activity> KidsActivity::createMemory(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kidsmoreui::Which::Memory));
}
std::unique_ptr<Activity> KidsActivity::createSums(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kidsmoreui::Which::Sums));
}
std::unique_ptr<Activity> KidsActivity::createMaze(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kidsmoreui::Which::Maze));
}
std::unique_ptr<Activity> KidsActivity::createDots(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeOnly(renderer, mappedInput, numberOf(kidsmoreui::Which::Dots));
}

void KidsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  rng_.state = static_cast<uint32_t>(millis()) * 2654435761u + 1u;
  view_ = View::Menu;
  stars_ = 0;
  // A game that could not be made falls back to the page of all of them.
  if (only_ != kMenu && !openNumber(only_)) only_ = kMenu;
  requestUpdate();
}

bool KidsActivity::openNumber(const int number) {
  if (number >= kids::kGames && number < kids::kGames + kidsmoreui::kGames) {
    mini_ = kidsmoreui::make(static_cast<kidsmoreui::Which>(number - kids::kGames));
    if (!mini_) {
      LOG_ERR("ILYA", "OOM: game %d", number);
      return false;
    }
    mini_->open(rng_, mappedInput.hasTouch());
    view_ = View::Mini;
    cleanNext_ = true;
    return true;
  }
  if (number < 0 || number >= kids::kGames) return false;
  open(static_cast<kids::Game>(number));
  return true;
}

void KidsActivity::open(const kids::Game game) {
  game_ = game;
  stars_ = 0;
  advanceAtMs_ = 0;
  if (game == kids::Game::Mole) {
    loadMoleConfig();
    view_ = View::MoleSetup;
    cleanNext_ = true;
  } else if (game == kids::Game::Cards || game == kids::Game::Letters) {
    card_ = 0;
    view_ = View::Card;
  } else {
    lastTarget_ = -1;
    lastCount_ = 0;
    deal();
    view_ = View::Round;
  }
}

void KidsActivity::deal() {
  advanceAtMs_ = 0;
  if (game_ == kids::Game::Count) {
    round_ = kids::dealCount(rng_, lastCount_);
    lastCount_ = round_.count;
  } else {
    round_ = kids::dealPictures(rng_, kids::kMaxOptions, lastTarget_);
    lastTarget_ = round_.target;
  }
}

void KidsActivity::did(const kidsmoreui::Did what) {
  using kidsmoreui::Did;
  switch (what) {
    case Did::Nothing:
      break;
    case Did::Nudge:
      haptic_feedback::touchAction();
      break;
    case Did::Clean:
      cleanNext_ = true;
      requestUpdate();
      break;
    case Did::Cheer:
      requestUpdateAndWait();
      cheer();
      break;
    case Did::Paint:
      requestUpdate();
      break;
  }
}

void KidsActivity::cheer() {
  haptic_feedback::touchAction(true);
#if KIDS_HAS_BUZZER
  Buzzer buzzer;
  if (buzzer.begin()) {
    // Two rising notes, a quarter of a second in all.
    buzzer.tone(1568, 90);
    delay(30);
    buzzer.tone(2093, 130);
    buzzer.end();
  }
#endif
}

void KidsActivity::loadMoleConfig() {
  moleConfig_ = mole::Config{};
  if (!Storage.exists(kMoleConfigPath)) return;
  HalFile file;
  uint8_t bytes[mole::kConfigBytes];
  if (!Storage.openFileForRead("ILYA", kMoleConfigPath, file)) return;
  const int got = file.read(bytes, sizeof(bytes));
  mole::unpack(bytes, got, moleConfig_);
}

void KidsActivity::saveMoleConfig() {
  HalFile file;
  if (!Storage.openFileForWrite("ILYA", kMoleConfigPath, file)) return;
  uint8_t bytes[mole::kConfigBytes];
  mole::pack(moleConfig_, bytes);
  file.write(bytes, sizeof(bytes));
  file.flush();
}

void KidsActivity::bopSound() {
  haptic_feedback::touchAction(true);
  if (!moleConfig_.sound) return;
#if KIDS_HAS_BUZZER
  Buzzer buzzer;
  if (buzzer.begin()) {
    // A bonk and the spring after it.
    buzzer.tone(330, 60);
    delay(20);
    buzzer.tone(988, 50);
    buzzer.tone(1319, 70);
    buzzer.end();
  }
#endif
}

void KidsActivity::partySound() {
  if (!moleConfig_.sound) return;
#if KIDS_HAS_BUZZER
  Buzzer buzzer;
  if (buzzer.begin()) {
    constexpr int kNotes[] = {1047, 1319, 1568, 2093};
    for (const int note : kNotes) {
      buzzer.tone(note, 110);
      delay(25);
    }
    buzzer.tone(2093, 220);
    buzzer.end();
  }
#endif
}

void KidsActivity::loopMole() {
  const uint32_t now = static_cast<uint32_t>(millis());
  int tapX = 0;
  int tapY = 0;
  const bool tapped = mappedInput.wasScreenTapped(tapX, tapY);

  if (partyUntilMs_ != 0) {
    // The celebration goes when it is touched or has been admired enough.
    if (tapped || now - partyUntilMs_ < 0x80000000u) {
      partyUntilMs_ = 0;
      cleanNext_ = true;
      // Nobody was counting while the page was up.
      for (mole::Hole& hole : mole_.holes) hole.since = now;
      requestUpdate();
    }
    return;
  }

  if (tapped) {
    const moleui::Meadow meadow =
        moleui::meadowFor(mole_.config, renderer.getScreenWidth(), renderer.getScreenHeight());
    if (mole::tap(mole_, moleui::holeAt(meadow, tapX, tapY), now)) {
      // The stars first, then the noise: the panel is the slow one.
      requestUpdateAndWait();
      bopSound();
    }
    return;
  }

  if (mole::tick(mole_, now)) {
    if (mole_.party) {
      mole_.party = false;
      partyUntilMs_ = now + kPartyMs;
      if (partyUntilMs_ == 0) partyUntilMs_ = 1;
      cleanNext_ = true;
      requestUpdateAndWait();
      partySound();
      return;
    }
    requestUpdate();
  }
}

void KidsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::Menu) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (view_ == View::MolePlay) {
      // Back to the page with the choices, not all the way out.
      view_ = View::MoleSetup;
      partyUntilMs_ = 0;
      cleanNext_ = true;
      requestUpdate();
      return;
    }
    if (view_ == View::Mini && mini_ && mini_->back()) {
      cleanNext_ = true;
      requestUpdate();
      return;
    }
    // Opened by itself from the shelf, a game has no page of games behind it.
    if (only_ != kMenu) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    mini_.reset();
    view_ = View::Menu;
    advanceAtMs_ = 0;
    cleanNext_ = true;
    requestUpdate();
    return;
  }

  if (view_ == View::Mini && mini_) {
    const uint32_t now = static_cast<uint32_t>(millis());
    if (mini_->playing()) {
      int x = 0;
      int y = 0;
      if (mappedInput.wasScreenTapped(x, y)) {
        did(mini_->tap(renderer, x, y, now));
      } else {
        did(mini_->tick(now));
      }
      return;
    }
  }

  if (view_ == View::Round && advanceAtMs_ != 0 && static_cast<uint32_t>(millis()) - advanceAtMs_ < 0x80000000u) {
    deal();
    requestUpdate();
    return;
  }

  if (view_ == View::MolePlay) {
    loopMole();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  // A tap while the answer is being shown moves on at once.
  if (view_ == View::Round && round_.solved) {
    deal();
    requestUpdate();
    return;
  }
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  route(interactions_.route(input));
}

void KidsActivity::route(const fui::ActionEvent& event) {
  namespace ui = kidsui;
  const int deck = game_ == kids::Game::Letters ? kids::letterCount() : kids::kPictures;
  switch (event.action) {
    case ui::ActionGame:
      if (!openNumber(event.value)) return;
      break;
    case moleui::ActionHoles:
      moleConfig_.holes = static_cast<uint8_t>((moleConfig_.holes + 1) % mole::kHoleChoices);
      saveMoleConfig();
      break;
    case moleui::ActionWho:
      moleConfig_.who = static_cast<uint8_t>((moleConfig_.who + 1) % mole::kWhoChoices);
      saveMoleConfig();
      break;
    case moleui::ActionHides:
      moleConfig_.hides = static_cast<uint8_t>((moleConfig_.hides + 1) % mole::kHideChoices);
      saveMoleConfig();
      break;
    case moleui::ActionSound:
      moleConfig_.sound = moleConfig_.sound ? 0 : 1;
      saveMoleConfig();
      break;
    case moleui::ActionPlay:
      mole::start(mole_, moleConfig_, rng_.next() | 1u, static_cast<uint32_t>(millis()));
      partyUntilMs_ = 0;
      cleanNext_ = true;
      view_ = View::MolePlay;
      break;
    case ui::ActionNext:
      card_ = kids::step(card_, 1, deck);
      break;
    case ui::ActionPrevious:
      card_ = kids::step(card_, -1, deck);
      break;
    case ui::ActionOption:
      if (view_ != View::Round) return;
      if (round_.choose(event.value)) {
        ++stars_;
        advanceAtMs_ = static_cast<uint32_t>(millis()) + kAdvanceAfterMs;
        if (advanceAtMs_ == 0) advanceAtMs_ = 1;
        requestUpdateAndWait();
        cheer();
        return;
      }
      haptic_feedback::touchAction();
      break;
    default:
      if (view_ == View::Mini && mini_) did(mini_->action(event.action));
      return;
  }
  requestUpdate();
}

void KidsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen surface(frame);

  kidsui::FaceSwap faces;
  faces.ctx = &target;
  faces.bindTitle = [](void* ctx, const kidsui::Face face) {
    int id = toybox::kDisplayFontId;
    if (face == kidsui::Face::Large) id = toybox::kLargeFontId;
    if (face == kidsui::Face::Huge) id = toybox::kHugeFontId;
    static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE, id);
  };

  switch (view_) {
    case View::Menu: {
      kidsui::MenuModel m;
      m.renderer = &renderer;
      m.touch = mappedInput.hasTouch();
      kidsui::buildMenu(surface, m);
      break;
    }
    case View::Mini:
      if (mini_) mini_->draw(surface, renderer);
      break;
    case View::MoleSetup:
      moleui::buildSetup(surface, renderer, moleConfig_);
      break;
    case View::MolePlay:
      if (partyUntilMs_ != 0) {
        moleui::drawParty(surface, renderer, mole_);
      } else {
        moleui::drawMeadow(surface, renderer, mole_);
      }
      break;
    case View::Card: {
      kidsui::CardModel m;
      const bool letters = game_ == kids::Game::Letters;
      m.index = card_;
      m.total = letters ? kids::letterCount() : kids::kPictures;
      m.picture = letters ? kids::pictureOfLetter(card_) : card_;
      m.letter = letters ? kids::letterAt(card_) : 0;
      m.faces = faces;
      kidsui::buildCard(surface, m);
      break;
    }
    case View::Round: {
      kidsui::RoundModel m;
      m.game = game_;
      m.round = &round_;
      m.stars = stars_;
      m.faces = faces;
      kidsui::buildRound(surface, m);
      break;
    }
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Ilya");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if ((view_ == View::MolePlay || view_ == View::Mini) && !cleanNext_ && ++fastPaints_ >= kCleanAfterPaints) {
    cleanNext_ = true;
  }
  if (cleanNext_) fastPaints_ = 0;
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}

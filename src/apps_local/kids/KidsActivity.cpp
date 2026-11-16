#include "KidsActivity.h"

#include <Memory.h>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"

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
}  // namespace

std::unique_ptr<Activity> KidsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<KidsActivity>(renderer, mappedInput);
}

void KidsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  rng_.state = static_cast<uint32_t>(millis()) * 2654435761u + 1u;
  view_ = View::Menu;
  stars_ = 0;
  requestUpdate();
}

void KidsActivity::open(const kids::Game game) {
  game_ = game;
  stars_ = 0;
  advanceAtMs_ = 0;
  if (game == kids::Game::Cards || game == kids::Game::Letters) {
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

void KidsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::Menu) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    view_ = View::Menu;
    advanceAtMs_ = 0;
    requestUpdate();
    return;
  }

  if (view_ == View::Round && advanceAtMs_ != 0 && static_cast<uint32_t>(millis()) - advanceAtMs_ < 0x80000000u) {
    deal();
    requestUpdate();
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
      if (event.value < 0 || event.value >= kids::kGames) return;
      open(static_cast<kids::Game>(event.value));
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
    case View::Menu:
      kidsui::buildMenu(surface);
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
  renderer.displayBuffer();
}

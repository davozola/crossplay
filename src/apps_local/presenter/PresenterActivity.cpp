#include "PresenterActivity.h"

#include <BatteryMonitor.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Memory.h>

#include <cstdio>

#include "../../HapticFeedback.h"
#include "../../SilentRestart.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace radio = presenter::link;

namespace {

enum : fui::ActionId {
  ActionOption = 1,  // + the option's index
  ActionStart = 20,
  ActionForget,
  ActionNext,
  ActionPrevious,
  ActionBlank,
  ActionClock,
  ActionReset,
  ActionOptions,
};

enum { OptLength, OptShow, OptWarn, OptKeys };
struct Option {
  const char* name;
  const char* const* values;
  uint8_t count;
};
constexpr const char* kLengths[] = {"NO LIMIT",   "5 MINUTES",  "10 MINUTES", "15 MINUTES", "20 MINUTES",
                                    "30 MINUTES", "45 MINUTES", "60 MINUTES", "90 MINUTES", "120 MINUTES"};
constexpr const char* kShows[] = {"TIME LEFT", "TIME USED"};
constexpr const char* kWarns[] = {"AT 5 AND AT 2 MINUTES", "AT 2 MINUTES", "NONE"};
constexpr const char* kKeys[] = {"ARROW KEYS", "PAGE UP AND PAGE DOWN"};
constexpr Option kOptionTable[] = {
    {"THE TALK LASTS", kLengths, presenter::kLimitCount},
    {"THE CLOCK SHOWS", kShows, 2},
    {"WARNINGS", kWarns, static_cast<uint8_t>(presenter::Warn::Count)},
    {"SLIDES TURN WITH", kKeys, static_cast<uint8_t>(presenter::KeySet::Count)},
};
static_assert(sizeof(kLengths) / sizeof(kLengths[0]) == presenter::kLimitCount);

constexpr char kOptionsPath[] = "/.crosspoint/presenter.cfg";
constexpr char kDeviceName[] = "Sticky Presenter";
constexpr uint8_t kCleanAfterPaints = 10;

uint8_t batteryPercent() {
#if defined(SIMULATOR)
  return 100;
#else
  static const BatteryMonitor battery;
  uint16_t percent = 0;
  if (!battery.readPercentageChecked(percent) || percent > 100) return 100;
  return static_cast<uint8_t>(percent);
#endif
}

const char* linkLine(const radio::State state) {
  switch (state) {
    case radio::State::Linked:
      return "LINKED TO THE COMPUTER";
    case radio::State::Waiting:
      return "WAITING FOR THE COMPUTER";
    case radio::State::Failed:
      return "BLUETOOTH DID NOT START";
    case radio::State::Off:
      break;
  }
  return "BLUETOOTH IS OFF";
}

}  // namespace

std::unique_ptr<Activity> PresenterActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<PresenterActivity>(renderer, mappedInput);
}

void PresenterActivity::loadOptions() {
  uint8_t bytes[kOptions] = {};
  int got = 0;
  HalFile file;
  if (Storage.exists(kOptionsPath) && Storage.openFileForRead("PRS", kOptionsPath, file)) {
    got = file.read(bytes, sizeof(bytes));
  }
  constexpr uint8_t kDefaults[kOptions] = {4, 0, 0, 0};  // twenty minutes, counting down
  for (int i = 0; i < kOptions; ++i) {
    options_[i] = i < got && bytes[i] < kOptionTable[i].count ? bytes[i] : kDefaults[i];
  }
}

void PresenterActivity::saveOptions() {
  HalFile file;
  if (!Storage.openFileForWrite("PRS", kOptionsPath, file)) return;
  file.write(options_, sizeof(options_));
  file.flush();
}

void PresenterActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadOptions();
  view_ = View::Setup;
  clock_.reset();
  radioUsed_ = radio::begin(kDeviceName, batteryPercent());
  linkShown_ = radio::state();
  shown_ = face(static_cast<uint32_t>(millis()));
  requestUpdate();
}

void PresenterActivity::onExit() {
  Activity::onExit();
  radio::end();
  // The radio's memory only comes back whole with a restart, as after Wi-Fi.
  if (radioUsed_ && radio::real()) silentRestart();
}

int PresenterActivity::pageBottom() const {
  return renderer.getScreenHeight() - (mappedInput.hasTouch() ? 8 : lab::kBottomReserve + 6);
}

presenter::Face PresenterActivity::face(const uint32_t now) const {
  return presenter::faceFor(clock_.elapsedMs(now), presenter::kLimits[options_[OptLength]], options_[OptShow] == 0,
                            static_cast<presenter::Warn>(options_[OptWarn]));
}

void PresenterActivity::press(const presenter::Key key) {
  const bool sent = radio::tap(presenter::usageFor(key, static_cast<presenter::KeySet>(options_[OptKeys])));
  if (!sent) {
    haptic_feedback::touchAction(true);
    return;
  }
  haptic_feedback::touchAction();
  // The clock starts itself with the first slide, so there is nothing to remember.
  if (key == presenter::Key::Next && clock_.untouched()) {
    clock_.start(static_cast<uint32_t>(millis()));
    requestUpdate();
  }
}

void PresenterActivity::loop() {
  using Button = MappedInputManager::Button;
  const uint32_t now = static_cast<uint32_t>(millis());

  if (mappedInput.wasReleased(Button::Back)) {
    if (view_ == View::Session) {
      view_ = View::Setup;
      clean_ = true;
      requestUpdate();
      return;
    }
    shelf::leave(renderer, mappedInput);
    return;
  }

  // Whatever changed without a touch: the minute, the warning, the link.
  const presenter::Face current = face(now);
  const radio::State linkNow = radio::state();
  if (current != shown_ || linkNow != linkShown_) {
    if (current.phase != shown_.phase) {
      clean_ = true;
      if (view_ == View::Session && current.phase != presenter::Phase::Plenty) haptic_feedback::touchAction(true);
    }
    shown_ = current;
    linkShown_ = linkNow;
    requestUpdate();
  }

  if (view_ == View::Session) {
    if (mappedInput.wasReleased(Button::Right) || mappedInput.wasReleased(Button::Down) ||
        mappedInput.wasReleased(Button::Confirm)) {
      press(presenter::Key::Next);
      return;
    }
    if (mappedInput.wasReleased(Button::Left) || mappedInput.wasReleased(Button::Up)) {
      press(presenter::Key::Previous);
      return;
    }
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);

  if (view_ == View::Session) {
    switch (event.action) {
      case ActionNext:
        press(presenter::Key::Next);
        return;
      case ActionPrevious:
        press(presenter::Key::Previous);
        return;
      case ActionBlank:
        press(presenter::Key::Blank);
        return;
      case ActionClock:
        if (clock_.running) {
          clock_.pause(now);
        } else {
          clock_.start(now);
        }
        break;
      case ActionReset:
        if (clock_.running) return;
        clock_.reset();
        clean_ = true;
        break;
      case ActionOptions:
        view_ = View::Setup;
        clean_ = true;
        break;
      default:
        return;
    }
    shown_ = face(now);
    requestUpdate();
    return;
  }

  if (event.action == ActionStart) {
    view_ = View::Session;
    clean_ = true;
  } else if (event.action == ActionForget) {
    radio::forget();
    note_ = "FORGOTTEN. REMOVE IT ON THE COMPUTER TOO, THEN PAIR AGAIN.";
  } else if (event.action >= ActionOption && event.action < ActionOption + kOptions) {
    const int i = event.action - ActionOption;
    options_[i] = static_cast<uint8_t>((options_[i] + 1) % kOptionTable[i].count);
    saveOptions();
    shown_ = face(now);
  } else {
    return;
  }
  requestUpdate();
}

void PresenterActivity::buildSetup(toybox::Screen& screen) {
  lab::chrome(screen, "PRESENTER");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  int y = toybox::kChromeHeight + 10;

  const radio::State state = radio::state();
  lab::value(screen, x, y, inner, linkLine(state));
  y += toybox::kUiCut.inkHeight + 12;
  const char* help = "";
  if (note_[0] != 0) {
    help = note_;
  } else if (state == radio::State::Waiting) {
    help = "ON THE COMPUTER, OPEN THE BLUETOOTH SETTINGS AND CHOOSE \"STICKY PRESENTER\". ONLY NEEDED THE FIRST TIME.";
  } else if (state == radio::State::Linked) {
    help = radio::real() ? "IT TYPES LIKE A KEYBOARD: CLICK ON THE SLIDES FIRST SO THEY TAKE THE KEYS."
                         : "THIS BUILD HAS NO BLUETOOTH: THE LINK SHOWN IS PRETEND.";
  } else {
    help = "LEAVE AND OPEN PRESENTER AGAIN. IF IT KEEPS HAPPENING, RESTART THE DEVICE.";
  }
  lab::paragraph(screen, x, y, inner, help, 3);
  y += 3 * toybox::kTileCut.lineHeight + 14;

  const int rowH = 70;
  for (int i = 0; i < kOptions; ++i) {
    lab::button(screen, lab::rect(x, y, inner, rowH), "", static_cast<fui::ActionId>(ActionOption + i), 0, false,
                false);
    lab::caption(screen, x + 18, y + 11, inner - 36, kOptionTable[i].name);
    lab::value(screen, x + 18, y + 11 + toybox::kTileCut.inkHeight + 9, inner - 36,
               kOptionTable[i].values[options_[i]]);
    y += rowH + 8;
  }

  const int bottom = pageBottom();
  lab::button(screen, lab::rect(x, bottom - 56, inner, 56), "FORGET PAIRED COMPUTERS", ActionForget, 0, false, true);
  lab::button(screen, lab::rect(x, bottom - 56 - 10 - 96, inner, 96), clock_.untouched() ? "START" : "BACK TO THE TALK",
              ActionStart, 0, true, false);
}

void PresenterActivity::buildSession(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - 2 * toybox::kMargin;
  const presenter::Face f = shown_;
  const radio::State state = radio::state();

  // The link, small when it is fine and impossible to miss when it is not.
  const bool linked = state == radio::State::Linked;
  if (linked) {
    lab::caption(screen, x, 10, inner, linkLine(state), fui::TextAlign::Center);
  } else {
    renderer.fillRect(0, 0, width, 36, true);
    lab::caption(screen, x, 10, inner, linkLine(state), fui::TextAlign::Center, fui::Color::White);
  }

  // The clock. A frame at five minutes, solid black from two.
  const int panelY = 44;
  const int panelH = 206;
  const bool solid = f.phase == presenter::Phase::Two || f.phase == presenter::Phase::Over;
  if (solid) {
    renderer.fillRoundedRect(x, panelY, inner, panelH, 14, Color::Black);
  } else {
    const int weight = f.phase == presenter::Phase::Five ? 8 : 2;
    renderer.drawRoundedRect(x, panelY, inner, panelH, weight, 14, true);
  }
  // An invisible button over the panel: a tap starts or pauses the clock.
  {
    fui::ButtonProps zone;
    zone.label = "";
    zone.action = ActionClock;
    zone.styles.explicitlySet = true;
    zone.minTouchSize = 0;
    screen.button(zone, lab::rect(x, panelY, inner, panelH));
  }
  char figure[toybox::kIntChars + 2];
  std::snprintf(figure, sizeof(figure), "%s%d", f.phase == presenter::Phase::Over ? "+" : "", f.minutes % 1000);
  const int figureW = renderer.getTextWidth(toybox::kHugeFontId, figure);
  renderer.drawText(toybox::kHugeFontId, x + (inner - figureW) / 2, panelY + 8, figure, !solid);
  const fui::Color ink = solid ? fui::Color::White : fui::Color::Black;
  lab::value(screen, x, panelY + 132, inner, presenter::captionFor(f), fui::TextAlign::Center, ink);
  const char* running = clock_.running       ? "TAP TO PAUSE"
                        : clock_.untouched() ? "STARTS WITH YOUR FIRST SLIDE"
                                             : "PAUSED: TAP TO GO ON";
  lab::caption(screen, x, panelY + 132 + toybox::kUiCut.inkHeight + 12, inner, running, fui::TextAlign::Center, ink);

  // Small things, out of the thumb's way.
  const int rowY = panelY + panelH + 10;
  const int rowH = 56;
  const int third = (inner - 16) / 3;
  lab::button(screen, lab::rect(x, rowY, third, rowH), "BLACK", ActionBlank, 0);
  if (!clock_.running && !clock_.untouched()) {
    lab::button(screen, lab::rect(x + third + 8, rowY, third, rowH), "RESET", ActionReset, 0);
  }
  lab::button(screen, lab::rect(x + 2 * (third + 8), rowY, third, rowH), "OPTIONS", ActionOptions, 0);

  // The two that matter, the one used most the biggest and nearest the hand.
  const int bottom = pageBottom();
  const int previousY = rowY + rowH + 10;
  const int previousH = 130;
  lab::button(screen, lab::rect(x, previousY, inner, previousH), "PREVIOUS", ActionPrevious, 0, false, false);
  const int nextY = previousY + previousH + 10;
  lab::button(screen, lab::rect(x, nextY, inner, bottom - nextY), "NEXT", ActionNext, 0, true, false);
}

void PresenterActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  if (view_ == View::Setup) {
    buildSetup(screen);
  } else {
    buildSession(screen);
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Presenter");
  const auto labels = view_ == View::Session ? mappedInput.mapLabels("Options", "Next", "Previous", "Next")
                                             : mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!clean_ && ++paints_ >= kCleanAfterPaints) clean_ = true;
  if (clean_) paints_ = 0;
  renderer.displayBuffer(clean_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  clean_ = false;
}

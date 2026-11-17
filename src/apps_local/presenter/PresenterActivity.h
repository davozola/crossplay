#pragma once

// PRESENTER: the device as a Bluetooth keyboard that turns slides, with a
// session clock that says when to wrap up.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "PresenterCore.h"
#include "PresenterLink.h"

class PresenterActivity final : public Activity {
 public:
  PresenterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Presenter", renderer, mappedInput) {}
  ~PresenterActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // A talk is long stretches of nothing pressed; the screen must not go to sleep under it.
  bool preventAutoSleep() override { return view_ == View::Session || clock_.running; }

 private:
  enum class View : uint8_t { Setup, Session };
  static constexpr int kOptions = 4;

  void loadOptions();
  void saveOptions();
  void press(presenter::Key key);
  presenter::Face face(uint32_t now) const;
  void buildSetup(toybox::Screen& screen);
  void buildSession(toybox::Screen& screen);
  int pageBottom() const;

  presenter::Clock clock_;
  presenter::Face shown_;
  presenter::link::State linkShown_ = presenter::link::State::Off;
  uint8_t options_[kOptions] = {};
  View view_ = View::Setup;
  const char* note_ = "";
  uint8_t paints_ = 0;
  bool clean_ = true;
  bool radioUsed_ = false;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};

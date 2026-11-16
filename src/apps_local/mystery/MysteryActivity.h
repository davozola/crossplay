#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "MysteryCore.h"

// MISTERIOS: three detective cases worked by questioning four suspects and
// examining three places, then accusing. The prose is set in the reading serif.
class MysteryActivity final : public Activity {
 public:
  MysteryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Mystery", renderer, mappedInput) {}
  ~MysteryActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Cases, Hub, Intro, Suspect, Answer, Place, Notebook, Accuse, Verdict };

  void loadState();
  void saveState();
  View parentOf(View v) const;

  void buildCases(toybox::Screen& screen);
  void buildHub(toybox::Screen& screen);
  void buildIntro(toybox::Screen& screen);
  void buildSuspect(toybox::Screen& screen);
  void buildAnswer(toybox::Screen& screen);
  void buildPlace(toybox::Screen& screen);
  void buildNotebook(toybox::Screen& screen);
  void buildAccuse(toybox::Screen& screen);
  void buildVerdict(toybox::Screen& screen);

  View view_ = View::Cases;
  int case_ = 0;
  int suspect_ = 0;
  int question_ = 0;
  int place_ = 0;
  int newClue_ = -1;  // the clue the last question or place turned up
  mystery::Verdict verdict_ = mystery::Verdict::WrongSuspect;
  mystery::Progress progress_[mystery::kCases];

  // The frame's target while a frame is being built, so a builder can rebind a font slot.
  void* target_ = nullptr;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};

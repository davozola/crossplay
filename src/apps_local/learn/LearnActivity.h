#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "LearnCore.h"

// APRENDE: five minutes a day on one idea from maths, economics, electronics
// or AI. A lesson is read in a couple of pages and closed with a question.
class LearnActivity final : public Activity {
 public:
  LearnActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Learn", renderer, mappedInput) {}
  ~LearnActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Home, Subject, Read, Question };

  void load();
  void save();
  void open(int lesson);
  void turn(int by);
  void route(const freeink::ui::ActionEvent& event);
  int linesPerPage() const;
  int pageCount() const;
  int32_t today() const;
  void buildHome(toybox::Screen& screen);
  void buildSubject(toybox::Screen& screen);
  void buildRead(toybox::Screen& screen);
  void buildQuestion(toybox::Screen& screen);

  View view_ = View::Home;
  learn::Progress progress_;
  learn::Subject subject_ = learn::Subject::Maths;
  int lesson_ = 0;
  std::vector<std::string> lines_;  // the lesson, wrapped; a leading tab marks a heading
  int page_ = 0;
  int8_t chosen_ = -1;  // the option tapped, once one has been

  int pressX_ = -1;
  uint8_t fastPaints_ = 0;
  bool cleanNext_ = true;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};

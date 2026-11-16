#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "../weather/WeatherCore.h"
#include "NavalCore.h"

// NAVAL WEATHER: wind, waves, swell and water for the places WEATHER keeps,
// a week ahead, fetched from Open-Meteo on request and kept on the card so it
// still reads at sea with no network.
class NavalActivity final : public Activity {
 public:
  NavalActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Naval", renderer, mappedInput) {}
  ~NavalActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Now, Day, Status };
  enum class Step : uint8_t { Idle, Joining, Ready, Running };

  void loadPlaces();
  bool loadCache();
  void cachePath(const char* which, char* out, size_t cap) const;
  bool readFile(const char* path, std::unique_ptr<char[]>& body, size_t& size) const;
  void startFetch();
  void runFetch();
  void showStatus(const char* headline, const char* detail, bool dismissable);
  void buildNow(toybox::Screen& screen);
  void buildDay(toybox::Screen& screen);
  void buildStatus(toybox::Screen& screen);

  View view_ = View::Now;
  Step step_ = Step::Idle;
  bool wifiActivated_ = false;

  weather::Place places_[weather::kMaxPlaces];
  int placeCount_ = 0;
  int selected_ = 0;

  // ~8 KB: on the heap, allocated once on entry.
  std::unique_ptr<naval::Sea> sea_;
  bool haveSea_ = false;
  int dayOpen_ = 0;

  const char* statusHeadline_ = "";
  const char* statusDetail_ = "";
  bool statusDismissable_ = false;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};

#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "WeatherCore.h"
#include "WeatherScreens.h"

// WEATHER: up to six saved cities, each forecast fetched from Open-Meteo on
// request and kept on the card, so the last one still reads with no network.
class WeatherActivity final : public Activity {
 public:
  WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Weather", renderer, mappedInput) {}
  ~WeatherActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Forecast, Cities, Results, Status };
  // A network job: joined Wi-Fi -> Ready -> the status frame is painted -> Run.
  enum class Job : uint8_t { None, Forecast, Search };
  enum class Step : uint8_t { Idle, Joining, Ready, Running };

  void loadState();
  void saveState();
  bool loadCache();
  bool loadCacheFor(const weather::Place& place, weather::Forecast& out) const;
  void openCities();
  bool saveCache(const char* body, size_t len);
  void cachePath(const weather::Place& place, char* out, size_t cap) const;

  void startJob(Job job);
  void runJob();
  void askCity();
  void showStatus(const char* headline, const char* detail, bool dismissable, View back);
  void route(const freeink::ui::ActionEvent& event);

  View view_ = View::Forecast;
  View statusBack_ = View::Forecast;
  Job job_ = Job::None;
  Step step_ = Step::Idle;
  bool wifiActivated_ = false;

  weather::Place places_[weather::kMaxPlaces];
  int placeCount_ = 0;
  int selected_ = 0;
  bool fahrenheit_ = false;

  // ~1.4 KB: on the heap with the activity, not the stack.
  weather::Forecast forecast_;
  bool haveForecast_ = false;
  weatherui::CityGlance glance_[weather::kMaxPlaces];

  char query_[48] = "";
  weather::Place results_[weather::kMaxResults];
  int resultCount_ = 0;

  const char* statusHeadline_ = "";
  const char* statusDetail_ = "";
  bool statusDismissable_ = false;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};

#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/AutoRefresh.h"
#include "../ui/ToyboxScreen.h"
#include "WeatherCore.h"
#include "WeatherScreens.h"

// WEATHER: up to six saved cities, each forecast fetched from Open-Meteo on
// request and kept on the card, so the last one still reads with no network.
class MeteoActivity final : public Activity {
 public:
  MeteoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Weather", renderer, mappedInput) {}
  ~MeteoActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // With auto-refresh on, the page stays on the panel and fetches again by
  // itself, with the radio up only for the fetch and a nap in between.
  bool preventAutoSleep() override { return autoMinutes_ > 0; }
  void requestUpdate(bool immediate = false) override;

 private:
  enum class View : uint8_t { Forecast, Day, Cities, Results, Status, AutoPicker, Board };
  // A network job: joined Wi-Fi -> Ready -> the status frame is painted -> Run.
  enum class Job : uint8_t { None, Forecast, Search, Board };
  enum class Step : uint8_t { Idle, Joining, Ready, Running };

  void loadState();
  void saveState();
  bool loadCache();
  bool loadCacheFor(const meteo::Place& place, meteo::Forecast& out) const;
  void openCities();
  bool saveCache(const char* body, size_t len);
  bool saveCacheFor(const meteo::Place& place, const char* body, size_t len);
  void cachePath(const meteo::Place& place, char* out, size_t cap) const;

  void startJob(Job job);
  void runJob();
  // The selected city's forecast, fetched with no status page. Needs Wi-Fi up.
  bool fetchQuietly(int index);
  // How many cities the page shows: 1, or the board's 3 or 5.
  int boardSize() const { return layout_ == 1 ? 3 : 5; }
  void loadBoard();
  void setLayout(int layout);
  void loadAuto();
  void saveAuto();
  void askCity();
  void showStatus(const char* headline, const char* detail, bool dismissable, View back);
  void route(const freeink::ui::ActionEvent& event);

  View view_ = View::Forecast;
  View statusBack_ = View::Forecast;
  Job job_ = Job::None;
  Step step_ = Step::Idle;
  bool wifiActivated_ = false;
  // A fetch asked for while a forecast is already on the page: it runs behind
  // the page, with a tag in the corner, instead of taking the screen.
  bool quietDue_ = false;
  bool quietPainted_ = false;
  int autoMinutes_ = 0;
  // 0: one city full screen. 1: three on a board. 2: five. A swipe changes it.
  int layout_ = 0;
  // On its side: a swipe up or down turns the forecast and the board over.
  bool wide_ = false;
  int wideLayout_ = 0;  // which of the three pages on its side
  bool onItsSide() const;
  int pressX_ = -1;
  meteoui::BoardCard cards_[meteo::kMaxPlaces];
  uint32_t lastFetchMs_ = 0;
  autorefresh::Quiet quiet_;

  meteo::Place places_[meteo::kMaxPlaces];
  int placeCount_ = 0;
  int selected_ = 0;
  bool fahrenheit_ = false;

  // ~2 KB: on the heap with the activity, not the stack.
  meteo::Forecast forecast_;
  int dayOpen_ = 0;
  bool haveForecast_ = false;
  meteoui::CityGlance glance_[meteo::kMaxPlaces];

  char query_[48] = "";
  meteo::Place results_[meteo::kMaxResults];
  int resultCount_ = 0;

  const char* statusHeadline_ = "";
  const char* statusDetail_ = "";
  bool statusDismissable_ = false;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};

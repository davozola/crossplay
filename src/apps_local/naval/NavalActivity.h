#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/AutoRefresh.h"
#include "../ui/ToyboxScreen.h"
#include "../weather/WeatherCore.h"
#include "NavalCore.h"

// NAVAL WEATHER: wind, waves, swell and water for the places WEATHER keeps,
// a week ahead, fetched from Open-Meteo on request and kept on the card so it
// still reads at sea with no network. The wind can be meteoblue's instead, for
// an owner who has a key of theirs: the key is typed once or left in a file on
// the card, stays on the card, and is never in the firmware or a log. On auto it stays on the panel and
// fetches again by itself, with the radio up only for the fetch and a nap in
// between.
class NavalActivity final : public Activity {
 public:
  NavalActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Naval", renderer, mappedInput) {}
  ~NavalActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return autoMinutes_ > 0; }
  void requestUpdate(bool immediate = false) override;

 private:
  enum class View : uint8_t { Now, Day, Status, AutoPicker, Source };
  enum class Fetched : uint8_t { Ok, NoAnswer, NoSea };
  enum class Step : uint8_t { Idle, Joining, Ready, Running };

  void loadPlaces();
  bool loadCache();
  void cachePath(const char* which, char* out, size_t cap) const;
  bool readFile(const char* path, std::unique_ptr<char[]>& body, size_t& size) const;
  void startFetch();
  void runFetch();
  // The selected place's sea and wind, kept on the card. Needs the radio up.
  // `asked` is a fetch the owner tapped for; a round on auto spends meteoblue's
  // credits no more than once every few hours.
  Fetched fetchSea(bool asked);
  void loadSource();
  void saveSource();
  void saveKey();
  void askKey();
  void buildSource(toybox::Screen& screen);
  void buildWide(toybox::Screen& screen);
  void buildWideDay(toybox::Screen& screen);
  void drawRose(toybox::Screen& screen, int cx, int cy, int r, const naval::Hour& h);
  void drawWindow(toybox::Screen& screen, int x, int y, int w, int h);
  void drawBars(toybox::Screen& screen, const freeink::ui::Rect& r, int d, float top, bool fine);
  void wideStrip(toybox::Screen& screen, int y, int h);
  void wideFact(toybox::Screen& screen, int x, int y, int w, const char* name, const char* value, const char* detail);
  void wideFacts(toybox::Screen& screen, int x, int y, int w, int pitch, const naval::Hour& h);
  void wideWind(toybox::Screen& screen, int x, int y, int w, const naval::Hour& h, int underX, int underY, int underW);
  void dayHit(toybox::Screen& screen, const freeink::ui::Rect& r, int d);
  void autoLabel(char* out, size_t cap) const;
  int windowOf(int d, int& first) const;
  naval::DayStats statsFor(int d) const;
  float scaleOf(int d) const;
  bool onItsSide() const;
  void loadView();
  void saveView();
  void loadAuto();
  void saveAuto();
  void showStatus(const char* headline, const char* detail, bool dismissable);
  void buildNow(toybox::Screen& screen);
  void buildDay(toybox::Screen& screen);
  void buildStatus(toybox::Screen& screen);

  View view_ = View::Now;
  Step step_ = Step::Idle;
  bool wifiActivated_ = false;
  int autoMinutes_ = 0;
  uint32_t lastFetchMs_ = 0;
  autorefresh::Quiet quiet_;

  weather::Place places_[weather::kMaxPlaces];
  int placeCount_ = 0;
  int selected_ = 0;

  // ~8 KB: on the heap, allocated once on entry.
  std::unique_ptr<naval::Sea> sea_;
  bool haveSea_ = false;
  int dayOpen_ = 0;
  // The hours looked at (08 to 21, or all of them) and which way up the week is.
  bool daylight_ = true;
  bool wide_ = false;
  int wideLayout_ = 1;  // 0 a table, 1 seven columns, 2 today by the hour
  int tallLayout_ = 0;  // upright: 0 now and a table, 1 the week in big rows, 2 today by the hour

  // The wind's source, and the owner's key for it.
  bool useMeteoblue_ = false;
  char key_[40] = "";
  bool meteoblueFailed_ = false;
  int meteoblueStatus_ = 0;
  uint32_t meteoblueAtMs_ = 0;  // 0: not yet this session
  int meteobluePlace_ = -1;

  const char* statusHeadline_ = "";
  const char* statusDetail_ = "";
  bool statusDismissable_ = false;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};

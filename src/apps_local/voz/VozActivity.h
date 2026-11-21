#pragma once

// VOZ: notes by voice. Tap, speak for up to a minute, tap again; the take goes
// to Gemini or OpenAI with the owner's own key and comes back as a note with a
// kind (idea, task or note), a title, a summary and the words. The audio and
// the note stay on the card in /voz.

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "VozCore.h"

class VozActivity final : public Activity {
 public:
  VozActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Voz", renderer, mappedInput) {}
  ~VozActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return view_ == View::Recording || view_ == View::Working; }

 private:
  enum class View : uint8_t { List, Recording, Working, Detail, Settings, Message };
  enum class Step : uint8_t { Idle, Ready, Running };

  struct Entry {
    char stamp[16] = "";
    bool done = false;  // a note came back
    int failure = 1;    // the last attempt's status when not done; 1 for never tried
    voz::Kind kind = voz::Kind::Note;
    std::string title;
    int seconds = 0;
  };

  void loadSettings();
  void saveSettings();
  void loadKey(voz::Service service);
  void saveKey(voz::Service service);
  void scan();
  void open(int index);
  void beginRecording();
  void endRecording();
  void send(int index);
  void runSend();
  void askKey(voz::Service service);
  void erase(int index);
  void show(View view);
  void message(const char* headline, const char* detail);

  void loopList();
  void loopDetail();
  void loopSettings();

  void drawList();
  void drawRecording();
  void drawWorking();
  void drawDetail();
  void drawSettings();
  void drawMessage();
  void layoutDetail();

  View view_ = View::List;
  Step step_ = Step::Idle;
  voz::Service service_ = voz::Service::Gemini;
  voz::Models models_;
  char keys_[2][200] = {};
  std::vector<Entry> entries_;
  int page_ = 0;
  int opened_ = -1;   // the entry the detail shows
  int sending_ = -1;  // the entry on its way to the service
  bool confirmErase_ = false;
  int shownSecond_ = -1;
  bool cleanNext_ = true;
  bool wifiActivated_ = false;
  const char* headline_ = "";
  const char* detail_ = "";

  // The detail as lines, laid out once per note and paged.
  struct Line {
    int font;
    int height;
    std::string text;
  };
  voz::Note note_;
  std::vector<Line> lines_;
  std::vector<int> pageStarts_;
  int detailPage_ = 0;
};

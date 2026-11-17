#pragma once

// PLEXAMP JUKEBOX: a remote for a Plexamp player on the same network.
//
// Links to a Plex account with a code typed at plex.tv/link, finds the
// account's Plexamp players, and then shows what one of them is playing and
// drives it. The protocol is in JukeboxCore; the wire is in JukeboxNet.

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "JukeboxCore.h"

class JukeboxActivity final : public Activity {
 public:
  JukeboxActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Jukebox", renderer, mappedInput) {}
  ~JukeboxActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void onExit() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Welcome, Linking, Players, Playing, Showcase, Queue, Browse, Menu, Unlink, Notice, Busy };
  enum class Step : uint8_t { None, PinCreate, Players, Adopt, Connect, Queue, Browse, Start };
  // Whether a command has ever been taken by this player, as far as this
  // sitting knows.
  enum class Control : uint8_t { Untried, Works, Failed };

  void loadConfig();
  void saveConfig();
  void forgetEverything();

  void request(Step next, const char* headline);
  void needNetwork(Step next, const char* headline);
  void notice(const char* headline, const char* message, View back);

  void doPinCreate();
  void pollPin();
  void doPlayers();
  void doAdopt();
  void doConnect();
  void doQueue();
  void doBrowse();
  void doStart();
  const jukebox::Config::Server* browseServer();
  void openLevel(uint8_t what, const std::string& key, const std::string& title, int count);
  void buildBrowse(toybox::Screen& screen);
  void askAddress();

  bool poll(bool withMetadata);
  void tick();
  void fetchArt();
  void command(const char* name, const char* extra = "");
  void act(freeink::ui::ActionId action, int value);
  int32_t positionNow() const;

  void buildWelcome(toybox::Screen& screen);
  freeink::ui::Rect buildLinking(toybox::Screen& screen);
  void buildPlayers(toybox::Screen& screen);
  void buildPlaying(toybox::Screen& screen);
  void buildShowcase(toybox::Screen& screen);
  void buildQueue(toybox::Screen& screen);
  void buildMenu(toybox::Screen& screen);
  bool drawCover(int x, int y, int side);
  uint32_t coverKey() const;
  void buildUnlink(toybox::Screen& screen);
  void buildNotice(toybox::Screen& screen);
  int pageBottom() const;
  int coverSide() const;
  int coverArea() const;
  void fillDisc(int cx, int cy, int radius, bool black);
  void ringAt(int cx, int cy, int radius, bool black);

  jukebox::Config config_;
  jukebox::Pin pin_;
  uint32_t pinStartedMs_ = 0;
  uint32_t lastPinPollMs_ = 0;

  std::vector<jukebox::Device> players_;
  // The player being tried, before it has earned a place in the settings.
  jukebox::Device candidate_;

  jukebox::Now now_;
  jukebox::Queue queue_;
  int queueFirst_ = 0;  // the first row the queue page drew, and where
  int16_t queueRowsY_ = 0;
  int16_t queueRows_ = 0;

  // Where the library is being looked at: each step down is a level.
  struct Level {
    uint8_t what = 0;
    std::string key;
    std::string title;
    int page = 0;
    int count = 0;
  };
  std::vector<Level> trail_;
  std::vector<jukebox::Entry> entries_;
  int entriesTotal_ = 0;
  // What is about to be started: an album or a playlist, perhaps from one track.
  jukebox::Entry startWhat_;
  std::string startTrack_;
  bool startShuffled_ = false;
  uint32_t nowAtMs_ = 0;
  uint32_t lastPollMs_ = 0;
  uint32_t lastPaintMs_ = 0;
  uint32_t commandId_ = 1;
  int misses_ = 0;
  bool monitorWorks_ = false;
  Control control_ = Control::Untried;
  // The cover on the card belongs to this track; 0 when there is none.
  uint32_t artFor_ = 0;
  bool artWanted_ = false;

  // What the panel last showed, so a poll that changed nothing paints nothing.
  std::string shownKey_;
  jukebox::State shownState_ = jukebox::State::Stopped;
  int shownVolume_ = -1;
  int shownShuffle_ = -1;
  int shownRepeat_ = -1;
  bool shownSilent_ = false;

  // Where a tap seeks, when the player takes seeking.
  int16_t barX_ = 0;
  int16_t barY_ = 0;
  int16_t barW_ = 0;
  // And where a touch sets the volume.
  int16_t volumeX_ = 0;
  int16_t volumeY_ = 0;
  int16_t volumeW_ = 0;
  // Where the cover sits on the playing page: a tap there opens it large.
  int16_t coverX_ = 0;
  int16_t coverY_ = 0;
  int16_t coverW_ = 0;

  View view_ = View::Welcome;
  View noticeBack_ = View::Welcome;
  Step step_ = Step::None;
  const char* busyHeadline_ = "";
  std::string noticeHeadline_;
  std::string noticeMessage_;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};

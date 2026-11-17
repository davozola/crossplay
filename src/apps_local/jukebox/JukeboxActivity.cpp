#include "JukeboxActivity.h"

#include <Bitmap.h>
#include <FontCacheManager.h>
#include <HalStorage.h>
#include <JpegToBmpConverter.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../components/UITheme.h"
#include "../../fontIds.h"
#include "../../util/QrUtils.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"
#include "JukeboxNet.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
using jukebox::Now;
using jukebox::State;

namespace {

enum : fui::ActionId {
  ActionLink = 1,
  ActionCancelLink,
  ActionPickPlayer,  // value: an index into the players found
  ActionSearchAgain,
  ActionTypeAddress,
  ActionPlayPause,
  ActionNext,
  ActionPrevious,
  ActionVolume,  // value: the step, -5 or +5
  ActionShuffle,
  ActionRepeat,
  ActionMenu,
  ActionQueue,
  ActionBackToPlayer,
  ActionChangePlayer,
  ActionUnlink,
  ActionUnlinkYes,
  ActionNoticeOk,
};

constexpr char kTag[] = "JUKE";
constexpr char kConfigPath[] = "/.crosspoint/jukebox.cfg";
constexpr char kDir[] = "/.crosspoint/jukebox";
constexpr char kArtJpg[] = "/.crosspoint/jukebox/art.jpg";
constexpr char kArtBmp[] = "/.crosspoint/jukebox/art.bmp";

constexpr uint32_t kPollMs = 3000;
// A player that has gone quiet is asked less often.
constexpr uint32_t kQuietPollMs = 8000;
constexpr int kQuietAfter = 3;
// The elapsed time moves on the panel this often while music plays.
constexpr uint32_t kProgressPaintMs = 20000;
constexpr uint32_t kPinPollMs = 3000;
constexpr uint32_t kPinLifeMs = 10UL * 60UL * 1000UL;
constexpr int kMaxPlayersShown = 5;
constexpr int kVolumeStep = 5;
constexpr auto kBold = EpdFontFamily::BOLD;

// The playing page, from the foot up: three rows of controls and the gaps
// between them, the progress line, and the track's three lines of text.
constexpr int kSmallRow = 50;
constexpr int kVolumeRow = 54;
constexpr int kTransportRow = 86;
constexpr int kRowGap = 8;
constexpr int kProgressRow = 40;
constexpr int kControlsHeight = kSmallRow + kVolumeRow + kTransportRow + 2 * kRowGap + kProgressRow;
constexpr int kTrackHeight = 100 + 14;
constexpr int kCoverTop = 44;
// The cover on its own: a margin above it, and the room the words below need.
constexpr int kShowcaseTop = 16;
constexpr int kShowcaseText = 190;
constexpr int kQueueWindow = 5;

uint32_t ms() { return static_cast<uint32_t>(millis()); }

}  // namespace

std::unique_ptr<Activity> JukeboxActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<JukeboxActivity>(renderer, mappedInput);
}

// --- Settings ------------------------------------------------------------------

void JukeboxActivity::loadConfig() {
  std::string text;
  if (Storage.exists(kConfigPath)) Storage.readFileToString(kTag, kConfigPath, 8192, text);
  jukebox::unpack(text, config_);
  if (config_.clientId.empty()) config_.clientId = jukebox::net::newClientId();
}

void JukeboxActivity::saveConfig() {
  Storage.mkdir(kDir);
  const std::string text = jukebox::pack(config_);
  HalFile file;
  if (!Storage.openFileForWrite(kTag, kConfigPath, file) || file.write(text.data(), text.size()) != text.size()) {
    LOG_ERR(kTag, "settings not written");
  }
}

void JukeboxActivity::forgetEverything() {
  Storage.remove(kConfigPath);
  Storage.remove(kArtJpg);
  Storage.remove(kArtBmp);
  config_ = jukebox::Config{};
  config_.clientId = jukebox::net::newClientId();
  now_ = Now{};
  players_.clear();
  artFor_ = 0;
  monitorWorks_ = false;
  control_ = Control::Untried;
}

// --- Lifecycle -----------------------------------------------------------------

void JukeboxActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadConfig();
  if (!config_.signedIn()) {
    view_ = View::Welcome;
    requestUpdate();
  } else if (!config_.hasPlayer()) {
    needNetwork(Step::Players, "LOOKING FOR PLAYERS");
  } else {
    needNetwork(Step::Connect, "CONNECTING");
  }
}

void JukeboxActivity::onExit() {
  Activity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void JukeboxActivity::request(const Step next, const char* headline) {
  {
    RenderLock lock(*this);
    view_ = View::Busy;
    busyHeadline_ = headline;
    step_ = next;
  }
  // The busy page is painted now and the work happens on the next pass.
  requestUpdate();
}

void JukeboxActivity::needNetwork(const Step next, const char* headline) {
  if (jukebox::net::kDemo || WiFi.status() == WL_CONNECTED) {
    request(next, headline);
    return;
  }
  step_ = next;
  busyHeadline_ = headline;
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           interactionsReady_ = false;
                           if (result.isCancelled) {
                             step_ = Step::None;
                             notice("NO WI-FI", "THE JUKEBOX NEEDS THE SAME WI-FI NETWORK AS YOUR PLEXAMP PLAYER.",
                                    config_.signedIn() ? View::Menu : View::Welcome);
                             return;
                           }
                           request(step_, busyHeadline_);
                         });
}

void JukeboxActivity::notice(const char* headline, const char* message, const View back) {
  {
    RenderLock lock(*this);
    noticeHeadline_ = headline;
    noticeMessage_ = message;
    noticeBack_ = back;
    view_ = View::Notice;
    step_ = Step::None;
  }
  requestUpdate();
}

// --- Linking the account -------------------------------------------------------

void JukeboxActivity::doPinCreate() {
  std::string body;
  const int status = jukebox::net::account(config_, "POST", jukebox::pinCreateUrl(), body);
  pin_ = jukebox::Pin{};
  if ((status != 200 && status != 201) || !jukebox::parsePin(body.data(), body.size(), pin_)) {
    notice("PLEX DID NOT ANSWER",
           status == 0 ? "COULD NOT REACH PLEX.TV OVER A VERIFIED CONNECTION. CHECK THE WI-FI AND TRY AGAIN."
                       : "PLEX.TV REFUSED TO START A LINK. TRY AGAIN IN A MINUTE.",
           View::Welcome);
    return;
  }
  pinStartedMs_ = ms();
  lastPinPollMs_ = pinStartedMs_;
  view_ = View::Linking;
}

void JukeboxActivity::pollPin() {
  const uint32_t now = ms();
  if (now - pinStartedMs_ > kPinLifeMs) {
    notice("THE CODE EXPIRED", "NOBODY ENTERED THE CODE IN TIME. START AGAIN FOR A NEW ONE.", View::Welcome);
    return;
  }
  if (now - lastPinPollMs_ < kPinPollMs) return;
  lastPinPollMs_ = now;
  std::string body;
  jukebox::Pin answer;
  const int status = jukebox::net::account(config_, "GET", jukebox::pinPollUrl(pin_.id), body);
  if (status != 200 || !jukebox::parsePin(body.data(), body.size(), answer) || answer.token.empty()) return;
  config_.token = answer.token;
  saveConfig();
  LOG_INF(kTag, "account linked");
  request(Step::Players, "LOOKING FOR PLAYERS");
}

// --- Finding and choosing a player ---------------------------------------------

void JukeboxActivity::doPlayers() {
  std::string body;
  const int status = jukebox::net::account(config_, "GET", jukebox::resourcesUrl(), body);
  if (status == 401) {
    config_.token.clear();
    saveConfig();
    notice("NOT LINKED ANY MORE", "PLEX NO LONGER ACCEPTS THIS READER. LINK THE ACCOUNT AGAIN.", View::Welcome);
    return;
  }
  std::vector<jukebox::Device> devices;
  devices.reserve(8);
  if (status != 200 || !jukebox::parseResources(body.data(), body.size(), devices)) {
    notice("PLEX DID NOT ANSWER", "COULD NOT READ YOUR DEVICES FROM PLEX.TV. CHECK THE WI-FI AND TRY AGAIN.",
           config_.hasPlayer() ? View::Menu : View::Players);
    return;
  }
  players_.clear();
  players_.reserve(devices.size());
  config_.servers.clear();
  for (const jukebox::Device& d : devices) {
    if (d.server && !d.token.empty() && config_.servers.size() < 8) config_.servers.push_back({d.id, d.token});
    // Only Plexamp, and only where it gave an address on this network.
    if (d.player && d.product == "Plexamp" && !d.host.empty()) players_.push_back(d);
  }
  saveConfig();
  view_ = View::Players;
}

void JukeboxActivity::askAddress() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "PLAYER IP ADDRESS", "", 40);
  if (!keyboard) {
    notice("NO MEMORY", "THERE WAS NOT ENOUGH MEMORY TO OPEN THE KEYBOARD.", View::Players);
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    interactionsReady_ = false;
    if (result.isCancelled) {
      requestUpdate();
      return;
    }
    const auto& entered = std::get<KeyboardResult>(result.data);
    std::string host;
    int port = 0;
    if (!jukebox::parseAddress(entered.text, host, port) || !jukebox::isLocalAddress(host)) {
      notice("NOT A LOCAL ADDRESS",
             "TYPE THE PLAYER'S ADDRESS ON THIS NETWORK, LIKE 192.168.1.20. THE JUKEBOX ONLY TALKS TO PLAYERS ON THE "
             "LOCAL NETWORK.",
             View::Players);
      return;
    }
    candidate_ = jukebox::Device{};
    candidate_.host = host;
    candidate_.port = port;
    request(Step::Adopt, "TRYING THE PLAYER");
  });
}

// The candidate earns its place by answering: its timeline has to read.
void JukeboxActivity::doAdopt() {
  jukebox::Config trial = config_;
  trial.playerId = candidate_.id;
  std::string body;
  if (candidate_.id.empty()) {
    // A typed address: the player says who it is.
    std::string product;
    const int status =
        jukebox::net::player(trial, candidate_.host, jukebox::playerInfoUrl(candidate_.host, candidate_.port), body);
    if (status == 200) jukebox::parsePlayerInfo(body.data(), body.size(), candidate_.name, candidate_.id, product);
    trial.playerId = candidate_.id;
  }
  Now seen;
  const int status = jukebox::net::player(
      trial, candidate_.host, jukebox::timelineUrl(candidate_.host, candidate_.port, commandId_++, true), body);
  if (status != 200 || !jukebox::parseTimeline(body.data(), body.size(), seen)) {
    notice("THE PLAYER DID NOT ANSWER",
           "OPEN PLEXAMP ON THE PLAYER, CHECK THAT ITS REMOTE CONTROL SETTING IS ON AND THAT IT IS ON THIS WI-FI "
           "NETWORK, THEN TRY AGAIN.",
           View::Players);
    return;
  }
  config_.playerName = candidate_.name.empty() ? candidate_.host : candidate_.name;
  config_.playerId = candidate_.id;
  config_.playerHost = candidate_.host;
  config_.playerPort = candidate_.port > 0 ? candidate_.port : jukebox::kPlayerPort;
  saveConfig();
  now_ = seen;
  nowAtMs_ = lastPollMs_ = ms();
  misses_ = 0;
  monitorWorks_ = true;
  control_ = Control::Untried;
  artFor_ = 0;
  artWanted_ = !now_.thumb.empty();
  view_ = View::Playing;
}

void JukeboxActivity::doQueue() {
  std::string body;
  const int status =
      jukebox::net::server(config_, now_, jukebox::queueUrl(now_.serverBase, now_.queueId, kQueueWindow * 2), body);
  if (status != 200 || !jukebox::parseQueue(body.data(), body.size(), queue_)) {
    notice("NO QUEUE", "THE SERVER DID NOT GIVE THE QUEUE OVER A VERIFIED CONNECTION. THE CONTROLS STILL WORK.",
           View::Playing);
    return;
  }
  view_ = View::Queue;
}

void JukeboxActivity::doConnect() {
  now_ = Now{};
  artFor_ = 0;
  if (!poll(true)) {
    misses_ = kQuietAfter;
  }
  lastPollMs_ = ms();
  view_ = View::Playing;
}

// --- Watching and driving the player -------------------------------------------

int32_t JukeboxActivity::positionNow() const {
  int32_t at = now_.timeMs;
  if (now_.state == State::Playing) at += static_cast<int32_t>(ms() - nowAtMs_);
  if (now_.durationMs > 0 && at > now_.durationMs) at = now_.durationMs;
  return at < 0 ? 0 : at;
}

bool JukeboxActivity::poll(const bool withMetadata) {
  std::string body;
  const int status = jukebox::net::player(
      config_, config_.playerHost,
      jukebox::timelineUrl(config_.playerHost, config_.playerPort, commandId_++, withMetadata), body);
  Now seen = now_;
  if (status != 200 || !jukebox::parseTimeline(body.data(), body.size(), seen, !withMetadata)) {
    ++misses_;
    monitorWorks_ = false;
    return false;
  }
  misses_ = 0;
  monitorWorks_ = true;
  now_ = seen;
  nowAtMs_ = ms();
  // A track this reader has no title for is asked about once more, in full.
  if (!withMetadata && now_.title.empty() && !now_.trackKey.empty() && now_.state != State::Stopped) return poll(true);
  if (now_.thumb.empty()) {
    artFor_ = 0;
    artWanted_ = false;
  } else if (coverKey() != artFor_) {
    artWanted_ = true;
  }
  return true;
}

void JukeboxActivity::fetchArt() {
  artWanted_ = false;
  const uint32_t key = coverKey();
  artFor_ = 0;
  Storage.mkdir(kDir);
  // Asked for at the size it is drawn, so the reader never decodes a full cover.
  const int side = coverSide();
  if (jukebox::net::art(config_, now_, side, kArtJpg) != 200) return;
  HalFile jpg;
  HalFile bmp;
  if (!Storage.openFileForRead(kTag, kArtJpg, jpg)) return;
  if (!Storage.openFileForWrite(kTag, kArtBmp, bmp)) return;
  const bool converted = JpegToBmpConverter::jpegFileTo1BitBmpStreamWithSize(jpg, bmp, side, side);
  // Closed before the remove, which the card insists on.
  jpg.close();
  bmp.close();
  Storage.remove(kArtJpg);
  if (!converted) {
    LOG_ERR(kTag, "cover did not convert");
    Storage.remove(kArtBmp);
    return;
  }
  artFor_ = key;
}

void JukeboxActivity::command(const char* name, const char* extra) {
  std::string body;
  const int status = jukebox::net::player(
      config_, config_.playerHost,
      jukebox::commandUrl(config_.playerHost, config_.playerPort, name, commandId_++, extra), body);
  control_ = status == 200 ? Control::Works : Control::Failed;
  if (status != 200) LOG_ERR(kTag, "%s refused: %d", name, status);
  // What the player did about it, rather than what was asked of it.
  delay(250);
  poll(false);
  lastPollMs_ = ms();
  requestUpdate();
}

void JukeboxActivity::tick() {
  if (artWanted_) {
    fetchArt();
    requestUpdate();
    return;
  }
  const uint32_t now = ms();
  const bool silent = misses_ >= kQuietAfter;
  if (now - lastPollMs_ >= (silent ? kQuietPollMs : kPollMs)) {
    poll(false);
    lastPollMs_ = ms();
  }
  const bool silentNow = misses_ >= kQuietAfter;
  const bool changed = now_.trackKey != shownKey_ || now_.state != shownState_ || now_.volume != shownVolume_ ||
                       now_.shuffle != shownShuffle_ || now_.repeat != shownRepeat_ || silentNow != shownSilent_;
  const bool progress = now_.state == State::Playing && !silentNow && ms() - lastPaintMs_ >= kProgressPaintMs;
  if (changed || progress) requestUpdate();
}

void JukeboxActivity::act(const fui::ActionId action, const int value) {
  switch (action) {
    case ActionLink:
      needNetwork(Step::PinCreate, "ASKING PLEX FOR A CODE");
      break;
    case ActionCancelLink:
      pin_ = jukebox::Pin{};
      view_ = View::Welcome;
      requestUpdate();
      break;
    case ActionPickPlayer:
      if (value >= 0 && value < static_cast<int>(players_.size())) {
        candidate_ = players_[static_cast<size_t>(value)];
        request(Step::Adopt, "TRYING THE PLAYER");
      }
      break;
    case ActionSearchAgain:
      needNetwork(Step::Players, "LOOKING FOR PLAYERS");
      break;
    case ActionTypeAddress:
      askAddress();
      break;
    case ActionPlayPause:
      if (now_.can & jukebox::CanPlayPause) command(now_.state == State::Playing ? "pause" : "play");
      break;
    case ActionNext:
      if (now_.can & jukebox::CanNext) command("skipNext");
      break;
    case ActionPrevious:
      if (now_.can & jukebox::CanPrevious) command("skipPrevious");
      break;
    case ActionVolume:
      if ((now_.can & jukebox::CanVolume) && now_.volume >= 0) {
        int to = now_.volume + value;
        to = to < 0 ? 0 : to > 100 ? 100 : to;
        char extra[24];
        std::snprintf(extra, sizeof(extra), "&volume=%d", to % 1000);
        command("setParameters", extra);
      }
      break;
    case ActionShuffle:
      if ((now_.can & jukebox::CanShuffle) && now_.shuffle >= 0) {
        command("setParameters", now_.shuffle ? "&shuffle=0" : "&shuffle=1");
      }
      break;
    case ActionRepeat:
      if ((now_.can & jukebox::CanRepeat) && now_.repeat >= 0) {
        // Off, the whole queue, one track, and round again.
        command("setParameters", now_.repeat == 0 ? "&repeat=2" : now_.repeat == 2 ? "&repeat=1" : "&repeat=0");
      }
      break;
    case ActionMenu:
      view_ = View::Menu;
      requestUpdate();
      break;
    case ActionQueue:
      request(Step::Queue, "READING THE QUEUE");
      break;
    case ActionBackToPlayer:
      view_ = config_.hasPlayer() ? View::Playing : View::Players;
      // The cover on the card may be the large one.
      artWanted_ = view_ == View::Playing && !now_.thumb.empty() && coverKey() != artFor_;
      requestUpdate();
      break;
    case ActionChangePlayer:
      needNetwork(Step::Players, "LOOKING FOR PLAYERS");
      break;
    case ActionUnlink:
      view_ = View::Unlink;
      requestUpdate();
      break;
    case ActionUnlinkYes:
      forgetEverything();
      view_ = View::Welcome;
      requestUpdate();
      break;
    case ActionNoticeOk:
      if (noticeBack_ == View::Players && players_.empty() && config_.signedIn()) {
        needNetwork(Step::Players, "LOOKING FOR PLAYERS");
      } else {
        view_ = noticeBack_;
        requestUpdate();
      }
      break;
    default:
      break;
  }
}

void JukeboxActivity::loop() {
  if (step_ != Step::None && view_ == View::Busy) {
    const Step what = step_;
    step_ = Step::None;
    switch (what) {
      case Step::PinCreate:
        doPinCreate();
        break;
      case Step::Players:
        doPlayers();
        break;
      case Step::Adopt:
        doAdopt();
        break;
      case Step::Connect:
        doConnect();
        break;
      case Step::Queue:
        doQueue();
        break;
      case Step::None:
        break;
    }
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (view_) {
      case View::Linking:
        act(ActionCancelLink, 0);
        break;
      case View::Menu:
      case View::Unlink:
        if (view_ == View::Unlink) {
          view_ = View::Menu;
          requestUpdate();
        } else if (config_.hasPlayer()) {
          act(ActionBackToPlayer, 0);
        } else {
          shelf::leave(renderer, mappedInput);
        }
        break;
      case View::Notice:
        act(ActionNoticeOk, 0);
        break;
      case View::Showcase:
      case View::Queue:
        act(ActionBackToPlayer, 0);
        break;
      case View::Players:
        if (config_.hasPlayer()) {
          view_ = View::Playing;
          requestUpdate();
        } else {
          shelf::leave(renderer, mappedInput);
        }
        break;
      default:
        shelf::leave(renderer, mappedInput);
        break;
    }
    return;
  }

  // The keys, on a reader that has them.
  if (view_ == View::Playing || view_ == View::Showcase) {
    using Button = MappedInputManager::Button;
    if (mappedInput.wasReleased(Button::Confirm)) return act(ActionPlayPause, 0);
    if (mappedInput.wasReleased(Button::Right)) return act(ActionNext, 0);
    if (mappedInput.wasReleased(Button::Left)) return act(ActionPrevious, 0);
    if (mappedInput.wasReleased(Button::Up)) return act(ActionVolume, kVolumeStep);
    if (mappedInput.wasReleased(Button::Down)) return act(ActionVolume, -kVolumeStep);
  }

  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY) && interactionsReady_) {
    // The progress bar: a tap along it is a place in the track.
    if (view_ == View::Playing && (now_.can & jukebox::CanSeek) && now_.durationMs > 0 && barW_ > 0 &&
        tapY >= barY_ - 18 && tapY <= barY_ + 28 && tapX >= barX_ && tapX <= barX_ + barW_) {
      const long offset = static_cast<long>(now_.durationMs) * (tapX - barX_) / barW_;
      char extra[32];
      std::snprintf(extra, sizeof(extra), "&offset=%ld", offset % 100000000L);
      command("seekTo", extra);
      return;
    }
    // The cover opens large, and the large cover closes on any tap.
    if (view_ == View::Showcase) {
      act(ActionBackToPlayer, 0);
      return;
    }
    if (view_ == View::Playing && coverW_ > 0 && tapX >= coverX_ && tapX <= coverX_ + coverW_ && tapY >= coverY_ &&
        tapY <= coverY_ + coverW_) {
      view_ = View::Showcase;
      artWanted_ = !now_.thumb.empty();
      requestUpdate();
      return;
    }
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
    const fui::ActionEvent event = interactions_.route(input);
    if (event.action != 0) {
      act(event.action, event.value);
      return;
    }
  }

  if (view_ == View::Linking) {
    pollPin();
  } else if (view_ == View::Playing || view_ == View::Showcase) {
    tick();
  }
}

// --- Pages ---------------------------------------------------------------------

// A touch reader draws no button hints, so the page runs to the foot.
int JukeboxActivity::pageBottom() const {
  return renderer.getScreenHeight() - (mappedInput.hasTouch() ? 8 : lab::kBottomReserve + 6);
}

// The cover's square: what the page has left between its head and the track's
// name. Asked of the server at this size and drawn at it.
uint32_t JukeboxActivity::coverKey() const {
  return jukebox::artHash(now_.serverId, now_.thumb) ^ (static_cast<uint32_t>(coverSide()) * 2654435761u);
}

int JukeboxActivity::coverSide() const {
  const int inner = renderer.getScreenWidth() - 2 * toybox::kMargin;
  if (view_ == View::Showcase) return std::min(inner, pageBottom() - kShowcaseText - kShowcaseTop);
  const int room = pageBottom() - kControlsHeight - kTrackHeight - 10 - kCoverTop;
  return std::max(120, std::min(inner, room));
}

void JukeboxActivity::buildWelcome(toybox::Screen& screen) {
  lab::chrome(screen, "JUKEBOX");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 24;
  lab::label(screen, lab::rect(x, y, inner, toybox::kDisplayCut.inkHeight), "PLEXAMP REMOTE", toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Left);
  y += toybox::kDisplayCut.inkHeight + 20;
  lab::paragraph(screen, x, y, inner,
                 "SEE WHAT A PLEXAMP PLAYER ON THIS WI-FI NETWORK IS PLAYING, AND PAUSE, SKIP AND SET ITS VOLUME FROM "
                 "HERE.",
                 4);
  y += toybox::kTileCut.lineHeight * 4 + 16;
  lab::caption(screen, x, y, inner, "YOU NEED");
  y += toybox::kTileCut.inkHeight + 10;
  lab::paragraph(screen, x, y, inner,
                 "A PLEX ACCOUNT, PLEXAMP OPEN ON THE PLAYER WITH ITS REMOTE CONTROL SETTING ON, AND BOTH ON THE SAME "
                 "NETWORK. NOTHING IS INSTALLED ON THE PLAYER.",
                 5);
  y += toybox::kTileCut.lineHeight * 5 + 16;
  lab::caption(screen, x, y, inner, "WHAT IS KEPT");
  y += toybox::kTileCut.inkHeight + 10;
  lab::paragraph(screen, x, y, inner,
                 "A PLEX SIGN-IN FOR THIS READER, ON ITS MEMORY CARD. NOT YOUR PASSWORD. UNLINK FROM THE MENU TO ERASE "
                 "IT.",
                 4);
  if (jukebox::net::kDemo) {
    lab::caption(screen, x, pageBottom() - 64 - 14 - toybox::kTileCut.inkHeight, inner,
                 "SIMULATOR: A MADE-UP ACCOUNT AND PLAYER");
  }
  lab::button(screen, lab::rect(x, pageBottom() - 64, inner, 64), "LINK A PLEX ACCOUNT", ActionLink, 0, true, false);
}

fui::Rect JukeboxActivity::buildLinking(toybox::Screen& screen) {
  lab::chrome(screen, "JUKEBOX");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 16;
  lab::caption(screen, x, y, inner, "1  ON A PHONE OR COMPUTER, OPEN", fui::TextAlign::Center);
  y += toybox::kTileCut.inkHeight + 12;
  lab::value(screen, x, y, inner, "PLEX.TV/LINK", fui::TextAlign::Center);
  y += toybox::kUiCut.inkHeight + 16;
  constexpr int kQrSide = 190;
  const fui::Rect qr = lab::rect(x + (inner - kQrSide) / 2, y, kQrSide, kQrSide);
  y += kQrSide + 22;
  lab::caption(screen, x, y, inner, "2  SIGN IN AND TYPE THIS CODE", fui::TextAlign::Center);
  y += toybox::kTileCut.inkHeight + 14;
  renderer.drawText(toybox::kLargeFontId,
                    x + (inner - renderer.getTextWidth(toybox::kLargeFontId, pin_.code.c_str())) / 2, y - 10,
                    pin_.code.c_str(), true);
  y += toybox::kLargeCut.inkHeight + 22;
  lab::caption(screen, x, y, inner, "WAITING FOR PLEX. THE CODE LASTS 10 MINUTES.", fui::TextAlign::Center);
  lab::button(screen, lab::rect(x, pageBottom() - 60, inner, 60), "CANCEL", ActionCancelLink, 0, false, false);
  return qr;
}

void JukeboxActivity::buildPlayers(toybox::Screen& screen) {
  lab::chrome(screen, "JUKEBOX");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 14;
  lab::caption(screen, x, y, inner, players_.empty() ? "NO PLEXAMP PLAYER FOUND" : "CHOOSE A PLAYER");
  y += toybox::kTileCut.inkHeight + 12;
  const int rowH = 78;
  int shown = 0;
  for (size_t i = 0; i < players_.size() && shown < kMaxPlayersShown; ++i, ++shown) {
    const jukebox::Device& d = players_[i];
    const bool current = !config_.playerId.empty() && d.id == config_.playerId;
    lab::button(screen, lab::rect(x, y, inner, rowH), "", ActionPickPlayer, static_cast<int>(i), false, false);
    // Names are whatever their owners typed, so they get a face with accents.
    const std::string name = renderer.truncatedText(NOTOSANS_14_FONT_ID, d.name.c_str(), inner - 28, kBold);
    renderer.drawText(NOTOSANS_14_FONT_ID, x + 14, y + 6, name.c_str(), true, kBold);
    char where[40];
    std::snprintf(where, sizeof(where), "%s:%d", d.host.c_str(), d.port % 100000);
    const std::string detail = d.platform + "  \xC2\xB7  " + where + (current ? "  \xC2\xB7  in use" : "");
    renderer.drawText(NOTOSANS_12_FONT_ID, x + 14, y + 38,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, detail.c_str(), inner - 28).c_str(), true);
    y += rowH + 8;
  }
  if (players_.empty()) {
    lab::paragraph(screen, x, y, inner,
                   "OPEN PLEXAMP ON THE PLAYER, SIGNED IN TO THE SAME PLEX ACCOUNT AND ON THIS WI-FI NETWORK, WITH ITS "
                   "REMOTE CONTROL SETTING ON. THEN SEARCH AGAIN, OR TYPE THE PLAYER'S ADDRESS.",
                   6);
  }
  const int buttonH = 56;
  int by = pageBottom() - buttonH;
  if (config_.signedIn()) {
    lab::button(screen, lab::rect(x, by, inner, buttonH), "ACCOUNT", ActionMenu, 0, false, false);
    by -= buttonH + 8;
  }
  const int half = (inner - 8) / 2;
  lab::button(screen, lab::rect(x, by, half, buttonH), "SEARCH AGAIN", ActionSearchAgain, 0, false, true);
  lab::button(screen, lab::rect(x + half + 8, by, half, buttonH), "TYPE ADDRESS", ActionTypeAddress, 0, false, true);
}

void JukeboxActivity::buildPlaying(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - toybox::kMargin * 2;
  const bool silent = misses_ >= kQuietAfter;
  const bool idle = now_.state == State::Stopped || now_.trackKey.empty();

  // The rows are placed from the foot up, and the cover takes what is left.
  const int foot = pageBottom();
  const int smallH = kSmallRow;
  const int volumeH = kVolumeRow;
  const int transportH = kTransportRow;
  const int gap = kRowGap;
  const int smallY = foot - smallH;
  const int volumeY = smallY - gap - volumeH;
  const int transportY = volumeY - gap - transportH;
  const int barY = transportY - kProgressRow;
  const int textY = barY - kTrackHeight;

  // The head: which player, and what it is doing.
  const int headY = 10;
  const char* state = silent                           ? "NO ANSWER"
                      : now_.state == State::Playing   ? "PLAYING"
                      : now_.state == State::Paused    ? "PAUSED"
                      : now_.state == State::Buffering ? "LOADING"
                                                       : "IDLE";
  lab::caption(screen, x, headY + 6, inner, state, fui::TextAlign::Right);
  const std::string player =
      renderer.truncatedText(NOTOSANS_12_FONT_ID, config_.playerName.c_str(), inner - 130, kBold);
  renderer.drawText(NOTOSANS_12_FONT_ID, x, headY, player.c_str(), true, kBold);
  if (jukebox::net::kDemo) lab::caption(screen, x, headY + 6, inner, "DEMO DATA", fui::TextAlign::Center);

  // The cover.
  const int artTop = kCoverTop;
  const int side = coverSide();
  const int artX = x + (inner - side) / 2;
  coverX_ = static_cast<int16_t>(artX);
  coverY_ = static_cast<int16_t>(artTop);
  coverW_ = static_cast<int16_t>(side);
  const bool drawn = !idle && drawCover(artX, artTop, side);
  if (!drawn) {
    renderer.drawRoundedRect(artX, artTop, side, side, 2, 10, true);
    const char* head = silent && idle ? "PLAYER NOT ANSWERING" : idle ? "NOTHING PLAYING" : "NO COVER";
    lab::label(screen, lab::rect(artX, artTop + side / 2 - 40, side, toybox::kUiCut.inkHeight), head, toybox::kUiFont,
               toybox::kUiCut);
    if (idle) {
      lab::caption(screen, artX, artTop + side / 2 + 10, side,
                   silent ? "IS PLEXAMP OPEN, ON THIS NETWORK?" : "START SOMETHING IN PLEXAMP", fui::TextAlign::Center);
    }
  }

  // The track.
  if (!idle) {
    const std::string title = renderer.truncatedText(NOTOSANS_16_FONT_ID, now_.title.c_str(), inner, kBold);
    renderer.drawText(NOTOSANS_16_FONT_ID, x, textY, title.c_str(), true, kBold);
    const std::string artist = renderer.truncatedText(NOTOSANS_14_FONT_ID, now_.artist.c_str(), inner);
    renderer.drawText(NOTOSANS_14_FONT_ID, x, textY + 40, artist.c_str(), true);
    const std::string album = now_.year.empty() ? now_.album : now_.album + "  \xC2\xB7  " + now_.year;
    renderer.drawText(NOTOSANS_12_FONT_ID, x, textY + 72,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, album.c_str(), inner).c_str(), true);

    // Where it is. The bar is the seek control when the player takes seeking.
    char at[16];
    char total[16];
    const int32_t position = positionNow();
    jukebox::formatTime(position, at, sizeof(at));
    jukebox::formatTime(now_.durationMs, total, sizeof(total));
    const int timeW = 64;
    barX_ = static_cast<int16_t>(x + timeW);
    barW_ = static_cast<int16_t>(inner - 2 * timeW);
    barY_ = static_cast<int16_t>(barY + 8);
    renderer.drawText(NOTOSANS_12_FONT_ID, x, barY, at, true);
    renderer.drawText(NOTOSANS_12_FONT_ID, x + inner - renderer.getTextWidth(NOTOSANS_12_FONT_ID, total), barY, total,
                      true);
    renderer.drawRoundedRect(barX_, barY_, barW_, 12, 2, 6, true);
    if (now_.durationMs > 0) {
      const int fill = static_cast<int>(static_cast<long>(barW_ - 6) * position / now_.durationMs);
      if (fill > 0) renderer.fillRect(barX_ + 3, barY_ + 3, fill, 6, true);
    }
  } else {
    barW_ = 0;
  }

  // Only what this player says it takes is drawn.
  const uint16_t can = silent ? 0 : now_.can;
  const int third = (inner - 2 * gap) / 3;
  auto transport = [&](const int slot, const fui::ActionId action, const int glyph) {
    const int bx = x + slot * (third + gap);
    lab::button(screen, lab::rect(bx, transportY, third, transportH), "", action, 0, false, false);
    const int cx = bx + third / 2;
    const int cy = transportY + transportH / 2;
    const int r = 18;
    auto triangle = [&](const int left, const int dir) {
      const int xs[3] = {left, left, left + dir * (r + 8)};
      const int ys[3] = {cy - r, cy + r, cy};
      renderer.fillPolygon(xs, ys, 3, true);
    };
    if (glyph == 0) {  // previous
      renderer.fillRect(cx - r - 6, cy - r, 6, 2 * r, true);
      triangle(cx + r + 4, -1);
    } else if (glyph == 1) {  // play
      triangle(cx - r / 2 - 4, 1);
    } else if (glyph == 2) {  // pause
      renderer.fillRect(cx - 16, cy - r, 11, 2 * r, true);
      renderer.fillRect(cx + 5, cy - r, 11, 2 * r, true);
    } else {  // next
      triangle(cx - r - 4, 1);
      renderer.fillRect(cx + r, cy - r, 6, 2 * r, true);
    }
  };
  if (can & jukebox::CanPrevious) transport(0, ActionPrevious, 0);
  if (can & jukebox::CanPlayPause) transport(1, ActionPlayPause, now_.state == State::Playing ? 2 : 1);
  if (can & jukebox::CanNext) transport(2, ActionNext, 3);
  if (can == 0) {
    lab::caption(screen, x, transportY + transportH / 2 - 8, inner,
                 silent ? "WAITING FOR THE PLAYER TO ANSWER" : "THIS PLAYER OFFERS NO CONTROLS RIGHT NOW",
                 fui::TextAlign::Center);
  }

  if ((can & jukebox::CanVolume) && now_.volume >= 0) {
    const int side2 = 96;
    lab::button(screen, lab::rect(x, volumeY, side2, volumeH), "-", ActionVolume, -kVolumeStep, false, false);
    lab::button(screen, lab::rect(x + inner - side2, volumeY, side2, volumeH), "+", ActionVolume, kVolumeStep, false,
                false);
    char volume[24];
    std::snprintf(volume, sizeof(volume), "VOLUME %d", now_.volume % 1000);
    lab::label(screen, lab::rect(x + side2, volumeY, inner - 2 * side2, volumeH), volume, toybox::kUiFont,
               toybox::kUiCut);
  }

  const int cell = (inner - 3 * gap) / 4;
  if ((can & jukebox::CanShuffle) && now_.shuffle >= 0) {
    lab::button(screen, lab::rect(x, smallY, cell, smallH), "SHUFFLE", ActionShuffle, 0, now_.shuffle != 0, true);
  }
  if ((can & jukebox::CanRepeat) && now_.repeat >= 0) {
    lab::button(screen, lab::rect(x + cell + gap, smallY, cell, smallH),
                now_.repeat == 1   ? "REPEAT 1"
                : now_.repeat == 2 ? "REP. ALL"
                                   : "REPEAT",
                ActionRepeat, 0, now_.repeat != 0, true);
  }
  // The queue lives on the server the track came from; without one there is none to show.
  if (!silent && !now_.queueId.empty() && !now_.serverBase.empty()) {
    lab::button(screen, lab::rect(x + 2 * (cell + gap), smallY, cell, smallH), "QUEUE", ActionQueue, 0, false, true);
  }
  lab::button(screen, lab::rect(x + 3 * (cell + gap), smallY, cell, smallH), "MENU", ActionMenu, 0, false, true);

  shownKey_ = now_.trackKey;
  shownState_ = now_.state;
  shownVolume_ = now_.volume;
  shownShuffle_ = now_.shuffle;
  shownRepeat_ = now_.repeat;
  shownSilent_ = silent;
  lastPaintMs_ = ms();
}

// The saved cover, when it is this track's at this size. False when there is
// none to draw.
bool JukeboxActivity::drawCover(const int x, const int y, const int side) {
  if (artFor_ == 0 || artFor_ != coverKey()) return false;
  // drawBitmap is a silent no-op while the font cache is scanning.
  const FontCacheManager* fonts = renderer.getFontCacheManager();
  HalFile file;
  if ((fonts != nullptr && fonts->isScanning()) || !Storage.openFileForRead(kTag, kArtBmp, file)) return false;
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok || bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0) return false;
  const int w = std::min(side, bitmap.getWidth());
  const int h = std::min(side, bitmap.getHeight());
  const int ax = x + (side - w) / 2;
  const int ay = y + (side - h) / 2;
  renderer.drawBitmap1Bit(bitmap, ax, ay, side, side);
  // A hairline, so a cover with a white edge still has one.
  renderer.drawRect(ax, ay, w, h, true);
  return true;
}

// The cover and the words, and nothing to press: a page to leave on a shelf
// while the music plays. A tap anywhere goes back to the controls.
void JukeboxActivity::buildShowcase(toybox::Screen& screen) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  const int side = coverSide();
  const int artX = x + (inner - side) / 2;
  const bool silent = misses_ >= kQuietAfter;
  const bool idle = now_.state == State::Stopped || now_.trackKey.empty();
  if (idle || !drawCover(artX, kShowcaseTop, side)) {
    renderer.drawRoundedRect(artX, kShowcaseTop, side, side, 2, 10, true);
    lab::label(screen, lab::rect(artX, kShowcaseTop + side / 2 - 20, side, toybox::kUiCut.inkHeight),
               silent && idle ? "PLAYER NOT ANSWERING"
               : idle         ? "NOTHING PLAYING"
                              : "NO COVER",
               toybox::kUiFont, toybox::kUiCut);
  }
  int y = kShowcaseTop + side + 14;
  if (!idle) {
    auto centred = [&](const int font, const std::string& text, const EpdFontFamily::Style style) {
      const std::string line = renderer.truncatedText(font, text.c_str(), inner, style);
      renderer.drawText(font, x + (inner - renderer.getTextWidth(font, line.c_str(), style)) / 2, y, line.c_str(), true,
                        style);
    };
    centred(NOTOSANS_18_FONT_ID, now_.title, kBold);
    y += 46;
    centred(NOTOSANS_16_FONT_ID, now_.artist, EpdFontFamily::REGULAR);
    y += 40;
    centred(NOTOSANS_12_FONT_ID, now_.year.empty() ? now_.album : now_.album + "  \xC2\xB7  " + now_.year,
            EpdFontFamily::REGULAR);
    y += 38;
    if (now_.durationMs > 0) {
      const int32_t position = positionNow();
      renderer.drawRoundedRect(x, y, inner, 10, 2, 5, true);
      const int fill = static_cast<int>(static_cast<long>(inner - 6) * position / now_.durationMs);
      if (fill > 0) renderer.fillRect(x + 3, y + 3, fill, 4, true);
    }
  }
  const char* state = silent ? "NO ANSWER" : now_.state == State::Paused ? "PAUSED" : "";
  lab::caption(screen, x, pageBottom() - toybox::kTileCut.inkHeight, inner, state, fui::TextAlign::Center);
  barW_ = 0;
  shownKey_ = now_.trackKey;
  shownState_ = now_.state;
  shownVolume_ = now_.volume;
  shownShuffle_ = now_.shuffle;
  shownRepeat_ = now_.repeat;
  shownSilent_ = silent;
  lastPaintMs_ = ms();
}

// What comes next, as the server has it. To look at, for now: jumping to a
// track from here waits until that command has been tried on a real player.
void JukeboxActivity::buildQueue(toybox::Screen& screen) {
  lab::chrome(screen, "QUEUE");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 10;
  char count[toybox::kIntChars * 2 + 24];
  std::snprintf(count, sizeof(count), "%d TRACKS IN THE QUEUE", queue_.total % 100000);
  lab::caption(screen, x, y, inner, count);
  y += toybox::kTileCut.inkHeight + 10;
  const int rowH = 52;
  const int buttonH = 56;
  const int room = (pageBottom() - buttonH - 10 - y) / rowH;
  // The playing track a third of the way down, so most of the page is what follows.
  int first = queue_.current > 2 ? queue_.current - 2 : 0;
  const int count2 = static_cast<int>(queue_.items.size());
  if (first + room > count2) first = std::max(0, count2 - room);
  for (int i = first; i < count2 && i < first + room; ++i) {
    const jukebox::QueueItem& item = queue_.items[static_cast<size_t>(i)];
    const bool current = i == queue_.current;
    char length[16];
    jukebox::formatTime(item.durationMs, length, sizeof(length));
    const int lengthW = renderer.getTextWidth(NOTOSANS_12_FONT_ID, length);
    if (current) renderer.fillRect(x, y + 4, 6, rowH - 10, true);
    const int tx = x + 16;
    const int tw = inner - 16 - lengthW - 12;
    renderer.drawText(NOTOSANS_12_FONT_ID, tx, y,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, item.title.c_str(), tw, kBold).c_str(), true, kBold);
    renderer.drawText(NOTOSANS_12_FONT_ID, tx, y + 24,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, item.artist.c_str(), tw).c_str(), true);
    renderer.drawText(NOTOSANS_12_FONT_ID, x + inner - lengthW, y, length, true);
    y += rowH;
  }
  if (queue_.items.empty()) lab::paragraph(screen, x, y, inner, "THE SERVER GAVE AN EMPTY QUEUE.", 2);
  lab::button(screen, lab::rect(x, pageBottom() - buttonH, inner, buttonH), "BACK TO THE MUSIC", ActionBackToPlayer, 0,
              true, false);
}

void JukeboxActivity::buildMenu(toybox::Screen& screen) {
  lab::chrome(screen, "JUKEBOX");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 14;
  auto row = [&](const char* name, const char* text) {
    lab::caption(screen, x, y, inner, name);
    y += toybox::kTileCut.inkHeight + 6;
    renderer.drawText(NOTOSANS_14_FONT_ID, x, y, renderer.truncatedText(NOTOSANS_14_FONT_ID, text, inner).c_str(),
                      true);
    y += 50;
  };
  if (config_.hasPlayer()) {
    char where[48];
    std::snprintf(where, sizeof(where), "%.24s:%d", config_.playerHost.c_str(), config_.playerPort % 100000);
    row("PLAYER", config_.playerName.c_str());
    row("ITS ADDRESS ON THIS NETWORK", where);
    // Two separate claims: that it can be read, and that it does as it is told.
    row("READING THE PLAYER", monitorWorks_ ? "Working" : "No answer");
    row("CONTROLLING IT", now_.can == 0 && monitorWorks_ ? "The player offers no controls"
                          : control_ == Control::Works   ? "Working"
                          : control_ == Control::Failed  ? "The last command was refused"
                                                         : "Not tried yet");
  } else {
    row("PLAYER", "None chosen");
  }
  row("PLEX ACCOUNT", config_.signedIn() ? "Linked" : "Not linked");
  if (jukebox::net::kDemo) lab::caption(screen, x, y, inner, "SIMULATOR: A MADE-UP ACCOUNT AND PLAYER");

  const int buttonH = 58;
  int by = pageBottom() - buttonH;
  lab::button(screen, lab::rect(x, by, inner, buttonH), "UNLINK AND ERASE", ActionUnlink, 0, false, false);
  by -= buttonH + 8;
  lab::button(screen, lab::rect(x, by, inner, buttonH), "CHANGE PLAYER", ActionChangePlayer, 0, false, false);
  if (config_.hasPlayer()) {
    by -= buttonH + 8;
    lab::button(screen, lab::rect(x, by, inner, buttonH), "BACK TO THE MUSIC", ActionBackToPlayer, 0, true, false);
  }
}

void JukeboxActivity::buildUnlink(toybox::Screen& screen) {
  lab::chrome(screen, "JUKEBOX");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 24;
  lab::label(screen, lab::rect(x, y, inner, toybox::kDisplayCut.inkHeight), "UNLINK?", toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Left);
  y += toybox::kDisplayCut.inkHeight + 20;
  lab::paragraph(screen, x, y, inner,
                 "THIS ERASES THE PLEX SIGN-IN, THE CHOSEN PLAYER AND THE SAVED COVER FROM THIS READER'S MEMORY CARD.",
                 4);
  y += toybox::kTileCut.lineHeight * 4 + 16;
  lab::paragraph(screen, x, y, inner,
                 "PLEX KEEPS LISTING THIS READER UNTIL YOU REMOVE IT YOURSELF: AT PLEX.TV, SETTINGS, AUTHORIZED "
                 "DEVICES, LOOK FOR CROSSPLAY JUKEBOX.",
                 5);
  const int buttonH = 60;
  int by = pageBottom() - buttonH;
  lab::button(screen, lab::rect(x, by, inner, buttonH), "KEEP IT", ActionMenu, 0, true, false);
  by -= buttonH + 8;
  lab::button(screen, lab::rect(x, by, inner, buttonH), "UNLINK AND ERASE", ActionUnlinkYes, 0, false, false);
}

void JukeboxActivity::buildNotice(toybox::Screen& screen) {
  lab::chrome(screen, "JUKEBOX");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 24;
  const bool busy = view_ == View::Busy;
  lab::label(screen, lab::rect(x, y, inner, toybox::kUiCut.inkHeight), busy ? busyHeadline_ : noticeHeadline_.c_str(),
             toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Left);
  y += toybox::kUiCut.inkHeight + 20;
  if (busy) {
    lab::caption(screen, x, y, inner, "ONE MOMENT");
    return;
  }
  lab::paragraph(screen, x, y, inner, noticeMessage_.c_str(), 8);
  lab::button(screen, lab::rect(x, pageBottom() - 60, inner, 60), "OK", ActionNoticeOk, 0, true, false);
}

void JukeboxActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  const char* hint = "Back";
  switch (view_) {
    case View::Welcome:
      buildWelcome(screen);
      break;
    case View::Linking: {
      const fui::Rect qr = buildLinking(screen);
      QrUtils::drawQrCode(renderer, Rect{qr.x, qr.y, qr.width, qr.height}, jukebox::kLinkUrl);
      break;
    }
    case View::Players:
      buildPlayers(screen);
      break;
    case View::Playing:
      buildPlaying(screen);
      break;
    case View::Showcase:
      buildShowcase(screen);
      break;
    case View::Queue:
      buildQueue(screen);
      break;
    case View::Menu:
      buildMenu(screen);
      break;
    case View::Unlink:
      buildUnlink(screen);
      break;
    case View::Notice:
    case View::Busy:
      buildNotice(screen);
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Jukebox");
  const bool playing = view_ == View::Playing;
  const auto labels = mappedInput.mapLabels(hint, playing ? "Play" : "", playing ? "Prev" : "", playing ? "Next" : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

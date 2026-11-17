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
#include "../ui/ToyboxSerif.h"
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
  ActionLook,

  ActionBrowse,
  ActionBrowseRow,   // value: the row on this page
  ActionBrowsePage,  // value: -1 or +1
  ActionBrowseUp,
  ActionStart,  // value: 1 for shuffled

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
// The playing page's three looks.
enum : uint8_t { LookClassic, LookMinimal, LookTape, LookEditorial, kLooks };
constexpr const char* kLookNames[kLooks] = {"CLASSIC", "MINIMAL", "TAPE", "EDITORIAL"};
enum : int { GlyphPrevious, GlyphPlay, GlyphPause, GlyphNext };
constexpr int kBrowseRows = 6;
constexpr int kBrowseRowH = 70;
// What a level of the library shows.
enum : uint8_t { LevelRoot, LevelRecent, LevelArtists, LevelPlaylists, LevelArtist, LevelAlbum, LevelPlaylist };

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
    if (d.server && !d.token.empty() && config_.servers.size() < 8) config_.servers.push_back({d.id, d.token, d.base});
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
  // The address the player gave for the server is used only for a server the
  // account has listed too.
  for (jukebox::Config::Server& s : config_.servers) {
    if (s.id == now_.serverId && s.base.empty()) s.base = now_.serverBase;
  }
  const int status = jukebox::net::server(config_, now_.serverId, "GET",
                                          jukebox::queueUrl(now_.serverBase, now_.queueId, kQueueWindow * 2), body);
  if (status != 200 || !jukebox::parseQueue(body.data(), body.size(), queue_)) {
    notice("NO QUEUE", "THE SERVER DID NOT GIVE THE QUEUE OVER A VERIFIED CONNECTION. THE CONTROLS STILL WORK.",
           View::Playing);
    return;
  }
  // The page that lists what is next can use this answer too.
  queueFor_ = now_.queueId + "/" + now_.trackKey;
  view_ = View::Queue;
}

// --- The library ---------------------------------------------------------------

// The server to browse: the one the music is coming from, else the first the
// account listed with an address on this network.
const jukebox::Config::Server* JukeboxActivity::browseServer() {
  bool any = false;
  for (const jukebox::Config::Server& s : config_.servers) any = any || !s.base.empty();
  if (!any) {
    // Settings from before addresses were kept: ask the account once more.
    std::string body;
    std::vector<jukebox::Device> devices;
    devices.reserve(8);
    if (jukebox::net::account(config_, "GET", jukebox::resourcesUrl(), body) == 200 &&
        jukebox::parseResources(body.data(), body.size(), devices)) {
      config_.servers.clear();
      for (const jukebox::Device& d : devices) {
        if (d.server && !d.token.empty() && config_.servers.size() < 8)
          config_.servers.push_back({d.id, d.token, d.base});
      }
      saveConfig();
    }
  }
  const jukebox::Config::Server* first = nullptr;
  for (const jukebox::Config::Server& s : config_.servers) {
    if (s.base.empty()) continue;
    if (s.id == now_.serverId) return &s;
    if (first == nullptr) first = &s;
  }
  return first;
}

void JukeboxActivity::openLevel(const uint8_t what, const std::string& key, const std::string& title, const int count) {
  Level level;
  level.what = what;
  level.key = key;
  level.title = title;
  level.count = count;
  trail_.push_back(level);
  request(Step::Browse, "READING THE LIBRARY");
}

void JukeboxActivity::doBrowse() {
  const jukebox::Config::Server* server = browseServer();
  if (server == nullptr || trail_.empty()) {
    trail_.clear();
    notice("NO SERVER", "YOUR PLEX ACCOUNT LISTED NO MEDIA SERVER WITH AN ADDRESS ON THIS NETWORK.", View::Menu);
    return;
  }
  Level& level = trail_.back();
  entries_.clear();
  entries_.reserve(32);
  entriesTotal_ = 0;
  std::string url;
  switch (level.what) {
    case LevelRoot:
      url = jukebox::librariesUrl(server->base);
      break;
    case LevelRecent:
      url = jukebox::recentUrl(server->base, level.key, level.page * kBrowseRows, kBrowseRows);
      break;
    case LevelArtists:
      url = jukebox::artistsUrl(server->base, level.key, level.page * kBrowseRows, kBrowseRows);
      break;
    case LevelPlaylists:
      url = jukebox::playlistsUrl(server->base, level.page * kBrowseRows, kBrowseRows);
      break;
    case LevelArtist:
    case LevelAlbum:
      url = jukebox::childrenUrl(server->base, level.key);
      break;
    default:
      // A playlist is a page with two buttons; there is nothing to read.
      view_ = View::Browse;
      return;
  }
  std::string body;
  const int status = jukebox::net::server(config_, server->id, "GET", url, body);
  if (status != 200 || !jukebox::parseEntries(body.data(), body.size(), entries_, entriesTotal_)) {
    trail_.pop_back();
    notice("THE LIBRARY DID NOT ANSWER", "THE MEDIA SERVER COULD NOT BE READ OVER A VERIFIED CONNECTION. IS IT ON?",
           trail_.empty() ? View::Menu : View::Browse);
    return;
  }
  if (level.what == LevelRoot) {
    // Each music library becomes two doors, and the playlists a third.
    std::vector<jukebox::Entry> doors;
    doors.reserve(entries_.size() * 2 + 1);
    for (const jukebox::Entry& library : entries_) {
      jukebox::Entry door = library;
      door.count = LevelRecent;
      door.detail = "Recently added albums";
      doors.push_back(door);
      door.count = LevelArtists;
      door.detail = "Artists";
      doors.push_back(door);
    }
    jukebox::Entry lists;
    lists.kind = jukebox::Kind::Library;
    lists.title = "Playlists";
    lists.count = LevelPlaylists;
    doors.push_back(lists);
    entries_ = doors;
    entriesTotal_ = static_cast<int>(entries_.size());
  }
  view_ = View::Browse;
}

// Asks the server for a queue and tells the player to play it.
void JukeboxActivity::doStart() {
  const jukebox::Config::Server* server = browseServer();
  if (server == nullptr) {
    notice("NO SERVER", "THE MEDIA SERVER COULD NOT BE FOUND.", View::Browse);
    return;
  }
  std::string body;
  std::string queueId;
  std::string firstKey;
  const int made = jukebox::net::server(
      config_, server->id, "POST", jukebox::makeQueueUrl(server->base, server->id, startWhat_, startShuffled_), body);
  if (made != 200 || !jukebox::parseMadeQueue(body.data(), body.size(), queueId, firstKey)) {
    notice("COULD NOT START IT", "THE MEDIA SERVER DID NOT MAKE A QUEUE FOR THAT.", View::Browse);
    return;
  }
  const std::string extra = jukebox::playMediaExtra(server->base, server->id, config_.tokenFor(server->id), queueId,
                                                    startTrack_.empty() ? firstKey : startTrack_);
  const int status = jukebox::net::player(
      config_, config_.playerHost,
      jukebox::commandUrl(config_.playerHost, config_.playerPort, "playMedia", commandId_++, extra.c_str()), body);
  control_ = status == 200 ? Control::Works : Control::Failed;
  if (status != 200) {
    notice("THE PLAYER DID NOT TAKE IT", "THE QUEUE WAS MADE BUT THE PLAYER DID NOT START IT. IS PLEXAMP OPEN?",
           View::Browse);
    return;
  }
  // The player needs a moment to fetch the queue before it says what it plays.
  delay(1200);
  poll(true);
  lastPollMs_ = ms();
  trail_.clear();
  view_ = View::Playing;
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
  // A look with no cover asks for none.
  if (coverSide() <= 0) return;
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
  // The look that lists what is next reads the queue once a track.
  if (view_ == View::Playing && config_.look % kLooks == LookEditorial && misses_ == 0 && !now_.queueId.empty() &&
      !now_.serverBase.empty() && !now_.trackKey.empty()) {
    const std::string want = now_.queueId + "/" + now_.trackKey;
    if (queueFor_ != want) {
      // Marked first: a server that does not answer is not asked again for this track.
      queueFor_ = want;
      for (jukebox::Config::Server& s : config_.servers) {
        if (s.id == now_.serverId && s.base.empty()) s.base = now_.serverBase;
      }
      std::string body;
      jukebox::Queue fresh;
      if (jukebox::net::server(config_, now_.serverId, "GET",
                               jukebox::queueUrl(now_.serverBase, now_.queueId, kQueueWindow * 2), body) == 200 &&
          jukebox::parseQueue(body.data(), body.size(), fresh)) {
        queue_ = fresh;
      } else {
        queue_ = jukebox::Queue{};
      }
      requestUpdate();
      return;
    }
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
    case ActionLook:
      config_.look = static_cast<uint8_t>((config_.look + 1) % kLooks);
      saveConfig();
      // The record's label is a smaller cover than the others'.
      artWanted_ = !now_.thumb.empty() && coverKey() != artFor_;
      requestUpdate();
      break;
    case ActionMenu:
      view_ = View::Menu;
      requestUpdate();
      break;
    case ActionQueue:
      request(Step::Queue, "READING THE QUEUE");
      break;
    case ActionBrowse:
      trail_.clear();
      openLevel(LevelRoot, "", "Library", 0);
      break;
    case ActionBrowseUp:
      if (trail_.size() <= 1) {
        trail_.clear();
        view_ = config_.hasPlayer() ? View::Playing : View::Menu;
        requestUpdate();
      } else {
        trail_.pop_back();
        request(Step::Browse, "READING THE LIBRARY");
      }
      break;
    case ActionBrowsePage:
      if (!trail_.empty()) {
        Level& level = trail_.back();
        const bool local = level.what == LevelRoot || level.what == LevelArtist || level.what == LevelAlbum;
        const int pages = (entriesTotal_ + kBrowseRows - 1) / kBrowseRows;
        const int to = level.page + value;
        if (to < 0 || to >= pages) break;
        level.page = to;
        if (local) {
          requestUpdate();
        } else {
          request(Step::Browse, "READING THE LIBRARY");
        }
      }
      break;
    case ActionBrowseRow:
      if (!trail_.empty()) {
        const Level level = trail_.back();
        const bool local = level.what == LevelRoot || level.what == LevelArtist || level.what == LevelAlbum;
        const int index = (local ? level.page * kBrowseRows : 0) + value;
        if (index < 0 || index >= static_cast<int>(entries_.size())) break;
        const jukebox::Entry entry = entries_[static_cast<size_t>(index)];
        if (level.what == LevelRoot) {
          openLevel(static_cast<uint8_t>(entry.count), entry.key,
                    entry.detail.empty() ? entry.title : entry.title + ": " + entry.detail, 0);
        } else if (entry.kind == jukebox::Kind::Artist) {
          openLevel(LevelArtist, entry.key, entry.title, 0);
        } else if (entry.kind == jukebox::Kind::Album) {
          openLevel(LevelAlbum, entry.key, entry.title, entry.count);
        } else if (entry.kind == jukebox::Kind::Playlist) {
          openLevel(LevelPlaylist, entry.key, entry.title, entry.count);
        } else if (entry.kind == jukebox::Kind::Track && level.what == LevelAlbum) {
          // The album, from this track on.
          startWhat_ = jukebox::Entry{};
          startWhat_.kind = jukebox::Kind::Album;
          startWhat_.key = level.key;
          startTrack_ = entry.key;
          startShuffled_ = false;
          request(Step::Start, "STARTING IT");
        }
      }
      break;
    case ActionStart:
      if (!trail_.empty() && config_.hasPlayer()) {
        const Level& level = trail_.back();
        startWhat_ = jukebox::Entry{};
        startWhat_.kind = level.what == LevelPlaylist ? jukebox::Kind::Playlist : jukebox::Kind::Album;
        startWhat_.key = level.key;
        startTrack_.clear();
        startShuffled_ = value != 0;
        request(Step::Start, "STARTING IT");
      }
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
      case Step::Browse:
        doBrowse();
        break;
      case Step::Start:
        doStart();
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
      case View::Browse:
        act(ActionBrowseUp, 0);
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
    // The volume bar: a touch along it is a volume.
    if (view_ == View::Playing && volumeW_ > 0 && tapY >= volumeY_ - 6 && tapY <= volumeY_ + volumeH_ + 6 &&
        tapX >= volumeX_ - 10 && tapX <= volumeX_ + volumeW_ + 10) {
      int to = (tapX - volumeX_) * 100 / volumeW_;
      to = to < 0 ? 0 : to > 100 ? 100 : to;
      char extra[24];
      std::snprintf(extra, sizeof(extra), "&volume=%d", to % 1000);
      command("setParameters", extra);
      return;
    }
    // What is next, on the look that lists it: a touch jumps there.
    if (view_ == View::Playing && nextRows_ > 0 && tapY >= nextY_ && (now_.can & jukebox::CanPlayPause)) {
      const int row = (tapY - nextY_) / 58;
      const int index = nextFirst_ + row;
      if (row < nextRows_ && index < static_cast<int>(queue_.items.size())) {
        command("skipTo", jukebox::skipToExtra(queue_.items[static_cast<size_t>(index)].key).c_str());
        poll(true);
        return;
      }
    }
    // The head of the playing page: the next look.
    if (view_ == View::Playing && tapY < kCoverTop - 4) {
      act(ActionLook, 0);
      return;
    }
    // A row of the queue: the player jumps to it.
    if (view_ == View::Queue && queueRows_ > 0 && tapY >= queueRowsY_ && (now_.can & jukebox::CanPlayPause)) {
      const int row = (tapY - queueRowsY_) / 52;
      const int index = queueFirst_ + row;
      if (row < queueRows_ && index < static_cast<int>(queue_.items.size()) && index != queue_.current) {
        view_ = View::Playing;
        command("skipTo", jukebox::skipToExtra(queue_.items[static_cast<size_t>(index)].key).c_str());
        poll(true);
        return;
      }
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

// The cover as this look draws it, which is the size the server is asked for:
// 0 for a look with no cover at all.
int JukeboxActivity::coverSide() const {
  const int inner = renderer.getScreenWidth() - 2 * toybox::kMargin;
  if (view_ == View::Showcase) return std::min(inner, pageBottom() - kShowcaseText - kShowcaseTop);
  switch (config_.look % kLooks) {
    case LookMinimal:
    case LookTape:
      return 0;
    case LookEditorial:
      return 208;
    default: {
      const int room = pageBottom() - kControlsHeight - kTrackHeight - 10 - kCoverTop;
      return std::max(120, std::min(inner, room));
    }
  }
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

// --- The playing page, in four looks --------------------------------------------
//
// Each look is its own page, not a skin over one: they share what the player
// reports and the pieces below, and nothing of their layout.

// A place to touch with nothing drawn for it.
void JukeboxActivity::zone(toybox::Screen& screen, const int x, const int y, const int w, const int h,
                           const fui::ActionId action, const int value) {
  fui::ButtonProps props;
  props.label = "";
  props.action = action;
  props.value = static_cast<int16_t>(value);
  props.styles.explicitlySet = true;
  props.minTouchSize = 0;
  screen.button(props, lab::rect(x, y, w, h));
}

// Previous, play, pause or next, centred on (cx, cy) and `r` tall each way.
void JukeboxActivity::glyph(const int cx, const int cy, const int r, const int kind, const bool black) {
  auto triangle = [&](const int left, const int dir) {
    const int xs[3] = {left, left, left + dir * (r + r / 2)};
    const int ys[3] = {cy - r, cy + r, cy};
    renderer.fillPolygon(xs, ys, 3, black);
  };
  const int bar = std::max(4, r / 3);
  if (kind == GlyphPrevious) {
    renderer.fillRect(cx - r - bar, cy - r, bar, 2 * r, black);
    triangle(cx + r, -1);
  } else if (kind == GlyphPlay) {
    triangle(cx - r / 2 - 2, 1);
  } else if (kind == GlyphPause) {
    const int w = std::max(6, r * 3 / 5);
    renderer.fillRect(cx - w - w / 3, cy - r, w, 2 * r, black);
    renderer.fillRect(cx + w / 3, cy - r, w, 2 * r, black);
  } else {
    triangle(cx - r, 1);
    renderer.fillRect(cx + r, cy - r, bar, 2 * r, black);
  }
}

// What the player takes right now: nothing while it is not answering.
uint16_t JukeboxActivity::canNow() const { return misses_ >= kQuietAfter ? 0 : now_.can; }
bool JukeboxActivity::idleNow() const { return now_.state == State::Stopped || now_.trackKey.empty(); }

const char* JukeboxActivity::stateWord() const {
  if (misses_ >= kQuietAfter) return "NO ANSWER";
  switch (now_.state) {
    case State::Playing:
      return "PLAYING";
    case State::Paused:
      return "PAUSED";
    case State::Buffering:
      return "LOADING";
    default:
      return "IDLE";
  }
}

// The line across the top of every look: which player, what it is doing. A
// touch on it is the next look.
void JukeboxActivity::headLine(toybox::Screen& screen) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  lab::caption(screen, x, 16, inner, stateWord(), fui::TextAlign::Right);
  const std::string player =
      renderer.truncatedText(NOTOSANS_12_FONT_ID, config_.playerName.c_str(), inner - 130, kBold);
  renderer.drawText(NOTOSANS_12_FONT_ID, x, 10, player.c_str(), true, kBold);
  if (jukebox::net::kDemo) lab::caption(screen, x, 16, inner, "DEMO DATA", fui::TextAlign::Center);
}

// The volume as a bar to touch anywhere along, with a knob. `steps` adds the
// minus and plus at its ends.
void JukeboxActivity::volumeBar(toybox::Screen& screen, const int x, const int y, const int w, const int h,
                                const bool steps, const bool slim) {
  volumeW_ = 0;
  if (!(canNow() & jukebox::CanVolume) || now_.volume < 0) return;
  int left = x;
  int width = w;
  if (steps) {
    const int step = 62;
    lab::button(screen, lab::rect(x, y, step, h), "-", ActionVolume, -kVolumeStep, false, false);
    lab::button(screen, lab::rect(x + w - step, y, step, h), "+", ActionVolume, kVolumeStep, false, false);
    left += step + 14;
    width -= 2 * step + 28;
  }
  const int numberW = 62;
  volumeX_ = static_cast<int16_t>(left);
  volumeW_ = static_cast<int16_t>(width - numberW - 8);
  volumeY_ = static_cast<int16_t>(y);
  volumeH_ = static_cast<int16_t>(h);
  const int cy = y + h / 2;
  const int travel = volumeW_ - 8;
  const int fill = travel * std::min(100, now_.volume) / 100;
  if (slim) {
    renderer.fillRect(volumeX_, cy - 1, volumeW_, 2, true);
    renderer.fillRect(volumeX_, cy - 3, fill + 4, 6, true);
  } else {
    renderer.drawRoundedRect(volumeX_, cy - 9, volumeW_, 18, 2, 9, true);
    if (fill > 0) renderer.fillRoundedRect(volumeX_ + 4, cy - 5, std::max(fill, 10), 10, 5, Color::Black);
  }
  fillDisc(volumeX_ + 4 + fill, cy, slim ? 10 : 13, true);
  fillDisc(volumeX_ + 4 + fill, cy, slim ? 5 : 8, false);
  char volume[toybox::kIntChars + 1];
  std::snprintf(volume, sizeof(volume), "%d", now_.volume % 1000);
  const int numberX = volumeX_ + volumeW_ + 14;
  renderer.drawText(NOTOSANS_14_FONT_ID,
                    numberX + (numberW - renderer.getTextWidth(NOTOSANS_14_FONT_ID, volume, kBold)) / 2,
                    cy - renderer.getLineHeight(NOTOSANS_14_FONT_ID) / 2, volume, true, kBold);
}

// Where the track is, with its two times, and the bar a touch seeks along.
void JukeboxActivity::progressBar(const int x, const int y, const int w, const bool slim) {
  barW_ = 0;
  if (idleNow()) return;
  char at[16];
  char total[16];
  const int32_t position = positionNow();
  jukebox::formatTime(position, at, sizeof(at));
  jukebox::formatTime(now_.durationMs, total, sizeof(total));
  if (slim) {
    // A hairline the whole width, the times under its ends.
    barX_ = static_cast<int16_t>(x);
    barW_ = static_cast<int16_t>(w);
    barY_ = static_cast<int16_t>(y);
    renderer.fillRect(x, y + 5, w, 2, true);
    if (now_.durationMs > 0) {
      const int fill = static_cast<int>(static_cast<long>(w) * position / now_.durationMs);
      renderer.fillRect(x, y + 2, fill, 8, true);
    }
    renderer.drawText(NOTOSANS_12_FONT_ID, x, y + 18, at, true);
    renderer.drawText(NOTOSANS_12_FONT_ID, x + w - renderer.getTextWidth(NOTOSANS_12_FONT_ID, total), y + 18, total,
                      true);
    return;
  }
  const int timeW = 64;
  barX_ = static_cast<int16_t>(x + timeW);
  barW_ = static_cast<int16_t>(w - 2 * timeW);
  barY_ = static_cast<int16_t>(y + 8);
  renderer.drawText(NOTOSANS_12_FONT_ID, x, y, at, true);
  renderer.drawText(NOTOSANS_12_FONT_ID, x + w - renderer.getTextWidth(NOTOSANS_12_FONT_ID, total), y, total, true);
  renderer.drawRoundedRect(barX_, barY_, barW_, 12, 2, 6, true);
  if (now_.durationMs > 0) {
    const int fill = static_cast<int>(static_cast<long>(barW_ - 6) * position / now_.durationMs);
    if (fill > 0) renderer.fillRect(barX_ + 3, barY_ + 3, fill, 6, true);
  }
}

// Shuffle, repeat, the queue and the menu, as four buttons along the foot.
void JukeboxActivity::smallRow(toybox::Screen& screen, const int y, const int h) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  const uint16_t can = canNow();
  const int gap = kRowGap;
  const int cell = (inner - 3 * gap) / 4;
  if ((can & jukebox::CanShuffle) && now_.shuffle >= 0) {
    lab::button(screen, lab::rect(x, y, cell, h), "SHUFFLE", ActionShuffle, 0, now_.shuffle != 0, true);
  }
  if ((can & jukebox::CanRepeat) && now_.repeat >= 0) {
    lab::button(screen, lab::rect(x + cell + gap, y, cell, h),
                now_.repeat == 1   ? "REPEAT 1"
                : now_.repeat == 2 ? "REP. ALL"
                                   : "REPEAT",
                ActionRepeat, 0, now_.repeat != 0, true);
  }
  // The queue lives on the server the track came from; without one there is none to show.
  if (can != 0 && !now_.queueId.empty() && !now_.serverBase.empty()) {
    lab::button(screen, lab::rect(x + 2 * (cell + gap), y, cell, h), "QUEUE", ActionQueue, 0, false, true);
  }
  lab::button(screen, lab::rect(x + 3 * (cell + gap), y, cell, h), "MENU", ActionMenu, 0, false, true);
}

// What a look draws where its transport would be when there is nothing to
// drive. True when it drew, and the transport should not.
bool JukeboxActivity::idleOrSilent(toybox::Screen& screen, const int y, const int h) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  const bool silent = misses_ >= kQuietAfter;
  if (idleNow() && !silent) {
    lab::button(screen, lab::rect(x, y + (h - 80) / 2, inner, 80), "BROWSE THE LIBRARY", ActionBrowse, 0, true, false);
    return true;
  }
  if (canNow() == 0) {
    lab::caption(screen, x, y + h / 2 - 8, inner,
                 silent ? "WAITING FOR THE PLAYER TO ANSWER" : "THIS PLAYER OFFERS NO CONTROLS RIGHT NOW",
                 fui::TextAlign::Center);
    return true;
  }
  return false;
}

void JukeboxActivity::buildPlaying(toybox::Screen& screen) {
  coverW_ = 0;
  barW_ = 0;
  volumeW_ = 0;
  nextRows_ = 0;
  switch (config_.look % kLooks) {
    case LookMinimal:
      buildMinimal(screen);
      break;
    case LookTape:
      buildTape(screen);
      break;
    case LookEditorial:
      buildEditorial(screen);
      break;
    default:
      buildClassic(screen);
      break;
  }
  shownKey_ = now_.trackKey;
  shownState_ = now_.state;
  shownVolume_ = now_.volume;
  shownShuffle_ = now_.shuffle;
  shownRepeat_ = now_.repeat;
  shownSilent_ = misses_ >= kQuietAfter;
  lastPaintMs_ = ms();
}

// CLASSIC: the cover, the track under it, and three rows of boxed controls.
void JukeboxActivity::buildClassic(toybox::Screen& screen) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  const bool silent = misses_ >= kQuietAfter;
  const bool idle = idleNow();
  const int foot = pageBottom();
  const int gap = kRowGap;
  const int smallY = foot - kSmallRow;
  const int volumeY = smallY - gap - kVolumeRow;
  const int transportY = volumeY - gap - kTransportRow;
  const int barY = transportY - kProgressRow;
  const int textY = barY - kTrackHeight;

  headLine(screen);
  const int side = coverSide();
  const int artX = x + (inner - side) / 2;
  coverX_ = static_cast<int16_t>(artX);
  coverY_ = static_cast<int16_t>(kCoverTop);
  coverW_ = static_cast<int16_t>(side);
  if (idle || !drawCover(artX, kCoverTop, side)) {
    renderer.drawRoundedRect(artX, kCoverTop, side, side, 2, 10, true);
    lab::label(screen, lab::rect(artX, kCoverTop + side / 2 - 40, side, toybox::kUiCut.inkHeight),
               silent && idle ? "PLAYER NOT ANSWERING"
               : idle         ? "NOTHING PLAYING"
                              : "NO COVER",
               toybox::kUiFont, toybox::kUiCut);
    if (idle) {
      lab::caption(screen, artX, kCoverTop + side / 2 + 10, side,
                   silent ? "IS PLEXAMP OPEN, ON THIS NETWORK?" : "START SOMETHING IN PLEXAMP OR BELOW",
                   fui::TextAlign::Center);
    }
  }
  if (!idle) {
    auto line = [&](const int font, const std::string& text, const int ly, const EpdFontFamily::Style style) {
      renderer.drawText(font, x, ly,
                        renderer.truncatedText(font, jukebox::drawable(text).c_str(), inner, style).c_str(), true,
                        style);
    };
    line(NOTOSANS_16_FONT_ID, now_.title, textY, kBold);
    line(NOTOSANS_14_FONT_ID, now_.artist, textY + 40, EpdFontFamily::REGULAR);
    line(NOTOSANS_12_FONT_ID, now_.year.empty() ? now_.album : now_.album + "  \xC2\xB7  " + now_.year, textY + 72,
         EpdFontFamily::REGULAR);
  }
  progressBar(x, barY, inner, false);

  if (!idleOrSilent(screen, transportY, kTransportRow)) {
    const uint16_t can = canNow();
    const int third = (inner - 2 * gap) / 3;
    auto key = [&](const int slot, const fui::ActionId action, const int kind) {
      const int bx = x + slot * (third + gap);
      lab::button(screen, lab::rect(bx, transportY, third, kTransportRow), "", action, 0, false, false);
      glyph(bx + third / 2, transportY + kTransportRow / 2, 18, kind, true);
    };
    if (can & jukebox::CanPrevious) key(0, ActionPrevious, GlyphPrevious);
    if (can & jukebox::CanPlayPause) key(1, ActionPlayPause, now_.state == State::Playing ? GlyphPause : GlyphPlay);
    if (can & jukebox::CanNext) key(2, ActionNext, GlyphNext);
  }
  volumeBar(screen, x, volumeY, inner, kVolumeRow, true, false);
  smallRow(screen, smallY, kSmallRow);
}

// MINIMAL: no cover and no boxes. The title set large, a hairline for the
// track, bare marks for the controls, words along the foot.
void JukeboxActivity::buildMinimal(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int x = 36;
  const int inner = width - 2 * x;
  const int foot = pageBottom();
  const bool idle = idleNow();
  const bool silent = misses_ >= kQuietAfter;
  headLine(screen);

  // The words, hung from a short heavy rule.
  int y = 120;
  renderer.fillRect(x, y, 56, 8, true);
  y += 34;
  if (idle) {
    renderer.drawText(NOTOSERIF_18_FONT_ID, x, y, silent ? "No answer" : "Silence", true);
    y += renderer.getLineHeight(NOTOSERIF_18_FONT_ID) + 10;
    renderer.drawText(NOTOSANS_12_FONT_ID, x, y,
                      silent ? "Is Plexamp open, on this network?" : "Nothing is playing on this player.", true);
  } else {
    std::vector<std::string> lines;
    lines.reserve(4);
    toyboxserif::wrap(renderer, NOTOSERIF_18_FONT_ID, jukebox::drawable(now_.title).c_str(), inner, 3, lines);
    const int lineH = renderer.getLineHeight(NOTOSERIF_18_FONT_ID);
    for (const std::string& text : lines) {
      renderer.drawText(NOTOSERIF_18_FONT_ID, x, y, text.c_str(), true);
      y += lineH;
    }
    y += 16;
    renderer.drawText(
        NOTOSANS_14_FONT_ID, x, y,
        renderer.truncatedText(NOTOSANS_14_FONT_ID, jukebox::drawable(now_.artist).c_str(), inner).c_str(), true,
        kBold);
    y += 38;
    const std::string album = now_.year.empty() ? now_.album : now_.album + ", " + now_.year;
    renderer.drawText(NOTOSANS_12_FONT_ID, x, y,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(album).c_str(), inner).c_str(),
                      true);
  }

  // From the foot up: the words, the volume, the marks, the hairline.
  const int wordsY = foot - 34;
  const int volumeY = wordsY - 74;
  const int marksY = volumeY - 110;
  const int lineY = marksY - 84;
  progressBar(x, lineY, inner, true);

  if (!idleOrSilent(screen, marksY, 90)) {
    const uint16_t can = canNow();
    const int cy = marksY + 45;
    const int third = inner / 3;
    if (can & jukebox::CanPrevious) {
      glyph(x + third / 2, cy, 15, GlyphPrevious, true);
      zone(screen, x, marksY, third, 90, ActionPrevious, 0);
    }
    if (can & jukebox::CanPlayPause) {
      glyph(x + third + third / 2, cy, 26, now_.state == State::Playing ? GlyphPause : GlyphPlay, true);
      zone(screen, x + third, marksY, third, 90, ActionPlayPause, 0);
    }
    if (can & jukebox::CanNext) {
      glyph(x + 2 * third + third / 2, cy, 15, GlyphNext, true);
      zone(screen, x + 2 * third, marksY, third, 90, ActionNext, 0);
    }
  }
  volumeBar(screen, x, volumeY, inner, 54, false, true);

  // Words for the rest, the ones that are on underlined.
  const uint16_t can = canNow();
  struct Word {
    const char* text;
    fui::ActionId action;
    bool shown;
    bool on;
  };
  const Word words[4] = {
      {"SHUFFLE", ActionShuffle, (can & jukebox::CanShuffle) && now_.shuffle >= 0, now_.shuffle > 0},
      {now_.repeat == 1 ? "REPEAT 1" : "REPEAT", ActionRepeat, (can & jukebox::CanRepeat) && now_.repeat >= 0,
       now_.repeat > 0},
      {"QUEUE", ActionQueue, can != 0 && !now_.queueId.empty() && !now_.serverBase.empty(), false},
      {"MENU", ActionMenu, true, false},
  };
  const int cell = inner / 4;
  for (int i = 0; i < 4; ++i) {
    if (!words[i].shown) continue;
    const int cx = x + i * cell;
    const fui::TextAlign align = i == 0   ? fui::TextAlign::Left
                                 : i == 3 ? fui::TextAlign::Right
                                          : fui::TextAlign::Center;
    lab::caption(screen, cx, wordsY + 8, cell, words[i].text, align);
    if (words[i].on) {
      const int w = renderer.getTextWidth(toybox::kTileFontId, words[i].text);
      const int ux = i == 0 ? cx : i == 3 ? cx + cell - w : cx + (cell - w) / 2;
      renderer.fillRect(ux, wordsY + 8 + toybox::kTileCut.inkHeight + 4, w, 3, true);
    }
    zone(screen, cx, wordsY - 10, cell, 44, words[i].action, 0);
  }
}

// TAPE: a cassette whose reels show how far through the track is, a counter,
// the keys of a deck and a fader.
void JukeboxActivity::buildTape(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - 2 * toybox::kMargin;
  const int foot = pageBottom();
  const bool idle = idleNow();
  const bool silent = misses_ >= kQuietAfter;
  headLine(screen);

  // The shell.
  // From the foot up: the small row, the fader, the keys, the counter. The
  // cassette sits in the middle of what is left above them.
  const int smallY = foot - kSmallRow;
  const int faderY = smallY - 12 - 58;
  const int keysY = faderY - 16 - 104;
  const int counterY = keysY - 52;
  const int cx0 = 20;
  const int cw = width - 40;
  const int ch = 286;
  const int cy0 = 44 + std::max(0, (counterY - 12 - 44 - ch) / 2);
  renderer.drawRoundedRect(cx0, cy0, cw, ch, 4, 18, true);
  for (const int sx : {cx0 + 18, cx0 + cw - 18}) {
    for (const int sy : {cy0 + 18, cy0 + ch - 18}) {
      fillDisc(sx, sy, 6, true);
      renderer.fillRect(sx - 4, sy - 1, 8, 2, false);
    }
  }
  // The label: ruled lines with the track written on them.
  const int lx = cx0 + 36;
  const int ly = cy0 + 16;
  const int lw = cw - 72;
  const int lh = 126;
  renderer.drawRoundedRect(lx, ly, lw, lh, 2, 8, true);
  renderer.fillRect(lx, ly, 44, lh, true);
  renderer.drawText(toybox::kDisplayFontId, lx + 10, ly + 30, "A", false);
  const int tx = lx + 58;
  const int tw = lw - 70;
  for (int rule = 0; rule < 3; ++rule) renderer.fillRect(tx, ly + 42 + rule * 34, tw, 1, true);
  if (idle) {
    renderer.drawText(NOTOSANS_14_FONT_ID, tx, ly + 6, silent ? "No answer" : "Nothing playing", true, kBold);
  } else {
    renderer.drawText(
        NOTOSANS_14_FONT_ID, tx, ly + 6,
        renderer.truncatedText(NOTOSANS_14_FONT_ID, jukebox::drawable(now_.title).c_str(), tw, kBold).c_str(), true,
        kBold);
    renderer.drawText(NOTOSANS_12_FONT_ID, tx, ly + 48,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(now_.artist).c_str(), tw).c_str(),
                      true);
    const std::string album = now_.year.empty() ? now_.album : now_.album + ", " + now_.year;
    renderer.drawText(NOTOSANS_12_FONT_ID, tx, ly + 82,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(album).c_str(), tw).c_str(), true);
  }
  // The window and its two reels: the tape goes from the left one to the right.
  const int wx = cx0 + 84;
  const int wy = ly + lh + 14;
  const int ww = cw - 168;
  const int wh = 92;
  renderer.drawRoundedRect(wx, wy, ww, wh, 3, 14, true);
  const int32_t position = positionNow();
  const int through =
      now_.durationMs > 0 && !idle ? static_cast<int>(static_cast<long>(100) * position / now_.durationMs) : 0;
  const int reelY = wy + wh / 2;
  const int reels[2] = {wx + 58, wx + ww - 58};
  const int packs[2] = {20 + (100 - through) * 22 / 100, 20 + through * 22 / 100};
  for (int i = 0; i < 2; ++i) {
    fillDisc(reels[i], reelY, packs[i], true);
    fillDisc(reels[i], reelY, 15, false);
    ringAt(reels[i], reelY, 15, true);
    // The hub's teeth.
    for (int tooth = 0; tooth < 6; ++tooth) {
      const float a = static_cast<float>(tooth) * 1.0471976f + static_cast<float>(through) * 0.2f;
      renderer.fillRect(reels[i] + static_cast<int>(std::lround(std::cos(a) * 10.0f)) - 2,
                        reelY + static_cast<int>(std::lround(std::sin(a) * 10.0f)) - 2, 4, 4, true);
    }
  }
  renderer.fillRect(reels[0] + packs[0], reelY + 30, reels[1] - packs[1] - reels[0] - packs[0], 2, true);
  // The foot of the shell, where the heads reach in.
  const int fy = cy0 + ch - 34;
  const int xs[4] = {cx0 + 96, cx0 + cw - 96, cx0 + cw - 70, cx0 + 70};
  const int ys[4] = {fy, fy, cy0 + ch - 3, cy0 + ch - 3};
  for (int i = 0; i < 4; ++i) renderer.drawLine(xs[i], ys[i], xs[(i + 1) % 4], ys[(i + 1) % 4], 3, true);
  for (const int hole : {cx0 + 130, cx0 + cw / 2, cx0 + cw - 130}) fillDisc(hole, fy + 16, 5, true);

  // The counter: where the tape is, as a deck would show it.
  progressBar(x, counterY + 6, inner, false);

  if (!idleOrSilent(screen, keysY, 104)) {
    // Deck keys: tall, with a heavy lower edge, the middle one widest.
    const uint16_t can = canNow();
    const int gap = 10;
    const int side = (inner - 2 * gap) * 3 / 10;
    const int middle = inner - 2 * gap - 2 * side;
    auto key = [&](const int kx, const int kw, const fui::ActionId action, const int kind, const int r) {
      renderer.fillRoundedRect(kx, keysY + 8, kw, 96, 10, Color::Black);
      renderer.fillRoundedRect(kx, keysY, kw, 92, 10, Color::White);
      renderer.drawRoundedRect(kx, keysY, kw, 92, 3, 10, true);
      glyph(kx + kw / 2, keysY + 46, r, kind, true);
      zone(screen, kx, keysY, kw, 104, action, 0);
    };
    if (can & jukebox::CanPrevious) key(x, side, ActionPrevious, GlyphPrevious, 16);
    if (can & jukebox::CanPlayPause) {
      key(x + side + gap, middle, ActionPlayPause, now_.state == State::Playing ? GlyphPause : GlyphPlay, 24);
    }
    if (can & jukebox::CanNext) key(x + side + gap + middle + gap, side, ActionNext, GlyphNext, 16);
  }

  // The fader, with its scale.
  volumeBar(screen, x + 4, faderY + 14, inner - 8, 44, false, false);
  if (volumeW_ > 0) {
    for (int tick = 0; tick <= 10; ++tick) {
      const int tx2 = volumeX_ + 4 + (volumeW_ - 8) * tick / 10;
      renderer.fillRect(tx2, faderY + 2, tick % 5 == 0 ? 3 : 1, tick % 5 == 0 ? 12 : 8, true);
    }
  }
  smallRow(screen, smallY, kSmallRow);
}

// EDITORIAL: a page laid out like a magazine's. The cover and the track side
// by side, round controls, and what comes next set as a list below.
void JukeboxActivity::buildEditorial(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - 2 * toybox::kMargin;
  const int foot = pageBottom();
  const bool idle = idleNow();
  const bool silent = misses_ >= kQuietAfter;
  headLine(screen);

  // The cover on the left, the words beside it.
  const int side = coverSide();
  const int top = 48;
  coverX_ = static_cast<int16_t>(x);
  coverY_ = static_cast<int16_t>(top);
  coverW_ = static_cast<int16_t>(side);
  if (idle || !drawCover(x, top, side)) renderer.drawRect(x, top, side, side, 2, true);
  const int tx = x + side + 18;
  const int tw = width - toybox::kMargin - tx;
  int y = top;
  renderer.fillRect(tx, y, tw, 6, true);
  y += 16;
  lab::caption(screen, tx, y, tw, idle ? "NOTHING PLAYING" : "NOW PLAYING");
  y += toybox::kTileCut.inkHeight + 12;
  if (idle) {
    renderer.drawText(NOTOSANS_12_FONT_ID, tx, y, silent ? "No answer." : "Start something", true);
    if (!silent) renderer.drawText(NOTOSANS_12_FONT_ID, tx, y + 26, "below or in Plexamp.", true);
  } else {
    std::vector<std::string> lines;
    lines.reserve(4);
    toyboxserif::wrap(renderer, NOTOSANS_14_FONT_ID, jukebox::drawable(now_.title).c_str(), tw, 3, lines);
    const int lineH = renderer.getLineHeight(NOTOSANS_14_FONT_ID);
    for (const std::string& text : lines) {
      renderer.drawText(NOTOSANS_14_FONT_ID, tx, y, text.c_str(), true, kBold);
      y += lineH;
    }
    y += 6;
    renderer.drawText(NOTOSANS_12_FONT_ID, tx, y,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(now_.artist).c_str(), tw).c_str(),
                      true);
    y += 28;
    const std::string album = now_.year.empty() ? now_.album : now_.album + ", " + now_.year;
    if (y + 24 <= top + side) {
      renderer.drawText(NOTOSANS_12_FONT_ID, tx, y,
                        renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(album).c_str(), tw).c_str(),
                        true);
    }
  }

  const int barY = top + side + 16;
  progressBar(x, barY, inner, false);

  // Round controls: the one in the middle solid.
  const int roundY = barY + 44;
  const int roundH = 108;
  if (!idleOrSilent(screen, roundY, roundH)) {
    const uint16_t can = canNow();
    const int cy = roundY + roundH / 2;
    const int centres[3] = {width / 2 - 130, width / 2, width / 2 + 130};
    if (can & jukebox::CanPrevious) {
      fillDisc(centres[0], cy, 40, true);
      fillDisc(centres[0], cy, 36, false);
      glyph(centres[0] - 2, cy, 14, GlyphPrevious, true);
      zone(screen, centres[0] - 60, roundY, 120, roundH, ActionPrevious, 0);
    }
    if (can & jukebox::CanPlayPause) {
      fillDisc(centres[1], cy, 52, true);
      glyph(centres[1] + (now_.state == State::Playing ? 0 : 4), cy, 22,
            now_.state == State::Playing ? GlyphPause : GlyphPlay, false);
      zone(screen, centres[1] - 64, roundY, 128, roundH, ActionPlayPause, 0);
    }
    if (can & jukebox::CanNext) {
      fillDisc(centres[2], cy, 40, true);
      fillDisc(centres[2], cy, 36, false);
      glyph(centres[2] + 2, cy, 14, GlyphNext, true);
      zone(screen, centres[2] - 60, roundY, 120, roundH, ActionNext, 0);
    }
  }
  const int volumeY = roundY + roundH + 8;
  volumeBar(screen, x, volumeY, inner, 50, true, false);

  // Up next: a black band and the tracks that follow, each a touch away.
  const int smallY = foot - kSmallRow;
  const int bandY = volumeY + 50 + 14;
  renderer.fillRect(0, bandY, width, 34, true);
  lab::caption(screen, x, bandY + 10, inner, "UP NEXT", fui::TextAlign::Left, fui::Color::White);
  const int rowH = 58;
  const int listY = bandY + 34 + 10;
  const int room = (smallY - 8 - listY) / rowH;
  const bool haveQueue = queueFor_ == now_.queueId + "/" + now_.trackKey && queue_.current >= 0;
  nextFirst_ = queue_.current + 1;
  nextY_ = static_cast<int16_t>(listY);
  const int after = haveQueue ? static_cast<int>(queue_.items.size()) - nextFirst_ : 0;
  nextRows_ = static_cast<int16_t>(std::max(0, std::min(room, after)));
  if (haveQueue && queue_.total > 0) {
    char place[toybox::kIntChars * 2 + 8];
    std::snprintf(place, sizeof(place), "%d TRACKS", queue_.total % 100000);
    lab::caption(screen, x, bandY + 10, inner, place, fui::TextAlign::Right, fui::Color::White);
  }
  for (int row = 0; row < nextRows_; ++row) {
    const jukebox::QueueItem& item = queue_.items[static_cast<size_t>(nextFirst_ + row)];
    const int ry = listY + row * rowH;
    char length[16];
    jukebox::formatTime(item.durationMs, length, sizeof(length));
    const int lengthW = renderer.getTextWidth(NOTOSANS_12_FONT_ID, length);
    char number[toybox::kIntChars + 1];
    std::snprintf(number, sizeof(number), "%d", (row + 1) % 100);
    renderer.drawText(NOTOSANS_14_FONT_ID, x, ry + 4, number, true, kBold);
    const int nx = x + 34;
    const int nw = inner - 34 - lengthW - 12;
    renderer.drawText(
        NOTOSANS_12_FONT_ID, nx, ry,
        renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(item.title).c_str(), nw, kBold).c_str(), true,
        kBold);
    renderer.drawText(NOTOSANS_12_FONT_ID, nx, ry + 26,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(item.artist).c_str(), nw).c_str(),
                      true);
    renderer.drawText(NOTOSANS_12_FONT_ID, x + inner - lengthW, ry, length, true);
    if (row + 1 < nextRows_) renderer.fillRect(nx, ry + rowH - 1, inner - 34, 1, true);
  }
  if (nextRows_ == 0) {
    lab::caption(screen, x, listY + 10, inner,
                 idle        ? ""
                 : haveQueue ? "THAT IS THE LAST TRACK OF THE QUEUE"
                             : "THE QUEUE WILL SHOW HERE");
  }
  smallRow(screen, smallY, kSmallRow);
}

void JukeboxActivity::fillDisc(const int cx, const int cy, const int radius, const bool black) {
  for (int dy = -radius; dy <= radius; ++dy) {
    const int half = static_cast<int>(std::lround(std::sqrt(static_cast<float>(radius * radius - dy * dy))));
    renderer.fillRect(cx - half, cy + dy, half * 2 + 1, 1, black);
  }
}

// A thin circle, for a record's grooves.
void JukeboxActivity::ringAt(const int cx, const int cy, const int radius, const bool black) {
  int px = radius;
  int py = 0;
  int err = 1 - radius;
  while (px >= py) {
    const int xs[8] = {px, py, -py, -px, -px, -py, py, px};
    const int ys[8] = {py, px, px, py, -py, -px, -px, -py};
    for (int i = 0; i < 8; ++i) renderer.fillRect(cx + xs[i], cy + ys[i], 1, 1, black);
    ++py;
    if (err < 0) {
      err += 2 * py + 1;
    } else {
      --px;
      err += 2 * (py - px) + 1;
    }
  }
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

// What comes next, as the server has it. A touch on a row jumps to it.
void JukeboxActivity::buildQueue(toybox::Screen& screen) {
  lab::chrome(screen, "QUEUE");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 10;
  char count[toybox::kIntChars * 2 + 40];
  std::snprintf(count, sizeof(count), "%d TRACKS. TOUCH ONE TO JUMP TO IT", queue_.total % 100000);
  lab::caption(screen, x, y, inner, count);
  y += toybox::kTileCut.inkHeight + 10;
  const int rowH = 52;
  const int buttonH = 56;
  const int room = (pageBottom() - buttonH - 10 - y) / rowH;
  // The playing track a third of the way down, so most of the page is what follows.
  int first = queue_.current > 2 ? queue_.current - 2 : 0;
  const int count2 = static_cast<int>(queue_.items.size());
  if (first + room > count2) first = std::max(0, count2 - room);
  queueFirst_ = first;
  queueRowsY_ = static_cast<int16_t>(y);
  queueRows_ = static_cast<int16_t>(std::min(room, count2 - first));
  for (int i = first; i < count2 && i < first + room; ++i) {
    const jukebox::QueueItem& item = queue_.items[static_cast<size_t>(i)];
    const bool current = i == queue_.current;
    char length[16];
    jukebox::formatTime(item.durationMs, length, sizeof(length));
    const int lengthW = renderer.getTextWidth(NOTOSANS_12_FONT_ID, length);
    if (current) renderer.fillRect(x, y + 4, 6, rowH - 10, true);
    const int tx = x + 16;
    const int tw = inner - 16 - lengthW - 12;
    renderer.drawText(
        NOTOSANS_12_FONT_ID, tx, y,
        renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(item.title).c_str(), tw, kBold).c_str(), true,
        kBold);
    renderer.drawText(NOTOSANS_12_FONT_ID, tx, y + 24,
                      renderer.truncatedText(NOTOSANS_12_FONT_ID, jukebox::drawable(item.artist).c_str(), tw).c_str(),
                      true);
    renderer.drawText(NOTOSANS_12_FONT_ID, x + inner - lengthW, y, length, true);
    y += rowH;
  }
  if (queue_.items.empty()) lab::paragraph(screen, x, y, inner, "THE SERVER GAVE AN EMPTY QUEUE.", 2);
  lab::button(screen, lab::rect(x, pageBottom() - buttonH, inner, buttonH), "BACK TO THE MUSIC", ActionBackToPlayer, 0,
              true, false);
}

// One level of the library: a list to step into, or an album or playlist to start.
void JukeboxActivity::buildBrowse(toybox::Screen& screen) {
  lab::chrome(screen, "LIBRARY");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 8;
  if (trail_.empty()) return;
  const Level& level = trail_.back();
  const bool local = level.what == LevelRoot || level.what == LevelArtist || level.what == LevelAlbum;
  const std::string title =
      renderer.truncatedText(NOTOSANS_14_FONT_ID, jukebox::drawable(level.title).c_str(), inner, kBold);
  renderer.drawText(NOTOSANS_14_FONT_ID, x, y, title.c_str(), true, kBold);
  y += 40;

  const bool startable = level.what == LevelAlbum || level.what == LevelPlaylist;
  if (startable) {
    const int half = (inner - 8) / 2;
    lab::button(screen, lab::rect(x, y, half, 64), "PLAY", ActionStart, 0, true, false);
    lab::button(screen, lab::rect(x + half + 8, y, half, 64), "SHUFFLE", ActionStart, 1, false, false);
    y += 64 + 12;
    if (level.what == LevelPlaylist) {
      char count[toybox::kIntChars + 24];
      std::snprintf(count, sizeof(count), "%d TRACKS", level.count % 1000000);
      lab::caption(screen, x, y, inner, count);
    }
  }

  const int buttonH = 56;
  const int footY = pageBottom() - buttonH;
  const int rows = std::min(kBrowseRows, (footY - 8 - y) / kBrowseRowH);
  const int first = local ? level.page * kBrowseRows : 0;
  for (int row = 0; row < rows && first + row < static_cast<int>(entries_.size()); ++row) {
    const jukebox::Entry& entry = entries_[static_cast<size_t>(first + row)];
    lab::button(screen, lab::rect(x, y, inner, kBrowseRowH - 6), "", ActionBrowseRow, row, false, false);
    std::string name = jukebox::drawable(entry.title);
    if (name.empty()) name = entry.kind == jukebox::Kind::Track ? "Untitled track" : "Untitled";
    std::string detail = jukebox::drawable(entry.detail);
    if (entry.kind == jukebox::Kind::Playlist && entry.count > 0) detail = std::to_string(entry.count) + " tracks";
    const int detailW =
        entry.kind == jukebox::Kind::Track ? renderer.getTextWidth(NOTOSANS_12_FONT_ID, detail.c_str()) : 0;
    const bool twoLines = !detail.empty() && entry.kind != jukebox::Kind::Track;
    renderer.drawText(
        NOTOSANS_12_FONT_ID, x + 14, y + (twoLines ? 3 : 18),
        renderer.truncatedText(NOTOSANS_12_FONT_ID, name.c_str(), inner - 28 - detailW - 10, kBold).c_str(), true,
        kBold);
    if (twoLines) {
      renderer.drawText(NOTOSANS_12_FONT_ID, x + 14, y + 31,
                        renderer.truncatedText(NOTOSANS_12_FONT_ID, detail.c_str(), inner - 28).c_str(), true);
    } else if (detailW > 0) {
      renderer.drawText(NOTOSANS_12_FONT_ID, x + inner - 14 - detailW, y + 18, detail.c_str(), true);
    }
    y += kBrowseRowH;
  }
  if (entries_.empty() && level.what != LevelPlaylist) lab::caption(screen, x, y + 10, inner, "NOTHING HERE");

  // The foot: up a level, and the pages when there is more than one.
  const int pages = (entriesTotal_ + kBrowseRows - 1) / kBrowseRows;
  const int third = (inner - 16) / 3;
  lab::button(screen, lab::rect(x, footY, third, buttonH), "BACK", ActionBrowseUp, 0, false, false);
  if (pages > 1) {
    if (level.page > 0) {
      lab::button(screen, lab::rect(x + third + 8, footY, third, buttonH), "<", ActionBrowsePage, -1, false, false);
    }
    if (level.page + 1 < pages) {
      lab::button(screen, lab::rect(x + 2 * (third + 8), footY, third, buttonH), ">", ActionBrowsePage, 1, false,
                  false);
    }
    char page[toybox::kIntChars * 2 + 8];
    std::snprintf(page, sizeof(page), "%d / %d", (level.page + 1) % 100000, pages % 100000);
    lab::caption(screen, x, footY - 22, inner, page, fui::TextAlign::Right);
  }
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
  if (config_.hasPlayer()) {
    char look[40];
    std::snprintf(look, sizeof(look), "LOOK: %s", kLookNames[config_.look % kLooks]);
    lab::button(screen, lab::rect(x, y, inner, 52), look, ActionLook, 0, false, true);
    y += 60;
  }
  if (jukebox::net::kDemo) lab::caption(screen, x, y, inner, "SIMULATOR: A MADE-UP ACCOUNT AND PLAYER");

  const int buttonH = 58;
  int by = pageBottom() - buttonH;
  lab::button(screen, lab::rect(x, by, inner, buttonH), "UNLINK AND ERASE", ActionUnlink, 0, false, false);
  by -= buttonH + 8;
  lab::button(screen, lab::rect(x, by, inner, buttonH), "CHANGE PLAYER", ActionChangePlayer, 0, false, false);
  if (config_.hasPlayer()) {
    by -= buttonH + 8;
    lab::button(screen, lab::rect(x, by, inner, buttonH), "BROWSE THE LIBRARY", ActionBrowse, 0, false, false);
  }
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
    case View::Browse:
      buildBrowse(screen);
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

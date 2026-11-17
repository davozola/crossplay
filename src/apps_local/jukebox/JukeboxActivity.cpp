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
enum : uint8_t { LookClassic, LookRecord, LookPoster, kLooks };
constexpr const char* kLookNames[kLooks] = {"CLASSIC", "RECORD", "POSTER"};
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
    if (view_ == View::Playing && volumeW_ > 0 && tapY >= volumeY_ && tapY <= volumeY_ + kVolumeRow &&
        tapX >= volumeX_ - 10 && tapX <= volumeX_ + volumeW_ + 10) {
      int to = (tapX - volumeX_) * 100 / volumeW_;
      to = to < 0 ? 0 : to > 100 ? 100 : to;
      char extra[24];
      std::snprintf(extra, sizeof(extra), "&volume=%d", to % 1000);
      command("setParameters", extra);
      return;
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

// The square the cover has on the playing page: what is left between the
// head and the track's name.
int JukeboxActivity::coverArea() const {
  const int inner = renderer.getScreenWidth() - 2 * toybox::kMargin;
  const int room = pageBottom() - kControlsHeight - kTrackHeight - 10 - kCoverTop;
  return std::max(120, std::min(inner, room));
}

// The cover itself, which is what the server is asked for: the whole square,
// or the label in the middle of the record.
int JukeboxActivity::coverSide() const {
  const int inner = renderer.getScreenWidth() - 2 * toybox::kMargin;
  if (view_ == View::Showcase) return std::min(inner, pageBottom() - kShowcaseText - kShowcaseTop);
  const int area = coverArea();
  return config_.look % kLooks == LookRecord ? (area * 46 / 100) & ~1 : area;
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
  const uint8_t look = config_.look % kLooks;
  // POSTER is the page in negative: white on black, the buttons white tiles.
  const bool dark = look == LookPoster;
  const bool ink = !dark;
  if (dark) renderer.fillRect(0, 0, width, renderer.getScreenHeight(), true);

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

  auto caption = [&](const int cx, const int cy, const int w, const char* text, const fui::TextAlign align) {
    lab::caption(screen, cx, cy, w, text, align, ink ? fui::Color::Black : fui::Color::White);
  };

  // The head: which player, and what it is doing. A touch on it changes the look.
  const int headY = 10;
  const char* state = silent                           ? "NO ANSWER"
                      : now_.state == State::Playing   ? "PLAYING"
                      : now_.state == State::Paused    ? "PAUSED"
                      : now_.state == State::Buffering ? "LOADING"
                                                       : "IDLE";
  caption(x, headY + 6, inner, state, fui::TextAlign::Right);
  const std::string player =
      renderer.truncatedText(NOTOSANS_12_FONT_ID, config_.playerName.c_str(), inner - 130, kBold);
  renderer.drawText(NOTOSANS_12_FONT_ID, x, headY, player.c_str(), ink, kBold);
  if (jukebox::net::kDemo) caption(x, headY + 6, inner, "DEMO DATA", fui::TextAlign::Center);

  // The cover, in this look's way.
  const int artTop = kCoverTop;
  const int area = coverArea();
  const int side = coverSide();
  const int areaX = x + (inner - area) / 2;
  const int artX = areaX + (area - side) / 2;
  const int artY = artTop + (area - side) / 2;
  coverX_ = static_cast<int16_t>(areaX);
  coverY_ = static_cast<int16_t>(artTop);
  coverW_ = static_cast<int16_t>(area);
  bool drawn = false;
  if (look == LookRecord) {
    // A record: the black disc, its grooves, and the cover as the label.
    const int cx = areaX + area / 2;
    const int cy = artTop + area / 2;
    const int radius = area / 2;
    fillDisc(cx, cy, radius, true);
    for (int groove = radius - 10; groove > side / 2 + 12; groove -= 14) ringAt(cx, cy, groove, false);
    fillDisc(cx, cy, side / 2 + 6, false);
    drawn = !idle && drawCover(artX, artY, side);
    // Everything of the square label outside its circle goes back to black.
    const int label = side / 2;
    for (int dy = -label - 6; dy <= label + 6; ++dy) {
      const int inside = dy < -label || dy > label
                             ? 0
                             : static_cast<int>(std::lround(std::sqrt(static_cast<float>(label * label - dy * dy))));
      const int reach = label + 6;
      renderer.fillRect(cx - reach, cy + dy, reach - inside, 1, true);
      renderer.fillRect(cx + inside, cy + dy, reach - inside + 1, 1, true);
    }
    ringAt(cx, cy, label, false);
    fillDisc(cx, cy, 7, true);
    fillDisc(cx, cy, 4, false);
    if (!drawn && !idle) drawn = true;  // a blank label is still a record
  } else {
    // A cover is black on white, so on the black page it gets its white first.
    if (dark && !idle && artFor_ != 0 && artFor_ == coverKey()) renderer.fillRect(artX, artY, side, side, false);
    drawn = !idle && drawCover(artX, artY, side);
  }
  if (!drawn || (idle && look == LookRecord)) {
    if (look != LookRecord) renderer.drawRoundedRect(areaX, artTop, area, area, 2, 10, ink);
    const char* head = silent && idle ? "PLAYER NOT ANSWERING" : idle ? "NOTHING PLAYING" : "NO COVER";
    const bool onDisc = look == LookRecord;
    if (onDisc) renderer.fillRect(areaX + 30, artTop + area / 2 - 52, area - 60, 104, false);
    lab::label(screen, lab::rect(areaX, artTop + area / 2 - 40, area, toybox::kUiCut.inkHeight), head, toybox::kUiFont,
               toybox::kUiCut, fui::TextAlign::Center, (ink || onDisc) ? fui::Color::Black : fui::Color::White);
    if (idle) {
      lab::caption(screen, areaX, artTop + area / 2 + 10, area,
                   silent ? "IS PLEXAMP OPEN, ON THIS NETWORK?" : "START SOMETHING IN PLEXAMP OR BELOW",
                   fui::TextAlign::Center, (ink || onDisc) ? fui::Color::Black : fui::Color::White);
    }
  }

  // The track.
  if (!idle) {
    const bool centred = look != LookClassic;
    auto line = [&](const int font, const std::string& text, const int ly, const EpdFontFamily::Style style) {
      const std::string fitted = renderer.truncatedText(font, jukebox::drawable(text).c_str(), inner, style);
      const int lx = centred ? x + (inner - renderer.getTextWidth(font, fitted.c_str(), style)) / 2 : x;
      renderer.drawText(font, lx, ly, fitted.c_str(), ink, style);
    };
    line(NOTOSANS_16_FONT_ID, now_.title, textY, kBold);
    line(NOTOSANS_14_FONT_ID, now_.artist, textY + 40, EpdFontFamily::REGULAR);
    line(NOTOSANS_12_FONT_ID, now_.year.empty() ? now_.album : now_.album + "  \xC2\xB7  " + now_.year, textY + 72,
         EpdFontFamily::REGULAR);

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
    renderer.drawText(NOTOSANS_12_FONT_ID, x, barY, at, ink);
    renderer.drawText(NOTOSANS_12_FONT_ID, x + inner - renderer.getTextWidth(NOTOSANS_12_FONT_ID, total), barY, total,
                      ink);
    renderer.drawRoundedRect(barX_, barY_, barW_, 12, 2, 6, ink);
    if (now_.durationMs > 0) {
      const int fill = static_cast<int>(static_cast<long>(barW_ - 6) * position / now_.durationMs);
      if (fill > 0) renderer.fillRect(barX_ + 3, barY_ + 3, fill, 6, ink);
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
  if (idle && !silent) {
    lab::button(screen, lab::rect(x, transportY, inner, transportH), "BROWSE THE LIBRARY", ActionBrowse, 0, true,
                false);
  } else if (can == 0) {
    caption(x, transportY + transportH / 2 - 8, inner,
            silent ? "WAITING FOR THE PLAYER TO ANSWER" : "THIS PLAYER OFFERS NO CONTROLS RIGHT NOW",
            fui::TextAlign::Center);
  }

  // The volume: a bar to touch anywhere along, and a step either side of it.
  volumeW_ = 0;
  if ((can & jukebox::CanVolume) && now_.volume >= 0) {
    const int step = 62;
    lab::button(screen, lab::rect(x, volumeY, step, volumeH), "-", ActionVolume, -kVolumeStep, false, false);
    lab::button(screen, lab::rect(x + inner - step, volumeY, step, volumeH), "+", ActionVolume, kVolumeStep, false,
                false);
    const int numberW = 70;
    volumeX_ = static_cast<int16_t>(x + step + 14);
    volumeW_ = static_cast<int16_t>(inner - 2 * step - 28 - numberW);
    volumeY_ = static_cast<int16_t>(volumeY);
    const int trackY = volumeY + volumeH / 2 - 9;
    renderer.drawRoundedRect(volumeX_, trackY, volumeW_, 18, 2, 9, ink);
    const int fill = (volumeW_ - 8) * std::min(100, now_.volume) / 100;
    if (fill > 0)
      renderer.fillRoundedRect(volumeX_ + 4, trackY + 4, std::max(fill, 10), 10, 5, ink ? Color::Black : Color::White);
    // The knob, so it reads as something to move.
    const int knobX = volumeX_ + 4 + fill;
    fillDisc(knobX, trackY + 9, 13, ink);
    fillDisc(knobX, trackY + 9, 8, !ink);
    char volume[toybox::kIntChars + 1];
    std::snprintf(volume, sizeof(volume), "%d", now_.volume % 1000);
    const int numberX = volumeX_ + volumeW_ + 16;
    renderer.drawText(NOTOSANS_14_FONT_ID,
                      numberX + (numberW - renderer.getTextWidth(NOTOSANS_14_FONT_ID, volume, kBold)) / 2,
                      volumeY + volumeH / 2 - renderer.getLineHeight(NOTOSANS_14_FONT_ID) / 2, volume, ink, kBold);
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

#pragma once

// PLEXAMP JUKEBOX, the part with no device in it: what Plex's and Plexamp's
// answers mean, the addresses to ask them at, and the settings worth keeping.
//
// The device half (JukeboxNet, JukeboxActivity) only moves bytes and draws.
// Everything here runs in host-tests/jukebox against answers captured from a
// real Plexamp, so the firmware's reading of the protocol is checked without a
// reader on the desk.
//
// The protocol in one paragraph. plex.tv signs a device in with a short code
// typed on another screen, and lists the account's servers and players with
// the address each can be reached at. A Plexamp player with Remote Control on
// answers on its own port: /player/timeline/poll says what it is doing, and
// /player/playback/<command> tells it what to do. Nothing goes through the
// media server except the album art.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jukebox {

constexpr char kProduct[] = "CrossPlay Jukebox";
constexpr char kVersion[] = "1";
constexpr char kLinkUrl[] = "https://plex.tv/link";
constexpr int kPlayerPort = 32500;

// --- Signing in ---------------------------------------------------------------

struct Pin {
  long id = 0;
  std::string code;   // what the person types at plex.tv/link
  std::string token;  // empty until they have
};
std::string pinCreateUrl();
std::string pinPollUrl(long id);
bool parsePin(const char* json, size_t len, Pin& out);

// --- The account's servers and players ---------------------------------------

struct Device {
  std::string name;
  std::string product;
  std::string platform;
  std::string id;     // its clientIdentifier
  std::string token;  // a server's own token; empty for a player
  bool server = false;
  bool player = false;
  // A player: where it listens, when it gave a local address.
  std::string host;
  int port = 0;
};
std::string resourcesUrl();
// Appends the servers and players of the answer. False when it is not one.
bool parseResources(const char* json, size_t len, std::vector<Device>& out);

// --- What a player is doing ----------------------------------------------------

enum class State : uint8_t { Stopped, Paused, Playing, Buffering };

// What the player says it will take, from its own `controllable` list. A
// control it does not name is not drawn.
enum Can : uint16_t {
  CanPlayPause = 1 << 0,
  CanNext = 1 << 1,
  CanPrevious = 1 << 2,
  CanSeek = 1 << 3,
  CanVolume = 1 << 4,
  CanShuffle = 1 << 5,
  CanRepeat = 1 << 6,
  CanStop = 1 << 7,
};

struct Now {
  bool valid = false;  // the answer was a timeline at all
  State state = State::Stopped;
  int32_t timeMs = 0;
  int32_t durationMs = 0;
  int volume = -1;  // 0..100, -1 when the player does not say
  int shuffle = -1;
  int repeat = -1;  // 0 off, 1 one, 2 all
  uint16_t can = 0;
  std::string trackKey;  // identifies the track; a change means a new one
  std::string queueId;   // the play queue it is working through, on the server
  std::string title;
  std::string artist;
  std::string album;
  std::string year;
  std::string thumb;  // a path on the server the track came from
  // That server, as the player reaches it.
  std::string serverId;
  std::string serverBase;  // "https://host:32400"
};

std::string timelineUrl(const std::string& host, int port, uint32_t commandId, bool withMetadata);
// `extra` is appended as given: "&volume=40".
std::string commandUrl(const std::string& host, int port, const char* command, uint32_t commandId,
                       const char* extra = "");
// The music timeline of a poll's answer. With `keepTrack`, an answer that
// carries no metadata leaves the track's own fields as they were.
bool parseTimeline(const char* xml, size_t len, Now& out, bool keepTrack = false);
// --- The queue, which the server keeps ------------------------------------------

struct QueueItem {
  std::string key;  // the track's own, as Now::trackKey
  std::string title;
  std::string artist;
  int32_t durationMs = 0;
};
struct Queue {
  std::vector<QueueItem> items;  // a window around the current track
  int current = -1;              // an index into items, -1 when it is not among them
  int total = 0;                 // the whole queue's length
};
// `window` tracks either side of the one playing.
std::string queueUrl(const std::string& serverBase, const std::string& queueId, int window);
bool parseQueue(const char* json, size_t len, Queue& out);

// The server's own resizer, so the reader never holds a full-size cover.
// A player's own account of itself, from its /resources: its name and its
// identifier. For a player reached by a typed address, which plex.tv never
// introduced.
std::string playerInfoUrl(const std::string& host, int port);
bool parsePlayerInfo(const char* xml, size_t len, std::string& name, std::string& id, std::string& product);
std::string artUrl(const std::string& serverBase, const std::string& thumb, int size);

// --- Settings ------------------------------------------------------------------

struct Config {
  std::string token;     // the account's
  std::string clientId;  // this reader's, made once
  std::string playerName;
  std::string playerId;
  std::string playerHost;
  int playerPort = kPlayerPort;
  // Servers the account can reach, for the token their art needs.
  struct Server {
    std::string id;
    std::string token;
  };
  std::vector<Server> servers;

  bool signedIn() const { return !token.empty(); }
  bool hasPlayer() const { return !playerHost.empty(); }
  const std::string& tokenFor(const std::string& serverId) const;
};
std::string pack(const Config& config);
void unpack(const std::string& text, Config& out);

// --- Small things --------------------------------------------------------------

// "3:07", or "1:02:45" past the hour.
void formatTime(int32_t ms, char* out, size_t cap);
// A stable name for a cover on the card, from the server and the path.
uint32_t artHash(const std::string& serverId, const std::string& thumb);
// "192.168.1.20" or "192.168.1.20:32500" into its parts; false for nonsense.
bool parseAddress(const std::string& typed, std::string& host, int& port);
// An IPv4 address that cannot leave the local network: 10/8, 172.16/12,
// 192.168/16 and link-local. A player is only ever spoken to at one of these,
// because that conversation is plain HTTP.
bool isLocalAddress(const std::string& host);

}  // namespace jukebox

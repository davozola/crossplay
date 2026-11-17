#include "JukeboxCore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../json/FlatJson.h"

namespace jukebox {

namespace {

using flatjson::member;
using flatjson::Span;

std::string text(const Span& v) {
  if (!v.ok() || *v.p != '"') return std::string();
  // Plex's strings are short; anything longer than this is cut, not trusted.
  char buf[320];
  flatjson::string(v, buf, sizeof(buf));
  return std::string(buf);
}

bool truth(const Span& v) { return v.ok() && v.end - v.p >= 4 && std::strncmp(v.p, "true", 4) == 0; }

long whole(const Span& v) {
  double d = 0;
  return flatjson::numberD(v, d) ? static_cast<long>(d) : 0;
}

bool contains(const std::string& list, const char* word) {
  const size_t n = std::strlen(word);
  size_t at = 0;
  while (at <= list.size()) {
    size_t end = list.find(',', at);
    if (end == std::string::npos) end = list.size();
    if (end - at == n && list.compare(at, n, word) == 0) return true;
    at = end + 1;
  }
  return false;
}

// --- A reader for the little XML a player speaks: tags and their attributes. ---

struct Tag {
  const char* p = nullptr;    // just past the name
  const char* end = nullptr;  // the closing '>'
  bool ok() const { return p != nullptr; }
};

// The next <name ...> at or after `from`. Quoted '>' does not end a tag.
Tag findTag(const char* from, const char* end, const char* name) {
  const size_t n = std::strlen(name);
  for (const char* p = from; p + n + 1 < end; ++p) {
    if (*p != '<' || std::strncmp(p + 1, name, n) != 0) continue;
    const char after = p[1 + n];
    if (after != ' ' && after != '>' && after != '/') continue;
    Tag tag;
    tag.p = p + 1 + n;
    char quote = 0;
    for (const char* q = tag.p; q < end; ++q) {
      if (quote) {
        if (*q == quote) quote = 0;
      } else if (*q == '"' || *q == '\'') {
        quote = *q;
      } else if (*q == '>') {
        tag.end = q;
        return tag;
      }
    }
    return Tag{};
  }
  return Tag{};
}

void appendUtf8(std::string& out, unsigned long cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

std::string unescape(const char* p, const char* end) {
  std::string out;
  out.reserve(static_cast<size_t>(end - p));
  while (p < end) {
    if (*p != '&') {
      out.push_back(*p++);
      continue;
    }
    const char* semi = static_cast<const char*>(std::memchr(p, ';', static_cast<size_t>(end - p)));
    if (semi == nullptr || semi - p > 10) {
      out.push_back(*p++);
      continue;
    }
    const std::string name(p + 1, semi);
    if (name == "amp") {
      out.push_back('&');
    } else if (name == "lt") {
      out.push_back('<');
    } else if (name == "gt") {
      out.push_back('>');
    } else if (name == "quot") {
      out.push_back('"');
    } else if (name == "apos") {
      out.push_back('\'');
    } else if (name.size() > 1 && name[0] == '#') {
      const bool hex = name[1] == 'x' || name[1] == 'X';
      appendUtf8(out, std::strtoul(name.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10));
    } else {
      out.append(p, semi + 1);
    }
    p = semi + 1;
  }
  return out;
}

// An attribute's value, unescaped. False when the tag does not carry it.
bool attr(const Tag& tag, const char* name, std::string& out) {
  if (!tag.ok()) return false;
  const size_t n = std::strlen(name);
  const char* p = tag.p;
  while (p < tag.end) {
    while (p < tag.end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '/')) ++p;
    const char* key = p;
    while (p < tag.end && *p != '=' && *p != ' ') ++p;
    const char* keyEnd = p;
    if (p >= tag.end || *p != '=') continue;
    ++p;
    if (p >= tag.end || (*p != '"' && *p != '\'')) continue;
    const char quote = *p++;
    const char* value = p;
    while (p < tag.end && *p != quote) ++p;
    if (static_cast<size_t>(keyEnd - key) == n && std::strncmp(key, name, n) == 0) {
      out = unescape(value, p);
      return true;
    }
    if (p < tag.end) ++p;
  }
  return false;
}

std::string attrOr(const Tag& tag, const char* name, const char* fallback = "") {
  std::string value;
  return attr(tag, name, value) ? value : std::string(fallback);
}

int attrInt(const Tag& tag, const char* name, const int fallback) {
  std::string value;
  if (!attr(tag, name, value) || value.empty()) return fallback;
  return static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
}

std::string percentEncode(const std::string& in) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(in.size() * 3);
  for (const unsigned char c : in) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(kHex[c >> 4]);
      out.push_back(kHex[c & 15]);
    }
  }
  return out;
}

}  // namespace

// --- Signing in ---------------------------------------------------------------

// strong=false is what asks for the short code a person can type.
std::string pinCreateUrl() { return "https://plex.tv/api/v2/pins?strong=false"; }

std::string pinPollUrl(const long id) {
  char url[64];
  std::snprintf(url, sizeof(url), "https://plex.tv/api/v2/pins/%ld", id);
  return url;
}

bool parsePin(const char* json, const size_t len, Pin& out) {
  const Span root = flatjson::root(json, len);
  if (!root.ok() || *root.p != '{') return false;
  out.id = whole(member(root, "id"));
  out.code = text(member(root, "code"));
  out.token = text(member(root, "authToken"));
  return out.id != 0 && !out.code.empty();
}

// --- Servers and players -------------------------------------------------------

std::string resourcesUrl() { return "https://plex.tv/api/v2/resources?includeHttps=1"; }

bool parseResources(const char* json, const size_t len, std::vector<Device>& out) {
  // The answer is an array, which flatjson::root does not take.
  Span root;
  root.p = flatjson::skipSpace(json, json + len);
  root.end = json + len;
  if (root.p >= root.end || *root.p != '[') return false;
  flatjson::forEach(root, 64, [&](int, const Span& item) {
    const std::string provides = text(member(item, "provides"));
    Device d;
    d.server = contains(provides, "server");
    d.player = contains(provides, "player");
    if (!d.server && !d.player) return;
    d.name = text(member(item, "name"));
    d.product = text(member(item, "product"));
    d.platform = text(member(item, "platform"));
    d.id = text(member(item, "clientIdentifier"));
    if (d.server) {
      d.token = text(member(item, "accessToken"));
      // Where it answers: its address on this network for choice, else any
      // direct one. A relay is the last resort and is not used.
      std::string direct;
      flatjson::forEach(member(item, "connections"), 8, [&](int, const Span& c) {
        if (text(member(c, "protocol")) != "https" || truth(member(c, "relay"))) return;
        const std::string uri = text(member(c, "uri"));
        if (truth(member(c, "local")) && d.base.empty()) d.base = uri;
        if (direct.empty()) direct = uri;
      });
      if (d.base.empty()) d.base = direct;
    }
    if (d.player) {
      // A player is only of use at an address on this network.
      flatjson::forEach(member(item, "connections"), 8, [&](int, const Span& c) {
        if (!d.host.empty() || !truth(member(c, "local"))) return;
        if (text(member(c, "protocol")) != "http") return;
        const std::string address = text(member(c, "address"));
        if (!isLocalAddress(address)) return;
        d.host = address;
        d.port = static_cast<int>(whole(member(c, "port")));
      });
    }
    if (!d.id.empty()) out.push_back(d);
  });
  return true;
}

// --- A player's own port -------------------------------------------------------

std::string timelineUrl(const std::string& host, const int port, const uint32_t commandId, const bool withMetadata) {
  char tail[96];
  std::snprintf(tail, sizeof(tail), ":%d/player/timeline/poll?wait=0%s&commandID=%lu", port % 100000,
                withMetadata ? "&includeMetadata=1" : "", static_cast<unsigned long>(commandId));
  return "http://" + host + tail;
}

std::string commandUrl(const std::string& host, const int port, const char* command, const uint32_t commandId,
                       const char* extra) {
  char tail[64];
  std::snprintf(tail, sizeof(tail), "?type=music&commandID=%lu", static_cast<unsigned long>(commandId));
  char head[16];
  std::snprintf(head, sizeof(head), ":%d", port % 100000);
  return "http://" + host + head + "/player/playback/" + command + tail + extra;
}

bool parseTimeline(const char* xml, const size_t len, Now& out, const bool keepTrack) {
  const char* end = xml + len;
  Tag music;
  for (const char* from = xml; from < end;) {
    const Tag tag = findTag(from, end, "Timeline");
    if (!tag.ok()) break;
    if (attrOr(tag, "type") == "music") {
      music = tag;
      break;
    }
    from = tag.end;
  }
  if (!music.ok()) return false;

  Now now;
  if (keepTrack) now = out;
  now.valid = true;
  const std::string state = attrOr(music, "state", "stopped");
  now.state = state == "playing"     ? State::Playing
              : state == "paused"    ? State::Paused
              : state == "buffering" ? State::Buffering
                                     : State::Stopped;
  now.timeMs = attrInt(music, "time", 0);
  now.durationMs = attrInt(music, "duration", 0);
  now.volume = attrInt(music, "volume", -1);
  now.shuffle = attrInt(music, "shuffle", -1);
  now.repeat = attrInt(music, "repeat", -1);
  const std::string can = attrOr(music, "controllable");
  now.can = 0;
  if (contains(can, "playPause")) now.can |= CanPlayPause;
  if (contains(can, "skipNext")) now.can |= CanNext;
  if (contains(can, "skipPrevious")) now.can |= CanPrevious;
  if (contains(can, "seekTo")) now.can |= CanSeek;
  if (contains(can, "volume")) now.can |= CanVolume;
  if (contains(can, "shuffle")) now.can |= CanShuffle;
  if (contains(can, "repeat")) now.can |= CanRepeat;
  if (contains(can, "stop")) now.can |= CanStop;

  const std::string key = attrOr(music, "ratingKey");
  // Without metadata the answer still names the track; a different one drops
  // what was known about the last.
  if (keepTrack && key != now.trackKey) {
    now.title.clear();
    now.artist.clear();
    now.album.clear();
    now.year.clear();
    now.thumb.clear();
  }
  now.trackKey = key;
  now.queueId = attrOr(music, "playQueueID");
  now.serverId = attrOr(music, "machineIdentifier");
  const std::string address = attrOr(music, "address");
  if (!address.empty()) {
    now.serverBase = attrOr(music, "protocol", "http") + "://" + address + ":" + attrOr(music, "port", "32400");
  } else {
    now.serverBase.clear();
  }

  // The track, when the answer carries it: it is the timeline's own child.
  const Tag track = findTag(music.end, end, "Track");
  const Tag next = findTag(music.end, end, "Timeline");
  if (track.ok() && (!next.ok() || track.p < next.p)) {
    now.title = attrOr(track, "title");
    now.artist = attrOr(track, "grandparentTitle");
    now.album = attrOr(track, "parentTitle");
    now.year = attrOr(track, "parentYear");
    now.thumb = attrOr(track, "thumb");
    if (now.thumb.empty()) now.thumb = attrOr(track, "parentThumb");
    if (now.thumb.empty()) now.thumb = attrOr(track, "grandparentThumb");
  }
  out = now;
  return true;
}

std::string playerInfoUrl(const std::string& host, const int port) {
  char tail[32];
  std::snprintf(tail, sizeof(tail), ":%d/resources", port % 100000);
  return "http://" + host + tail;
}

bool parsePlayerInfo(const char* xml, const size_t len, std::string& name, std::string& id, std::string& product) {
  const Tag tag = findTag(xml, xml + len, "Player");
  if (!tag.ok()) return false;
  name = attrOr(tag, "title");
  id = attrOr(tag, "machineIdentifier");
  product = attrOr(tag, "product");
  return !id.empty();
}

std::string queueUrl(const std::string& serverBase, const std::string& queueId, const int window) {
  char tail[48];
  std::snprintf(tail, sizeof(tail), "?window=%d&own=1", window % 1000);
  return serverBase + "/playQueues/" + queueId + tail;
}

bool parseQueue(const char* json, const size_t len, Queue& out) {
  const Span container = member(flatjson::root(json, len), "MediaContainer");
  if (!container.ok()) return false;
  out = Queue{};
  out.total = static_cast<int>(whole(member(container, "playQueueTotalCount")));
  const long selected = whole(member(container, "playQueueSelectedItemID"));
  out.items.reserve(24);
  flatjson::forEach(member(container, "Metadata"), 64, [&](int, const Span& item) {
    QueueItem q;
    // A number in some answers and a string in others.
    const Span key = member(item, "ratingKey");
    q.key = key.ok() && *key.p == '"' ? text(key) : std::to_string(whole(key));
    q.title = text(member(item, "title"));
    q.artist = text(member(item, "grandparentTitle"));
    q.durationMs = static_cast<int32_t>(whole(member(item, "duration")));
    if (selected != 0 && whole(member(item, "playQueueItemID")) == selected) {
      out.current = static_cast<int>(out.items.size());
    }
    out.items.push_back(q);
  });
  return true;
}

// --- The library ---------------------------------------------------------------

namespace {
std::string paged(const int start, const int size) {
  char tail[72];
  std::snprintf(tail, sizeof(tail), "X-Plex-Container-Start=%d&X-Plex-Container-Size=%d", start % 1000000, size % 1000);
  return tail;
}
}  // namespace

std::string librariesUrl(const std::string& serverBase) { return serverBase + "/library/sections"; }

std::string recentUrl(const std::string& serverBase, const std::string& section, const int start, const int size) {
  return serverBase + "/library/sections/" + section + "/recentlyAdded?type=9&" + paged(start, size);
}

std::string artistsUrl(const std::string& serverBase, const std::string& section, const int start, const int size) {
  return serverBase + "/library/sections/" + section + "/all?type=8&" + paged(start, size);
}

std::string playlistsUrl(const std::string& serverBase, const int start, const int size) {
  return serverBase + "/playlists?playlistType=audio&" + paged(start, size);
}

std::string childrenUrl(const std::string& serverBase, const std::string& key) {
  return serverBase + "/library/metadata/" + key + "/children";
}

bool parseEntries(const char* json, const size_t len, std::vector<Entry>& out, int& total) {
  const Span container = member(flatjson::root(json, len), "MediaContainer");
  if (!container.ok()) return false;
  const size_t before = out.size();
  flatjson::forEach(member(container, "Directory"), 32, [&](int, const Span& item) {
    if (text(member(item, "type")) != "artist") return;
    Entry e;
    e.kind = Kind::Library;
    e.key = text(member(item, "key"));
    e.title = text(member(item, "title"));
    if (!e.key.empty()) out.push_back(e);
  });
  flatjson::forEach(member(container, "Metadata"), 128, [&](int, const Span& item) {
    const std::string type = text(member(item, "type"));
    Entry e;
    if (type == "artist") {
      e.kind = Kind::Artist;
    } else if (type == "album") {
      e.kind = Kind::Album;
      e.detail = text(member(item, "parentTitle"));
      const long year = whole(member(item, "year"));
      if (year > 0) e.detail += (e.detail.empty() ? "" : "  \xC2\xB7  ") + std::to_string(year);
    } else if (type == "track") {
      e.kind = Kind::Track;
      char length[16];
      formatTime(static_cast<int32_t>(whole(member(item, "duration"))), length, sizeof(length));
      e.detail = length;
    } else if (type == "playlist") {
      e.kind = Kind::Playlist;
    } else {
      return;
    }
    const Span key = member(item, "ratingKey");
    e.key = key.ok() && *key.p == '"' ? text(key) : std::to_string(whole(key));
    e.title = text(member(item, "title"));
    e.count = static_cast<int>(whole(member(item, "leafCount")));
    if (!e.key.empty()) out.push_back(e);
  });
  const long said = whole(member(container, "totalSize"));
  total = said > 0 ? static_cast<int>(said) : static_cast<int>(out.size() - before);
  return true;
}

std::string makeQueueUrl(const std::string& serverBase, const std::string& serverId, const Entry& what,
                         const bool shuffle) {
  std::string url = serverBase + "/playQueues?type=audio&repeat=0&continuous=0&own=1&shuffle=" + (shuffle ? "1" : "0");
  if (what.kind == Kind::Playlist) return url + "&playlistID=" + what.key;
  return url +
         "&uri=" + percentEncode("server://" + serverId + "/com.plexapp.plugins.library/library/metadata/" + what.key);
}

bool parseMadeQueue(const char* json, const size_t len, std::string& queueId, std::string& firstKey) {
  const Span container = member(flatjson::root(json, len), "MediaContainer");
  if (!container.ok()) return false;
  const long id = whole(member(container, "playQueueID"));
  const Span first = member(container, "playQueueSelectedMetadataItemID");
  firstKey = first.ok() && *first.p == '"' ? text(first) : std::to_string(whole(first));
  queueId = std::to_string(id);
  return id > 0 && !firstKey.empty() && firstKey != "0";
}

std::string playMediaExtra(const std::string& serverBase, const std::string& serverId, const std::string& serverToken,
                           const std::string& queueId, const std::string& trackKey) {
  // "https://host:32400" into its three parts.
  const size_t scheme = serverBase.find("://");
  const std::string protocol = scheme == std::string::npos ? "http" : serverBase.substr(0, scheme);
  std::string host = scheme == std::string::npos ? serverBase : serverBase.substr(scheme + 3);
  std::string port = "32400";
  const size_t colon = host.rfind(':');
  if (colon != std::string::npos) {
    port = host.substr(colon + 1);
    host = host.substr(0, colon);
  }
  return "&providerIdentifier=com.plexapp.plugins.library&machineIdentifier=" + serverId + "&protocol=" + protocol +
         "&address=" + host + "&port=" + port + "&containerKey=" + percentEncode("/playQueues/" + queueId + "?own=1") +
         "&key=" + percentEncode("/library/metadata/" + trackKey) + "&offset=0&token=" + serverToken;
}

std::string skipToExtra(const std::string& trackKey) {
  return "&key=" + percentEncode("/library/metadata/" + trackKey);
}

std::string drawable(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size();) {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    const size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    // Latin with its accents is two bytes and up to U+024F; a few marks of
    // punctuation live at U+2010..U+2026. Everything else is dropped.
    bool keep = n == 1 || (n == 2 && c <= 0xC9);
    if (n == 3 && i + 2 < in.size() && c == 0xE2 && static_cast<unsigned char>(in[i + 1]) == 0x80) {
      const unsigned char last = static_cast<unsigned char>(in[i + 2]);
      keep = last >= 0x90 && last <= 0xA6;
    }
    if (keep && i + n <= in.size()) out.append(in, i, n);
    i += n;
  }
  size_t a = 0;
  while (a < out.size() && out[a] == ' ') ++a;
  return out.substr(a);
}

std::string artUrl(const std::string& serverBase, const std::string& thumb, const int size) {
  char dims[96];
  std::snprintf(dims, sizeof(dims), "/photo/:/transcode?width=%d&height=%d&minSize=1&upscale=1&url=", size % 10000,
                size % 10000);
  return serverBase + dims + percentEncode(thumb);
}

// --- Settings ------------------------------------------------------------------

const std::string& Config::tokenFor(const std::string& serverId) const {
  for (const Server& s : servers) {
    if (s.id == serverId && !s.token.empty()) return s.token;
  }
  return token;
}

// One "key=value" a line. Values are tokens, addresses and names: no newlines
// to escape, and a name that somehow had one is cut at it.
std::string pack(const Config& c) {
  auto line = [](const char* key, const std::string& value) {
    std::string clean = value.substr(0, value.find('\n'));
    return std::string(key) + "=" + clean + "\n";
  };
  std::string out = "v=1\n";
  out += line("token", c.token);
  out += line("client", c.clientId);
  out += line("player.name", c.playerName);
  out += line("player.id", c.playerId);
  out += line("player.host", c.playerHost);
  out += line("player.port", std::to_string(c.playerPort));
  out += line("look", std::to_string(c.look));
  for (const Config::Server& s : c.servers) out += line("server", s.id + " " + s.token + " " + s.base);
  return out;
}

void unpack(const std::string& text, Config& out) {
  out = Config{};
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    const std::string row = text.substr(at, end - at);
    at = end + 1;
    const size_t eq = row.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = row.substr(0, eq);
    std::string value = row.substr(eq + 1);
    if (!value.empty() && value.back() == '\r') value.pop_back();
    if (key == "token") {
      out.token = value;
    } else if (key == "client") {
      out.clientId = value;
    } else if (key == "player.name") {
      out.playerName = value;
    } else if (key == "player.id") {
      out.playerId = value;
    } else if (key == "player.host") {
      out.playerHost = value;
    } else if (key == "player.port") {
      const int port = std::atoi(value.c_str());
      out.playerPort = port > 0 && port < 65536 ? port : kPlayerPort;
    } else if (key == "look") {
      out.look = static_cast<uint8_t>(std::atoi(value.c_str()) & 0x0F);
    } else if (key == "server") {
      const size_t space = value.find(' ');
      if (space != std::string::npos && out.servers.size() < 8) {
        const size_t second = value.find(' ', space + 1);
        Config::Server server;
        server.id = value.substr(0, space);
        server.token = value.substr(space + 1, second == std::string::npos ? std::string::npos : second - space - 1);
        if (second != std::string::npos) server.base = value.substr(second + 1);
        out.servers.push_back(server);
      }
    }
  }
}

// --- Small things --------------------------------------------------------------

void formatTime(const int32_t ms, char* out, const size_t cap) {
  const long total = ms < 0 ? 0 : ms / 1000;
  const long hours = (total / 3600) % 1000;
  const int minutes = static_cast<int>((total / 60) % 60);
  const int seconds = static_cast<int>(total % 60);
  if (hours > 0) {
    std::snprintf(out, cap, "%ld:%02d:%02d", hours, minutes, seconds);
  } else {
    std::snprintf(out, cap, "%d:%02d", minutes, seconds);
  }
}

uint32_t artHash(const std::string& serverId, const std::string& thumb) {
  uint32_t h = 2166136261u;
  for (const std::string* s : {&serverId, &thumb}) {
    for (const unsigned char c : *s) {
      h ^= c;
      h *= 16777619u;
    }
    h ^= 0xFF;
    h *= 16777619u;
  }
  return h;
}

bool parseAddress(const std::string& typed, std::string& host, int& port) {
  size_t a = 0;
  size_t b = typed.size();
  while (a < b && typed[a] == ' ') ++a;
  while (b > a && typed[b - 1] == ' ') --b;
  std::string value = typed.substr(a, b - a);
  if (value.compare(0, 7, "http://") == 0) value = value.substr(7);
  const size_t slash = value.find('/');
  if (slash != std::string::npos) value = value.substr(0, slash);
  port = kPlayerPort;
  const size_t colon = value.rfind(':');
  if (colon != std::string::npos) {
    const int typedPort = std::atoi(value.c_str() + colon + 1);
    if (typedPort <= 0 || typedPort > 65535) return false;
    port = typedPort;
    value = value.substr(0, colon);
  }
  if (value.empty() || value.size() > 80) return false;
  for (const char c : value) {
    const bool fine =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
    if (!fine) return false;
  }
  host = value;
  return true;
}

bool isLocalAddress(const std::string& host) {
  int part[4] = {0, 0, 0, 0};
  int index = 0;
  int digits = 0;
  for (const char c : host) {
    if (c == '.') {
      if (digits == 0 || ++index > 3) return false;
      digits = 0;
    } else if (c >= '0' && c <= '9') {
      part[index] = part[index] * 10 + (c - '0');
      if (++digits > 3 || part[index] > 255) return false;
    } else {
      return false;
    }
  }
  if (index != 3 || digits == 0) return false;
  if (part[0] == 10) return true;
  if (part[0] == 172 && part[1] >= 16 && part[1] <= 31) return true;
  if (part[0] == 192 && part[1] == 168) return true;
  return part[0] == 169 && part[1] == 254;
}

}  // namespace jukebox

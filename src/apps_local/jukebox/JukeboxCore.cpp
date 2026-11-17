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
    if (d.server) d.token = text(member(item, "accessToken"));
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
  for (const Config::Server& s : c.servers) out += line("server", s.id + " " + s.token);
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
    } else if (key == "server") {
      const size_t space = value.find(' ');
      if (space != std::string::npos && out.servers.size() < 8) {
        out.servers.push_back({value.substr(0, space), value.substr(space + 1)});
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

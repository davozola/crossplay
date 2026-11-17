#include "JukeboxNet.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#if defined(FREEINK_NET_WOLFSSL)
#include <Arduino.h>
#include <SecureHttpClient.h>
#include <esp_random.h>

// Let's Encrypt and Google Trust Services, the same bundle the bridges verify
// against: plex.direct names are issued by the first. plex.tv's own root is in
// JukeboxRoots.h.
#include "../study/StudySyncRoots.h"
#include "JukeboxRoots.h"
#else
#include <cstdlib>
#endif

namespace jukebox::net {

namespace {

constexpr char kTag[] = "JUKE";

#if defined(FREEINK_NET_WOLFSSL)

// A newer bundle on the card wins, so a change of certificate authority at
// Plex is a file copy and not a reflash.
constexpr char kRootsOverride[] = "/.crosspoint/jukebox-roots.pem";
constexpr size_t kArtCeiling = 600 * 1024;

const char* roots() {
  static std::string fromCard;
  static bool probed = false;
  if (!probed) {
    probed = true;
    HalFile file;
    if (Storage.exists(kRootsOverride) && Storage.openFileForRead(kTag, kRootsOverride, file)) {
      const size_t size = file.size();
      if (size > 512 && size < 65536) {
        fromCard.resize(size);
        if (file.read(reinterpret_cast<uint8_t*>(fromCard.data()), size) != static_cast<int>(size) ||
            fromCard.find("-----BEGIN CERTIFICATE-----") == std::string::npos) {
          fromCard.clear();
        }
      }
      LOG_INF(kTag, "root bundle on the card: %s", fromCard.empty() ? "unusable, using the built-in one" : "in use");
    }
  }
  if (!fromCard.empty()) return fromCard.c_str();
  // One bundle, built once: wolfSSL takes concatenated PEM.
  static const std::string builtIn = std::string(kDigiCertGlobalRootG2) + study::kBridgeCaRoots;
  return builtIn.c_str();
}

bool heapTooLow() {
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t maxBlock = ESP.getMaxAllocHeap();
  if (freeHeap >= 35000 && maxBlock >= 20000) return false;
  LOG_ERR(kTag, "heap too low for TLS: free=%u block=%u", static_cast<unsigned>(freeHeap),
          static_cast<unsigned>(maxBlock));
  return true;
}

// What every Plex endpoint wants to know about who is asking. The token is
// added by the caller, and is never logged.
void identify(freeink::SecureHttpClient& http, const Config& config) {
  http.setUserAgent("CrossPlay-Jukebox/" CROSSPOINT_VERSION);
  http.addHeader("X-Plex-Product", kProduct);
  http.addHeader("X-Plex-Version", kVersion);
  http.addHeader("X-Plex-Client-Identifier", config.clientId);
  http.addHeader("X-Plex-Device-Name", "CrossPlay");
  http.addHeader("X-Plex-Platform", "CrossPlay");
}

// The path alone: a query can carry nothing secret here, but a log line that
// never holds one cannot leak one either.
std::string forLog(const std::string& url) { return url.substr(0, url.find('?')); }

#else

// --- The simulator's made-up account and player --------------------------------

struct DemoTrack {
  const char* title;
  const char* artist;
  const char* album;
  const char* year;
  int durationMs;
};
constexpr DemoTrack kDemoTracks[] = {
    {"Paper Lanterns", "The Demo Quartet", "Made-Up Sessions", "2021", 215000},
    {"Caf&#233; at the End of the Line", "Example &amp; Sons", "Placeholder Hours", "2019", 187000},
    {"A Song With a Considerably Longer Title Than Fits", "Sample Orchestra", "Fixtures, Vol. 2", "2024", 302000},
};
constexpr int kDemoTrackCount = sizeof(kDemoTracks) / sizeof(kDemoTracks[0]);

struct DemoPlayer {
  int track = 0;
  bool playing = true;
  int timeMs = 64000;
  int volume = 60;
  int shuffle = 0;
  int repeat = 0;
  int pinPolls = 0;
} demo;

int numberAfter(const std::string& url, const char* key) {
  const size_t at = url.find(key);
  return at == std::string::npos ? -1 : std::atoi(url.c_str() + at + std::strlen(key));
}

#endif

}  // namespace

#if defined(FREEINK_NET_WOLFSSL)

int account(const Config& config, const char* method, const std::string& url, std::string& body) {
  body.clear();
  if (url.compare(0, 16, "https://plex.tv/") != 0 || heapTooLow()) return 0;
  freeink::SecureHttpClient http;
  http.setCACert(roots());
  http.setTimeout(15000);
  if (!http.begin(url)) return 0;
  identify(http, config);
  http.addHeader("Accept", "application/json");
  if (!config.token.empty()) http.addHeader("X-Plex-Token", config.token);
  const int status = http.sendRequest(method, std::string());
  if (status > 0) body = http.getString();
  http.end();
  LOG_INF(kTag, "%s %s -> %d (verified TLS)", method, forLog(url).c_str(), status);
  return status > 0 ? status : 0;
}

int player(const Config& config, const std::string& host, const std::string& url, std::string& body) {
  body.clear();
  if (!isLocalAddress(host)) {
    LOG_ERR(kTag, "player address is not a local one; not asked");
    return 0;
  }
  freeink::SecureHttpClient http;
  http.setTimeout(4000);
  if (!http.begin(url)) return 0;
  identify(http, config);
  if (!config.playerId.empty()) http.addHeader("X-Plex-Target-Client-Identifier", config.playerId);
  if (!config.token.empty()) http.addHeader("X-Plex-Token", config.token);
  const int status = http.sendRequest("GET", std::string());
  if (status > 0) body = http.getString();
  http.end();
  if (status != 200) LOG_ERR(kTag, "player %s -> %d", forLog(url).c_str(), status);
  return status > 0 ? status : 0;
}

int art(const Config& config, const Now& now, const int size, const char* path) {
  if (now.thumb.empty() || now.serverBase.compare(0, 8, "https://") != 0 || heapTooLow()) return 0;
  freeink::SecureHttpClient http;
  http.setCACert(roots());
  http.setTimeout(15000);
  if (!http.begin(artUrl(now.serverBase, now.thumb, size))) return 0;
  identify(http, config);
  http.addHeader("X-Plex-Token", config.tokenFor(now.serverId));
  HalFile out;
  bool opened = false;
  bool failed = false;
  size_t written = 0;
  const int status = http.GET([&](const uint8_t* data, const size_t len) {
    if (!opened) {
      if (!Storage.openFileForWrite(kTag, path, out)) {
        failed = true;
        return false;
      }
      opened = true;
    }
    if (written + len > kArtCeiling || out.write(data, len) != len) {
      failed = true;
      return false;
    }
    written += len;
    return true;
  });
  http.end();
  if (opened) out.close();
  LOG_INF(kTag, "art -> %d, %u bytes (verified TLS)", status, static_cast<unsigned>(written));
  if (status != 200 || failed || written == 0) {
    if (opened) Storage.remove(path);
    return status == 200 ? 0 : (status > 0 ? status : 0);
  }
  return 200;
}

int server(const Config& config, const std::string& serverId, const char* method, const std::string& url,
           std::string& body) {
  body.clear();
  // Only ever a server this account listed, at the address it listed.
  bool known = false;
  for (const Config::Server& s : config.servers) {
    known = known || (!s.base.empty() && url.compare(0, s.base.size(), s.base) == 0);
  }
  if (url.compare(0, 8, "https://") != 0 || !known || heapTooLow()) return 0;
  freeink::SecureHttpClient http;
  http.setCACert(roots());
  http.setTimeout(20000);
  if (!http.begin(url)) return 0;
  identify(http, config);
  http.addHeader("Accept", "application/json");
  http.addHeader("X-Plex-Token", config.tokenFor(serverId));
  const int status = http.sendRequest(method, std::string());
  if (status > 0) body = http.getString();
  http.end();
  LOG_INF(kTag, "server %s %s -> %d (verified TLS)", method, forLog(url).c_str(), status);
  return status > 0 ? status : 0;
}

std::string newClientId() {
  char id[40];
  std::snprintf(id, sizeof(id), "crossplay-%08lx%08lx", static_cast<unsigned long>(esp_random()),
                static_cast<unsigned long>(esp_random()));
  return id;
}

#else

int account(const Config&, const char* method, const std::string& url, std::string& body) {
  if (std::strcmp(method, "POST") == 0) {
    demo.pinPolls = 0;
    body = R"({"id":1,"code":"DEMO","authToken":null})";
    return 201;
  }
  if (url.find("/pins/") != std::string::npos) {
    body = ++demo.pinPolls >= 2 ? R"({"id":1,"code":"DEMO","authToken":"demo-token"})"
                                : R"({"id":1,"code":"DEMO","authToken":null})";
    return 200;
  }
  body = R"([{"name":"Demo Server","product":"Plex Media Server","platform":"Linux","clientIdentifier":"demo-server",
    "provides":"server","accessToken":"demo-server-token",
    "connections":[{"protocol":"https","uri":"https://demo.invalid:32400","local":true,"relay":false}]},
   {"name":"Living Room","product":"Plexamp","platform":"Android","clientIdentifier":"demo-player-1",
    "provides":"client,player,pubsub-player","connections":[{"protocol":"http","address":"192.168.0.20","port":32500,"local":true}]},
   {"name":"Kitchen","product":"Plexamp","platform":"Linux","clientIdentifier":"demo-player-2",
    "provides":"client,player,pubsub-player","connections":[{"protocol":"http","address":"192.168.0.21","port":32500,"local":true}]}])";
  return 200;
}

int player(const Config&, const std::string& host, const std::string& url, std::string& body) {
  body.clear();
  if (!isLocalAddress(host)) return 0;
  const DemoTrack* track = &kDemoTracks[demo.track];
  if (url.find("/player/playback/") != std::string::npos) {
    if (url.find("/pause?") != std::string::npos) demo.playing = false;
    if (url.find("/play?") != std::string::npos) demo.playing = true;
    if (url.find("/skipNext?") != std::string::npos) {
      demo.track = (demo.track + 1) % kDemoTrackCount;
      demo.timeMs = 0;
    }
    if (url.find("/playMedia?") != std::string::npos || url.find("/skipTo?") != std::string::npos) {
      demo.playing = true;
      demo.timeMs = 0;
      const int key = numberAfter(url, "metadata%2F");
      demo.track = key >= 100 ? (key - 100) % kDemoTrackCount : 0;
    }
    if (url.find("/skipPrevious?") != std::string::npos) {
      demo.track = (demo.track + kDemoTrackCount - 1) % kDemoTrackCount;
      demo.timeMs = 0;
    }
    if (const int v = numberAfter(url, "&volume="); v >= 0) demo.volume = v;
    if (const int v = numberAfter(url, "&shuffle="); v >= 0) demo.shuffle = v;
    if (const int v = numberAfter(url, "&repeat="); v >= 0) demo.repeat = v;
    if (const int v = numberAfter(url, "&offset="); v >= 0) demo.timeMs = v;
    return 200;
  }
  if (demo.playing) demo.timeMs = (demo.timeMs + 3000) % track->durationMs;
  char head[640];
  std::snprintf(head, sizeof(head),
                "<MediaContainer><Timeline state=\"%s\" duration=\"%d\" time=\"%d\" ratingKey=\"%d\" type=\"music\" "
                "volume=\"%d\" shuffle=\"%d\" repeat=\"%d\" "
                "controllable=\"volume,repeat,skipPrevious,seekTo,stop,playPause,shuffle,skipNext\" "
                "machineIdentifier=\"demo-server\" protocol=\"https\" address=\"demo.invalid\" port=\"32400\" "
                "playQueueID=\"1\">",
                demo.playing ? "playing" : "paused", track->durationMs, demo.timeMs % 10000000, 100 + demo.track,
                demo.volume % 1000, demo.shuffle % 10, demo.repeat % 10);
  body = head;
  if (url.find("includeMetadata=1") != std::string::npos) {
    body += std::string("<Track title=\"") + track->title + "\" grandparentTitle=\"" + track->artist +
            "\" parentTitle=\"" + track->album + "\" parentYear=\"" + track->year + "\" thumb=\"/demo/" +
            std::to_string(demo.track) + "\"/>";
  }
  body += "</Timeline></MediaContainer>";
  return 200;
}

// The simulator's covers are files a developer put on its card:
// /jukebox-demo/0.jpg, 1.jpg, 2.jpg. Without them the page draws its
// no-cover state, which is worth seeing too.
int art(const Config&, const Now& now, int, const char* path) {
  const std::string from = "/jukebox-demo/" + now.thumb.substr(now.thumb.rfind('/') + 1) + ".jpg";
  HalFile in;
  HalFile out;
  if (!Storage.exists(from.c_str()) || !Storage.openFileForRead(kTag, from, in)) return 404;
  if (!Storage.openFileForWrite(kTag, path, out)) return 0;
  uint8_t chunk[512];
  for (int n = in.read(chunk, sizeof(chunk)); n > 0; n = in.read(chunk, sizeof(chunk))) out.write(chunk, n);
  return 200;
}

int server(const Config&, const std::string&, const char* method, const std::string& url, std::string& body) {
  auto has = [&](const char* part) { return url.find(part) != std::string::npos; };
  if (method[0] == 'P') {
    body = R"({"MediaContainer":{"playQueueID":7,"playQueueSelectedMetadataItemID":"100"}})";
    return 200;
  }
  if (has("/library/sections") && !has("/library/sections/")) {
    body =
        R"({"MediaContainer":{"Directory":[{"key":"1","type":"artist","title":"Music"},{"key":"2","type":"show","title":"Shows"}]}})";
    return 200;
  }
  if (has("recentlyAdded") || has("/library/metadata/a")) {
    body = R"({"MediaContainer":{"totalSize":3,"Metadata":[
      {"ratingKey":"b1","type":"album","title":"Made-Up Sessions","parentTitle":"The Demo Quartet","year":2021,"leafCount":9},
      {"ratingKey":"b2","type":"album","title":"Placeholder Hours","parentTitle":"Example & Sons","year":2019,"leafCount":11},
      {"ratingKey":"b3","type":"album","title":"Fixtures, Vol. 2","parentTitle":"Sample Orchestra","year":2024,"leafCount":7}]}})";
    return 200;
  }
  if (has("type=8")) {
    body = R"({"MediaContainer":{"totalSize":3,"Metadata":[
      {"ratingKey":"a1","type":"artist","title":"The Demo Quartet"},{"ratingKey":"a2","type":"artist","title":"Example & Sons"},
      {"ratingKey":"a3","type":"artist","title":"Sample Orchestra"}]}})";
    return 200;
  }
  if (has("/playlists")) {
    body = R"({"MediaContainer":{"totalSize":2,"Metadata":[
      {"ratingKey":"p1","type":"playlist","title":"All Music","leafCount":412},
      {"ratingKey":"p2","type":"playlist","title":"Sunday Morning","leafCount":23}]}})";
    return 200;
  }
  if (has("/children")) {
    body = R"({"MediaContainer":{"Metadata":[
      {"ratingKey":"100","type":"track","title":"Paper Lanterns","duration":215000},
      {"ratingKey":"101","type":"track","title":"Second Thoughts","duration":187000},
      {"ratingKey":"102","type":"track","title":"A Song With a Considerably Longer Title Than Fits","duration":302000},
      {"ratingKey":"103","type":"track","title":"Closing Time at the Fixture Shop","duration":244000}]}})";
    return 200;
  }
  body = R"({"MediaContainer":{"playQueueTotalCount":3,"playQueueSelectedItemID":)" + std::to_string(10 + demo.track) +
         R"(,"Metadata":[)";
  for (int i = 0; i < kDemoTrackCount; ++i) {
    // The demo titles carry XML entities for the timeline; the queue is JSON.
    const char* titles[] = {"Paper Lanterns", "Caf\xC3\xA9 at the End of the Line",
                            "A Song With a Considerably Longer Title Than Fits"};
    const char* artists[] = {"The Demo Quartet", "Example & Sons", "Sample Orchestra"};
    body += std::string(i ? "," : "") + R"({"playQueueItemID":)" + std::to_string(10 + i) + R"(,"ratingKey":")" +
            std::to_string(100 + i) + R"(","title":")" + titles[i] + R"(","grandparentTitle":")" + artists[i] +
            R"(","duration":)" + std::to_string(kDemoTracks[i].durationMs) + "}";
  }
  body += "]}}";
  return 200;
}

std::string newClientId() { return "crossplay-simulator"; }

#endif

}  // namespace jukebox::net

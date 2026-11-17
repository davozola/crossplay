// JUKEBOX host tests: the readers against answers captured from a real
// Plexamp and a real Plex Media Server (see run.sh), the addresses, and the
// settings file.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "JukeboxCore.h"

namespace {

int failures = 0;

void check(const bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

std::string slurp(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}

}  // namespace

int main() {
  using namespace jukebox;

  // --- signing in ---
  {
    const char created[] = R"({"id":1234567,"code":"AB3D","product":"x","trusted":false,"authToken":null})";
    Pin pin;
    check(parsePin(created, std::strlen(created), pin), "a new pin reads");
    check(pin.id == 1234567 && pin.code == "AB3D" && pin.token.empty(), "a new pin has a code and no token");
    const char claimed[] = R"({"id":1234567,"code":"AB3D","authToken":"tok-123"})";
    check(parsePin(claimed, std::strlen(claimed), pin) && pin.token == "tok-123", "a claimed pin has its token");
    const char error[] = R"({"errors":[{"code":1020,"message":"Code not found or expired"}]})";
    check(!parsePin(error, std::strlen(error), pin), "an error is not a pin");
    check(pinPollUrl(42) == "https://plex.tv/api/v2/pins/42", "the poll address");
  }

  // --- the account's devices, as plex.tv really lists them ---
  {
    const std::string json = slurp("fixtures/resources.json");
    std::vector<Device> devices;
    check(parseResources(json.data(), json.size(), devices), "resources read");
    int servers = 0;
    const Device* player = nullptr;
    for (const Device& d : devices) {
      if (d.server) ++servers;
      if (d.player) player = &d;
    }
    check(servers == 1, "one server");
    check(player != nullptr, "one player");
    if (player != nullptr) {
      check(player->product == "Plexamp", "the player is Plexamp");
      check(player->host == "192.168.1.135" && player->port == 32500, "the player's local address");
      check(player->token.empty(), "a player keeps no token");
      check(!player->id.empty(), "the player's identifier");
    }
    for (const Device& d : devices) {
      if (d.server) check(d.token == "REDACTED" && !d.id.empty(), "a server carries its token and identifier");
    }
    std::vector<Device> none;
    check(!parseResources("{\"errors\":[]}", 13, none) && none.empty(), "an error is not a list");
  }

  // --- what the player says it is doing ---
  {
    const std::string xml = slurp("fixtures/timeline.xml");
    Now now;
    check(parseTimeline(xml.data(), xml.size(), now), "the timeline reads");
    check(now.valid && now.state == State::Playing, "it is playing");
    check(now.timeMs == 245000 && now.durationMs == 262706, "time and duration");
    check(now.volume == 97 && now.shuffle == 0 && now.repeat == 0, "volume, shuffle, repeat");
    const uint16_t all = CanPlayPause | CanNext | CanPrevious | CanSeek | CanVolume | CanShuffle | CanRepeat | CanStop;
    check(now.can == all, "every control the player lists");
    check(now.title == "L'Heure d'essai", "title");
    check(now.artist == "Fran\xC3\xA7oise Exemple", "artist, with its accent");
    check(now.album == "Vingt \xC3\xA0 trente mille pas" && now.year == "2000", "album and year");
    check(now.trackKey == "9253", "the track's key");
    check(now.thumb.compare(0, 18, "/library/metadata/") == 0, "the cover's path");
    check(now.serverBase == "https://192-168-1-131.0123456789abcdef0123456789abcdef.plex.direct:32400",
          "the server it came from");
    const std::string art = artUrl(now.serverBase, now.thumb, 320);
    check(art.find("/photo/:/transcode?width=320&height=320&minSize=1&upscale=1&url=%2Flibrary%2Fmetadata%2F") !=
              std::string::npos,
          "the cover's address");

    // The same moment asked for without metadata keeps what was known.
    const std::string plain = slurp("fixtures/timeline_plain.xml");
    Now again = now;
    again.timeMs = 0;
    check(parseTimeline(plain.data(), plain.size(), again, true), "the plain timeline reads");
    check(again.timeMs == 245000 && again.title == "L'Heure d'essai", "plain keeps the track");
    Now fresh;
    check(parseTimeline(plain.data(), plain.size(), fresh, true) && fresh.title.empty() && fresh.trackKey == "9253",
          "plain alone knows the key and no title");
    Now other = now;
    other.trackKey = "1";
    check(parseTimeline(plain.data(), plain.size(), other, true) && other.title.empty(),
          "a new track drops the old title");
  }
  {
    const char stopped[] =
        "<MediaContainer><Timeline type=\"video\" state=\"stopped\"/>"
        "<Timeline type=\"music\" state=\"paused\" time=\"10\" duration=\"20\" controllable=\"playPause,skipNext\">"
        "<Track title=\"A &amp; B &#233; &lt;x&gt; &quot;q&quot; a>b\" grandparentTitle='it&apos;s' "
        "parentThumb=\"/p/1\"/>"
        "</Timeline><Timeline type=\"photo\" state=\"stopped\"/></MediaContainer>";
    Now now;
    check(parseTimeline(stopped, sizeof(stopped) - 1, now), "a made-up timeline reads");
    check(now.state == State::Paused && now.can == (CanPlayPause | CanNext), "only the controls it lists");
    check(now.title == "A & B \xC3\xA9 <x> \"q\" a>b", "entities and a quoted >");
    check(now.artist == "it's" && now.thumb == "/p/1", "single quotes, and the album's cover as a fallback");
    check(now.volume == -1 && now.serverBase.empty(), "what it did not say stays unknown");
    const char idle[] = "<MediaContainer><Timeline type=\"music\" state=\"stopped\"/></MediaContainer>";
    check(
        parseTimeline(idle, sizeof(idle) - 1, now) && now.state == State::Stopped && now.can == 0 && now.title.empty(),
        "an idle player");
    const char junk[] = "<html>not a player</html>";
    check(!parseTimeline(junk, sizeof(junk) - 1, now), "something else is not a timeline");
  }

  // --- the queue, as the server really answers ---
  {
    const std::string json = slurp("fixtures/playqueue.json");
    Queue queue;
    check(parseQueue(json.data(), json.size(), queue), "the queue reads");
    check(queue.total == 14 && queue.items.size() == 14, "its length, and the window");
    check(queue.current == 10, "which one is playing");
    if (queue.current >= 0) {
      check(queue.items[queue.current].key == "9253", "the playing track's key matches the timeline's");
      check(queue.items[queue.current].title == "Onze heures", "its title, past the nested members before it");
    }
    check(queue.items[0].artist == "Fran\xC3\xA7oise Exemple" && queue.items[0].durationMs == 353093,
          "artist and duration");
    check(!parseQueue("[]", 2, queue), "something else is not a queue");
    check(queueUrl("https://h:32400", "333", 8) == "https://h:32400/playQueues/333?window=8&own=1",
          "where it is asked");
    const std::string xml = slurp("fixtures/timeline.xml");
    Now now;
    check(parseTimeline(xml.data(), xml.size(), now) && now.queueId == "333", "the timeline names its queue");
  }

  // --- a player describing itself ---
  {
    const std::string xml = slurp("fixtures/player_resources.xml");
    std::string name;
    std::string id;
    std::string product;
    check(parsePlayerInfo(xml.data(), xml.size(), name, id, product), "the player's own description reads");
    check(name == "Android" && product == "Plexamp" && id == "11111111-2222-4333-8444-555555555555",
          "its name, product and identifier");
    check(!parsePlayerInfo("<html/>", 7, name, id, product), "something else is not a player");
    check(playerInfoUrl("192.168.1.9", 32500) == "http://192.168.1.9:32500/resources", "where it is asked");
  }

  // --- addresses ---
  {
    check(timelineUrl("192.168.1.9", 32500, 7, true) ==
              "http://192.168.1.9:32500/player/timeline/poll?wait=0&includeMetadata=1&commandID=7",
          "the timeline's address");
    check(commandUrl("192.168.1.9", 32500, "skipNext", 8) ==
              "http://192.168.1.9:32500/player/playback/skipNext?type=music&commandID=8",
          "a command's address");
    check(commandUrl("192.168.1.9", 32500, "setParameters", 9, "&volume=40") ==
              "http://192.168.1.9:32500/player/playback/setParameters?type=music&commandID=9&volume=40",
          "a parameter's address");
    std::string host;
    int port = 0;
    check(parseAddress(" 192.168.1.20 ", host, port) && host == "192.168.1.20" && port == 32500, "a bare address");
    check(parseAddress("http://10.0.0.5:32501/", host, port) && host == "10.0.0.5" && port == 32501, "a pasted one");
    check(!parseAddress("", host, port) && !parseAddress("a b", host, port) && !parseAddress("1.2.3.4:0", host, port),
          "nonsense is refused");
    check(isLocalAddress("192.168.1.135") && isLocalAddress("10.1.2.3") && isLocalAddress("172.31.0.1") &&
              isLocalAddress("169.254.3.4"),
          "local addresses");
    check(!isLocalAddress("203.0.113.7") && !isLocalAddress("172.32.0.1") && !isLocalAddress("plex.tv") &&
              !isLocalAddress("192.168.1") && !isLocalAddress("192.168.1.999") && !isLocalAddress(""),
          "everything else is not local");
  }

  // --- settings ---
  {
    Config c;
    c.token = "tok";
    c.clientId = "crossplay-1";
    c.playerName = "Sal\xC3\xB3n";
    c.playerId = "pid";
    c.playerHost = "192.168.1.135";
    c.playerPort = 32501;
    c.servers.push_back({"srv1", "t1"});
    c.servers.push_back({"srv2", "t2"});
    Config back;
    unpack(pack(c), back);
    check(back.token == "tok" && back.clientId == "crossplay-1" && back.playerName == c.playerName, "account and name");
    check(back.playerId == "pid" && back.playerHost == "192.168.1.135" && back.playerPort == 32501, "the player");
    check(back.servers.size() == 2 && back.tokenFor("srv2") == "t2", "a server's own token");
    check(back.tokenFor("unknown") == "tok", "an unknown server falls back to the account's");
    check(back.signedIn() && back.hasPlayer(), "signed in, with a player");
    Config empty;
    unpack("", empty);
    check(!empty.signedIn() && !empty.hasPlayer() && empty.playerPort == kPlayerPort, "an empty file is a fresh start");
  }

  // --- small things ---
  {
    char t[16];
    formatTime(245000, t, sizeof(t));
    check(std::strcmp(t, "4:05") == 0, "minutes and seconds");
    formatTime(3765000, t, sizeof(t));
    check(std::strcmp(t, "1:02:45") == 0, "past the hour");
    formatTime(-5, t, sizeof(t));
    check(std::strcmp(t, "0:00") == 0, "never negative");
    check(artHash("a", "bc") != artHash("ab", "c"), "the hash keeps its parts apart");
  }

  if (failures == 0) std::printf("jukebox: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

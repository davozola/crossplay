#include "JukeboxPlay.h"

#include <Arduino.h>
#include <HalStorage.h>

#include <string>

#include "JukeboxCore.h"
#include "JukeboxNet.h"

namespace jukebox {

Started playAlbum(const char* ratingKey) {
  constexpr char kConfigPath[] = "/.crosspoint/jukebox.cfg";
  std::string text;
  if (Storage.exists(kConfigPath)) Storage.readFileToString("JUKE", kConfigPath, 8192, text);
  Config config;
  unpack(text, config);
  if (!config.signedIn() || !config.hasPlayer()) return Started::NotSetUp;
  // The account may list several servers and only one holds this album, so
  // each with an address is asked until one makes the queue.
  const Config::Server* server = nullptr;
  std::string body;
  std::string queueId;
  std::string firstKey;
  bool anyAddress = false;
  bool anyAnswered = false;
  Entry album;
  album.kind = Kind::Album;
  album.key = ratingKey;
  for (const Config::Server& s : config.servers) {
    if (s.base.empty()) continue;
    anyAddress = true;
    const int made = net::server(config, s.id, "POST", makeQueueUrl(s.base, s.id, album, false), body);
    anyAnswered = anyAnswered || made > 0;
    if (made == 200 && parseMadeQueue(body.data(), body.size(), queueId, firstKey)) {
      server = &s;
      break;
    }
  }
  if (!anyAddress) return Started::NoServer;
  if (server == nullptr) return anyAnswered ? Started::NoQueue : Started::Unreachable;
  const std::string extra = playMediaExtra(server->base, server->id, config.tokenFor(server->id), queueId, firstKey);
  const uint32_t commandId = static_cast<uint32_t>(millis());
  const int status =
      net::player(config, config.playerHost,
                  commandUrl(config.playerHost, config.playerPort, "playMedia", commandId, extra.c_str()), body);
  return status == 200 ? Started::Yes : Started::PlayerRefused;
}

}  // namespace jukebox

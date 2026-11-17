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
  const Config::Server* server = nullptr;
  for (const Config::Server& s : config.servers) {
    if (!s.base.empty() && server == nullptr) server = &s;
  }
  if (server == nullptr) return Started::NoServer;

  Entry album;
  album.kind = Kind::Album;
  album.key = ratingKey;
  std::string body;
  std::string queueId;
  std::string firstKey;
  const int made = net::server(config, server->id, "POST", makeQueueUrl(server->base, server->id, album, false), body);
  if (made != 200 || !parseMadeQueue(body.data(), body.size(), queueId, firstKey)) return Started::NoQueue;
  const std::string extra = playMediaExtra(server->base, server->id, config.tokenFor(server->id), queueId, firstKey);
  const uint32_t commandId = static_cast<uint32_t>(millis());
  const int status =
      net::player(config, config.playerHost,
                  commandUrl(config.playerHost, config.playerPort, "playMedia", commandId, extra.c_str()), body);
  return status == 200 ? Started::Yes : Started::PlayerRefused;
}

}  // namespace jukebox

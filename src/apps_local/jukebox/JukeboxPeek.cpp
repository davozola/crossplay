#include "JukeboxPeek.h"

#include <HalStorage.h>

#include <cstdio>
#include <string>

#include "JukeboxCore.h"
#include "JukeboxNet.h"

namespace jukebox {

bool peekPlaying(char* out, const size_t cap) {
  if (cap > 0) out[0] = '\0';
  constexpr char kConfigPath[] = "/.crosspoint/jukebox.cfg";
  if (!Storage.exists(kConfigPath)) return false;
  std::string text;
  Storage.readFileToString("JUKE", kConfigPath, 8192, text);
  Config config;
  unpack(text, config);
  if (!config.signedIn() || !config.hasPlayer()) return false;
  std::string body;
  if (net::player(config, config.playerHost, timelineUrl(config.playerHost, config.playerPort, 1, true), body) != 200) {
    return false;
  }
  Now now;
  if (!parseTimeline(body.data(), body.size(), now) || now.state != State::Playing || now.title.empty()) return false;
  std::snprintf(out, cap, "%s \xE2\x80\x94 %s", now.title.c_str(), now.artist.c_str());
  return true;
}

}  // namespace jukebox

#pragma once

// PLEXAMP JUKEBOX on the wire. Three conversations, each with its own rules:
//
//   plex.tv        HTTPS, certificate verified. Carries the account's token.
//   a media server HTTPS at its plex.direct name, verified. Album art only.
//   a player       plain HTTP, because that is the only thing Plexamp's remote
//                  port speaks, and so only ever at an address that cannot
//                  leave the local network (jukebox::isLocalAddress).
//
// A verification failure is a failure: nothing here retries without it.
//
// The simulator has no radio. There this file answers from a made-up account
// and player instead (kDemo), so the screens can be built and photographed;
// nothing a simulator shows is evidence about a real Plexamp.

#include <string>

#include "JukeboxCore.h"

namespace jukebox::net {

#if defined(FREEINK_NET_WOLFSSL)
constexpr bool kDemo = false;
#else
constexpr bool kDemo = true;
#endif

// Each returns the HTTP status, or 0 when there was no answer at all.

// plex.tv. `method` is "GET" or "POST"; the account's token rides when set.
int account(const Config& config, const char* method, const std::string& url, std::string& body);
// The chosen player. Refuses, with 0, an address that is not a local one.
int player(const Config& config, const std::string& host, const std::string& url, std::string& body);
// A cover, resized by the server it lives on, into a file on the card.
int art(const Config& config, const Now& now, int size, const char* path);

// A media server at its plex.direct name: the queue and the library. `serverId`
// picks the token; `method` is "GET", or "POST" to have a queue made.
int server(const Config& config, const std::string& serverId, const char* method, const std::string& url,
           std::string& body);

// A fresh identifier for this reader, made once and kept with the settings.
std::string newClientId();

}  // namespace jukebox::net

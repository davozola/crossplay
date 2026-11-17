#pragma once

// What every page that refreshes by itself does when its round comes up and
// the radio turns out to be down: ask it to rejoin, and come back in a minute
// instead of a whole interval later.

#include <WiFi.h>

#include <cstdint>

namespace autorefresh {

constexpr uint32_t kRetryMs = 60000;

// True when the network is there for this round. When it is not, the radio is
// told to rejoin the network it was on and `lastFetchMs` is set so that the
// round comes up again in a minute.
inline bool online(const uint32_t now, const uint32_t intervalMs, uint32_t& lastFetchMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
#if !defined(SIMULATOR)
  WiFi.reconnect();
#endif
  if (intervalMs > kRetryMs) lastFetchMs = now - (intervalMs - kRetryMs);
  return false;
}

}  // namespace autorefresh

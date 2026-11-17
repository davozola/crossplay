#pragma once

// One look at the chosen Plexamp player, for a page that is not the Jukebox:
// HOY asks this on its refresh and shows a line when music is playing.

#include <cstddef>

namespace jukebox {

// The title, a dash and the artist into `out` when a player has been chosen in JUKEBOX and it
// is playing right now; false, and an empty string, in every other case. One
// short request on the local network; the caller has Wi-Fi up already.
bool peekPlaying(char* out, size_t cap);

}  // namespace jukebox

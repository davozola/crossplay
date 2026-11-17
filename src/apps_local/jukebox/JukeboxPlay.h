#pragma once

// Starts an album on the Plexamp player chosen in JUKEBOX, for a page that is
// not the Jukebox: LINER NOTES plays the record it is describing.

namespace jukebox {

enum class Started : unsigned char {
  Yes,
  NotSetUp,       // nobody has signed in, or no player was chosen, in JUKEBOX
  NoServer,       // the settings hold no media server address yet
  NoQueue,        // the server did not make a queue for that key
  PlayerRefused,  // the queue exists and the player did not take it
};

// `ratingKey` is the album's own key on the media server. The caller has Wi-Fi
// up already, on the player's network.
Started playAlbum(const char* ratingKey);

}  // namespace jukebox

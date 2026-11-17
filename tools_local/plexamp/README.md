# Plexamp Jukebox: phase A diagnostic

`plex_diag.py` answers one question before any firmware is written: can a
Plexamp player be watched and controlled from another machine on the same
network, with nothing installed on the player?

It needs Python 3 and nothing else. Run it on a computer on the same network
as the player, with a track playing:

    python3 plex_diag.py login      # shows a code to type at plex.tv/link
    python3 plex_diag.py report     # servers, sessions, players, artwork: read-only
    python3 plex_diag.py control    # pause/play, next/previous, then back
    python3 plex_diag.py logout

Every line of its summary is VERIFIED, FAILED or NOT TESTED. The token is kept
in `~/.config/crossplay-plexdiag/state.json`, never in this repository and
never printed.

## What it relies on, and how sure each part is

| Part | Source | Status |
| --- | --- | --- |
| Link-code sign-in (`POST /api/v2/pins`, poll, `authToken`) | Plex's developer documentation | documented |
| Servers and players of an account (`/api/v2/resources`) | Plex's developer documentation | documented |
| What is playing (`/status/sessions` on the server) | Plex's developer documentation | documented; owner's token only |
| A player's own port, 32500 (`/player/timeline/poll`, `/player/playback/*`) | Plex staff on the forum, for headless Plexamp | not documented; to verify per platform |
| The same port on Plexamp for Android | nobody | the thing this tool is for |

Until `control` has been run against a real player, nothing about controlling
Plexamp on Android is known.

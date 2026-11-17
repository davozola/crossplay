# Jukebox: a Plexamp remote

Jukebox shows what a Plexamp player on your network is playing and drives it:
play and pause, next and previous, seek, volume, shuffle and repeat. It links to
a Plex account with a short code and installs nothing on the player.

## What you need

- A Plex account, and Plexamp signed in to it on the player.
- The reader and the player on the same local network.
- Plexamp open on the player, with its remote control setting turned on.

No companion service, no changes to the player, no router ports.

## Setting it up

1. Open **JUKEBOX** and choose **LINK A PLEX ACCOUNT**.
2. On a phone or computer open `plex.tv/link` (the page shows a QR code for
   it), sign in, and type the four-character code from the reader.
3. Pick the player from the list. If it is not listed, **TYPE ADDRESS** takes
   its IP address on your network.
4. The reader asks the player what it is doing. If it answers, it is saved and
   the playing page opens.

The menu on the playing page shows two things separately: whether the player
can be *read*, and whether it has *taken a command*. A player that can be read
but offers no controls is shown without buttons rather than with buttons that
do nothing.

## How it works

| Conversation | Transport | What goes over it |
| --- | --- | --- |
| `plex.tv` | HTTPS, certificate verified | the link code, then the list of your servers and players |
| your media server, at its `plex.direct` name | HTTPS, certificate verified | the album cover, resized by the server |
| the player, port 32500 | **plain HTTP, local network only** | `/player/timeline/poll` to read it, `/player/playback/*` to drive it |

The reader asks the player every three seconds while the page is open, and
repaints only when something changed. Only the controls the player lists in its
own `controllable` field are drawn.

## What has been verified, and what has not

Verified against a real player (Plexamp 4.12.4 on Android) and a real Plex
Media Server, from a computer running `tools_local/plexamp/plex_diag.py`, which
speaks exactly the requests the firmware does:

- linking with a code, listing servers and players;
- reading the timeline with track metadata, fetching the cover, and reading
  the play queue from the server;
- pause, play, next, previous, volume, shuffle, repeat and seek;
- listing the music libraries, recently added albums, artists, an artist's
  albums, an album's tracks and the audio playlists;
- jumping within the queue (`skipTo`);
- having the server make a queue for an album and starting it on the player
  (`playMedia`).

The firmware's readers run against those captured answers in
`host-tests/jukebox` (tokens, identifiers and public addresses replaced).

**Not yet verified:**

- Most of the firmware's screens on a reader. Linking, choosing a player and
  the controls have been used on a reTerminal Sticky against a real Plexamp;
  the queue, the library and the looks were built in the simulator, which has
  no radio and answers from a made-up account and player (it says so on every
  page).
- Whether Plexamp's remote control setting has to be on, or is on by default.
- Three things the library page does that were not in that run: starting a
  **playlist** (the queue is asked for with `playlistID`), **SHUFFLE** (the
  same request with `shuffle=1`), and starting an album **from a chosen
  track** (the tested request, naming that track instead of the first). If
  one fails the page says so and nothing else changes.
- Plexamp on iOS, desktop or headless; shared (not owned) servers; several
  players at once; a player that is asleep or in the background.
- That a media server's `plex.direct` certificate chains to the Let's Encrypt
  root the firmware carries (covers only). plex.tv's is DigiCert Global Root
  G2, checked with `plex_diag.py tls` and carried too. If a certificate
  authority changes, a PEM bundle at `/.crosspoint/jukebox-roots.pem` on the
  card replaces the built-in roots.

## Security and privacy

- **No credentials in the firmware.** The reader holds no server address,
  player, user name or token until you link it.
- **What is stored.** A Plex token for this reader, the tokens of your servers
  (needed for covers), the chosen player's name and address, and the last
  cover, in `/.crosspoint/jukebox.cfg` and `/.crosspoint/jukebox/` on the
  memory card. Your password never reaches the reader.
- **The card is not encrypted.** Anyone holding the card can read the token,
  and a Plex token gives access to the account's servers. Treat the card like
  a signed-in phone. This is a limit of the hardware: these readers have no
  secure storage the firmware uses.
- **Unlinking.** MENU, then UNLINK AND ERASE, deletes all of the above from
  the card. It does not revoke the token at Plex: remove "CrossPlay Jukebox"
  under Authorized Devices in your Plex account settings to do that.
- **The player conversation is plain HTTP**, because that is all Plexamp's
  remote port speaks. It carries the token as a header. The reader therefore
  only ever speaks to a player at a private address (10/8, 172.16/12,
  192.168/16, link-local) and refuses anything else, including a typed one.
  On a network you do not trust, someone else on it could read that traffic.
- **Starting music hands the player a server token.** That is how any Plex
  remote tells a player where to fetch from: the `playMedia` command, over
  the same plain HTTP on the local network, carries the media server's token
  in its address.
- **Tokens are never logged**, and log lines carry request paths without their
  query strings.
- **No analytics or telemetry.** Requests go to plex.tv, your server and your
  player, and nowhere else.

## More than the controls

- **The volume bar** takes a touch anywhere along it; the minus and plus
  either side step by five.
- **Four looks** for the playing page, each a layout of its own. CLASSIC: the
  cover, the track under it, boxed controls. MINIMAL: a small cover and no boxes,
  the title set large, a hairline for the track, bare marks for the controls.
  TAPE: a cassette whose reels show how far through the track is, the keys of
  a deck and a fader. EDITORIAL: the cover and the track side by side, round
  controls, and the tracks that come next listed below, each a touch away. A
  touch on the top line of the page, or LOOK in the menu, moves to the next.
- **QUEUE** shows the tracks around the one playing, read from the media
  server. A touch on a row jumps to it.
- **BROWSE THE LIBRARY** (in the menu, and on the page when nothing is
  playing) lists each music library's recently added albums and its artists,
  and the audio playlists. An album or a playlist has PLAY and SHUFFLE; a
  touch on a track starts the album there.
- **Tapping the cover** opens it large with the track's name and nothing to
  press, for a reader left on a shelf. Any tap goes back.
- **HOY** shows a "Suena ahora" line with the track when its refresh finds the
  chosen player playing, and a tap on it opens Jukebox.

## Not built yet

Searching the library, and browsing by anything but recent albums, artists
and playlists.

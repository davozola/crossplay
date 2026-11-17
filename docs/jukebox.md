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
- reading the timeline with track metadata, and fetching the cover;
- pause, play, next, previous, volume, shuffle, repeat and seek.

The firmware's readers run against those captured answers in
`host-tests/jukebox` (tokens, identifiers and public addresses replaced).

**Not yet verified:**

- The firmware itself on a reader against a real player. The screens were
  built in the simulator, which has no radio and answers from a made-up
  account and player; it says so on every page.
- Whether Plexamp's remote control setting has to be on, or is on by default.
- Plexamp on iOS, desktop or headless; shared (not owned) servers; several
  players at once; a player that is asleep or in the background.
- Whether plex.tv's certificate chains to one of the roots the firmware
  carries (Let's Encrypt and Google Trust Services). `plex_diag.py tls` prints
  who signs it. If linking fails with "could not reach plex.tv over a verified
  connection", a PEM bundle at `/.crosspoint/jukebox-roots.pem` on the card
  replaces the built-in roots.

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
- **Tokens are never logged**, and log lines carry request paths without their
  query strings.
- **No analytics or telemetry.** Requests go to plex.tv, your server and your
  player, and nowhere else.

## Not built yet

A view of the queue, browsing the library, and starting playback from the
reader.

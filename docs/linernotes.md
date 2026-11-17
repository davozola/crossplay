# Liner Notes

Notes on the records in your own library, read on the device: why a record
matters, its context, how it sounds, what to listen for, its essential tracks,
a few curiosities, why it is classed as it is, where the facts come from, and
which of your other records it leads to.

## The app and the pack are two things

The firmware holds the reader and nothing else. The notes are a **pack**: a
folder on the SD card, made on a computer by whoever owns the library.

- The pack is personal. It is built from one person's library and it carries
  that library's album keys. It does not belong in this repository and nothing
  here contains one.
- The app works with no pack: it says what a pack is and where to put it.
- Nothing about the Plex account is in the pack or in this app. Playing a
  record goes through JUKEBOX's own settings (see [jukebox.md](jukebox.md)).

## The pack, version 1

`tools_local/linernotes/ln_pack.py` writes it. Copy the folder to the card as
`/linernotes`.

| File        | What it holds                                                            |
| ----------- | ------------------------------------------------------------------------ |
| `index.tsv` | A header line, then one album per line, sorted by artist.                 |
| `es.txt`    | Every album's notes in Spanish, one record after another.                 |
| `en.txt`    | The same in English.                                                      |

An index line has eleven tab-separated fields: id, artist, title, year, label,
category bits, the album's key on the media server, and the offset and length
of its record in `es.txt` and in `en.txt`.

A record is lines of `K<tab>text`:

| K | Meaning                                              |
| - | ---------------------------------------------------- |
| W | why it matters                                       |
| C | context                                              |
| S | the sound                                            |
| L | one thing to listen for                              |
| T | an essential track: title, tab, a note               |
| U | a curiosity                                          |
| E | a category: its name, tab, why                       |
| X | a related album: its line in the index, tab, a note  |
| R | a source: its title, tab, its address                |

The index is read whole when the app opens (about 85 bytes an album). A record
is read by offset when its album is opened and dropped when it is closed, so
the size of the pack does not matter to memory.

Text is folded by the packer to what the reading face can draw, U+0020 to
U+007E and U+00A0 to U+00FF: curly quotes and dashes become plain ones,
Cyrillic is transliterated, anything else is reduced to its base letters.

## What is on the screen

- **Home**: the album of the day, Essential, Hidden gems, Explore (everything,
  or one category at a time), My marks, Recent, By letter, Surprise me, and
  the language of the notes.
- **An album**: its notes as pages, then PLAY, a mark that cycles (want to
  listen, listening, rediscovered, favourite) and RELATED, which lists the
  records the notes connect it to; each opens.
- Up/Down and Left/Right turn pages. Back goes back the way you came.

Marks, the recent list and the language are kept in
`/.crosspoint/linernotes.cfg`, apart from the pack, so replacing the pack does
not lose them.

## Verified and not

Verified in the simulator and by `host-tests/linernotes`: reading a pack,
laying a record out, paging, marks, related albums, the page shown with no
pack.

**Not verified on a device**: PLAY. In the simulator the Jukebox answers from a
made-up player, so "PLAYING ON YOUR PLEXAMP" there is not evidence. On a device
it needs JUKEBOX to have signed in, chosen a player and browsed the library at
least once, which is when it learns the server's address.

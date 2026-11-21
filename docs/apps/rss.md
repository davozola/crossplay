# RSS

The sites you read, newest first, as text. Card #692, approved by Mario on
2026-10-17. This file is the design as built, and the things the code will not
tell a reader.

## What it is

- **On the shelf** as RSS, the last row of the Apps folder (appended, so no
  existing row moved under a thumb that knew where it was).
- **It is set like NEWS**, a page of a paper rather than the fork's black band
  and boxed rows (davozola fork, 2026-11-21): a serif masthead over a double
  rule, headlines set whole and divided by hairlines, the controls in one quiet
  line at the foot.
- **The first screen is every post from every feed**, newest first, with the
  site's name and the date in a small line over each headline. An unread post
  is bold behind a square; an opened one is the plain face. The masthead says
  how many are new.
- **The foot is Feeds, Refresh and the page.** Feeds (or the masthead) opens
  the menu: all posts, the saved shelf, and each feed with how many of its
  posts are unread, or "no answer" when the last refresh did not reach it.
  Tapping one sets the front to it. A sideways swipe on the front steps to the
  next one without the menu; a vertical swipe pages. Pages are measured as
  they are reached, so the foot says which page this is, not how many.
- **Feeds are added from a phone**, the way Notes takes its typing. Add feeds
  (on the empty first screen) or Add or remove (on the menu) joins Wi-Fi, serves
  one page and shows a QR code with the address under it. On the page a person
  pastes either a site's address or a feed's, and removes feeds. The panel says
  what happened in a sentence: "Adding kottke.org...", "Added Kottke.", "No feed
  at that address."
- **The card is shown first.** Opening the app reads the feeds unasked only
  when two hours have gone by since the last refresh and a saved network can
  be joined with no picker; Refresh on the front and on the menu reads them at
  any time. Nothing runs in the background or asleep. Declining the Wi-Fi
  picker is not leaving: the list read last time is on the card and opens as
  it was.
- **Reading** is NEWS's story page: the headline on the first page, whose it
  is and the page in one line at the top, and Back, Aa and Save at the foot. A
  tap on the right half or a swipe turns on, the left half back; Aa steps
  through three sizes of type and is remembered. A post the feed carried whole
  (`content:encoded`, Atom `<content>`) is read off the card. Any other post
  is fetched through the extractor Hacker News uses (r.jina.ai) and judged by
  Hacker News's readability gate.
- **Read posts are marked read** the moment they are opened, whether or not the
  words then arrive: the bold face answers "have I looked at this".
- **SAVE** keeps the post on the card exactly as it was shown, in the same
  shelf format Hacker News's saved articles use, in a folder of its own.
- **No server of ours and no account.** The reader fetches each feed from its
  own site. The only third party is the extractor, and only for posts whose feed
  did not carry the whole text.

## Where it lives

|                                                  |                                                                                                                                      |
| ------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------ |
| `src/apps_local/rss/RssCore.*`                   | the model, addresses, dates, HTML to text, keeping the newest, the files on the card. Freestanding                                   |
| `src/apps_local/rss/RssFeed.*`                   | the streaming parser (expat) for the three formats, the HTML `<link>` scanner, the probe that tells a feed from a page. Freestanding |
| `src/apps_local/rss/RssDesk.*`                   | what the phone page asked for and what the app answered, behind one mutex. Freestanding                                              |
| `src/apps_local/rss/RssScreens.*`                | the front, the menu, the notice and the phone screen. Freestanding                                                                   |
| `src/apps_local/rss/RssServer.*`, `RssPage.html` | the phone page and its four routes                                                                                                   |
| `src/apps_local/rss/RssFetch.*` | the feed GET that STOP can interrupt while it connects |
| `src/apps_local/rss/RssActivity.*`               | the radio, the card and the panel                                                                                                    |
| `host-tests/rss/`                                | the formats, discovery, limits, broken feeds, dates, read and saved state, the desk                                                  |
| `host-tests/ui/` (RSS block)                     | every screen's taps, the unread mark, the menu's rows, the refresh's Stop                                                            |

Shared rather than copied: the story page and its paging are NEWS's
(`newsui::buildStory`, `news::Paging`); the saved shelf (`hn::Library`, which
takes its directory), the extractor's address and size cap
(`hn::kExtractorPrefix`, `hn::kMaxArticleBytes`) and the readability gate are
Hacker News's.

## The three formats

Told apart by the root element and nothing else: `<rss>` is RSS 2.0 (and
0.9x), `<feed>` is Atom, `<rdf:RDF>` is RSS 1.0. The firmware's expat is built
without namespace support (`XML_NS` is not defined), so names arrive with their
prefixes and are matched by their local part: inside RSS a prefixed element is
an extension unless it is `content:encoded` or `dc:date`; inside Atom every
element is Atom's own.

A post's date is its publication date when the feed gives one, ranked
`published` > `pubDate` > `dc:date` > `updated`. RFC 822 and ISO 8601 are both
read, offsets and US zone names included; anything else is no date.

## Reading a feed without holding it

Bytes go from the socket through `EntityFixer` into expat, and each post is
decided the moment its closing tag arrives: kept or not, and if kept its words
go to the card there and then. A feed of a thousand posts costs one post's
worth of RAM.

- **The newest twenty, not the first twenty.** `Keeper` ranks by date (an
  undated post ranks by where it stood, earlier being newer), so an oldest-first
  feed keeps its end and a newest-first one keeps its top. Once twenty are kept
  and five in a row are turned away, the download stops.
- **Limits**, all in `RssCore.h`: 1MB per feed, 96KB of markup per post (Hacker
  News's article cap), 16KB per summary, 300 bytes per headline, 1KB per link,
  512KB of a web page read for its `<head>`, thirty feeds.
- **Ids are scoped to their feed.** Plenty of feeds number their posts from 1,
  and two feeds' post 7 must not share a file on the card.
- **A refresh merges, it does not replace.** Each feed keeps the newest twenty
  of what it just sent and what it had. Slashdot's server was cutting its feed
  off at 16KB while this was built, and replacing would have turned fifteen
  posts into three on every refresh.

## Careless and broken feeds

- **HTML entities outside CDATA** (`&nbsp;`, `&mdash;`, a bare `AT&T`) stop
  expat dead, often in the first title. `EntityFixer` escapes every ampersand
  that does not begin one of XML's own references before expat sees it, and
  `decodeEntities` turns the text back into a space or a dash afterwards. CDATA
  and comments pass through untouched. The same filter drops raw control bytes,
  escapes numeric references to characters XML forbids (`&#0;`, a lone
  surrogate), and skips whitespace before the `<?xml` declaration, each of
  which stops expat at the first byte. A document that opens like UTF-16 is
  left alone.
- **Windows-1252** is declared by enough feeds that expat is given a table for
  it. ISO-8859-15 is read the same way.
- **A feed broken halfway down still has a top.** Whatever arrived before the
  error stands, and is merged as above.
- **Pictographs are dropped**: stars, arrows the typography fold did not turn
  into ASCII, dingbats, emoji, private use. No cut on this device draws them and
  a missing glyph draws as nothing, so Daring Fireball's starred headlines had a
  hole in them. Letters of every script stay, because a wrong letter is worse
  than a missing one.
- **A post with no headline** is named by its first words.

## Adding a feed

The page sends what was pasted. The app normalises it (a bare host gets
`https://`, `feed://` becomes `https://`, other schemes are refused), fetches it
once, and reads the same bytes two ways: as a feed, and as a page looking for
`<link rel="alternate" type="application/rss+xml">` (or Atom, or RDF) in its
head. A feed is subscribed on the spot with its posts already read; a page's
first named feed is fetched and subscribed. Adding and the first read are one
step, so there is no refresh to wait for after the phone.

The web server never touches a feed. It puts requests on the `Desk` and the app
answers them from its own loop: adding takes seconds, and the simulator serves
on a thread of its own, so a handler that edited the list would race the loop
that draws it. The page polls `GET /r/state` for its answer.

**What is behind the QR code** is the list of feed addresses and the two
requests. No file, no setting, nothing on the card. Anyone on the same network
can add or remove a feed while that screen is up, which is the same exposure
Notes' page has. An address that points back at the reader (localhost, 127.x,
link-local, its own IP) is refused, because the reader would wait on a server
its own loop is busy not answering; a feed elsewhere on the house network is
allowed. While an add is fetching, the page goes on being answered, so it
shows the reader working. It is its own small server rather than another surface on
`CrossPointWebServer`, so no upstream file grew. Developer Mode yields port 80
while it is up.

## On the card

Under `/.crosspoint/rss/`, plain text, readable in an editor:

|                                    |                                                                                                                                |
| ---------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| `feeds.tsv`                        | `rssfeeds 1`, then `url TAB title TAB failed`                                                                                  |
| `posts.tsv`                        | `rssposts 1`, then `id TAB feed TAB date TAB body TAB title TAB link`; `body` is 0 none, 1 whole, 2 summary                    |
| `read.txt`                         | `rssread 1`, then one id per line; forgotten when the post leaves every feed                                                   |
| `posts/<id>.txt`, `posts/<id>.sum` | a post's words, whole or summary; the kind is in the name so a post whose feed starts carrying the whole text is written again |
| `saved/`                           | Hacker News's shelf format (`hnsaved 2`), one text file per saved post                                                         |

A body file is written once and pruned when no post points at it; one written
by a refresh and pushed out by a newer post in the same feed is removed at
once. Every list is written beside itself and moved over the old one; if the
move fails after the old file is gone, the `.tmp` copy is what the next
opening reads. A list that is on the card and cannot be read (damaged, too
big, or from a newer build) is left alone: nothing is written over it for as
long as the app is open, and adding a feed says why it cannot.

## When a post's words are not there

- The link cannot be an article (a PDF, a video, a social site) and the feed
  carried a summary: the summary, with "That is all the feed carries, and its
  link is not a page this reader can open." With no summary: NOT READABLE HERE.
- The extractor's answer fails the readability gate: the summary with "The page
  itself would not come through as text", or NOT READABLE HERE.
- No connection, or the picker declined: the summary with "The rest of the post
  needs a connection", or the list again.

## Refreshing

Every feed in one loop pass, painting "REFRESHING", the feed's name and "n of
m" before each. The pass holds the loop for as long as it runs, so it pumps
input itself, between feeds and every 50ms inside each download, connecting
and waiting included, and answers Back, the Home gesture and a tap on STOP.
Stopping abandons the feed in hand, which keeps its old posts and is not marked
FAILED; the feeds already read stand. The Home gesture only stops the refresh:
the event is consumed by this pump, so a second one goes home.

Feeds are fetched through `RssFetch`, the same resumable SecureHttpClient GET
`HttpDownloader` makes, with a 20 second timeout instead of 60 and a poll that
runs while it waits. `HttpDownloader` only calls back once body bytes arrive,
so a host that never answered held STOP dead for minutes; it is CrossPoint's
file, so the variant lives here. This is the one place the app calls
`mappedInput.update()`, which the input-pump rule allows exactly because
nothing else is sampling.

**The simulator cannot show this screen.** It presents frames on its main
thread, which is the thread the refresh holds, so a scripted run never
photographs REFRESHING: the next frame it shows is the list. Its network shim
asks the abort poll while curl runs (since 2026-11-18, `sim_catchup.py`, "a
hung host can be left"), so STOP ends the feed in hand at once there too. The
screen is covered by `host-tests/ui` instead, and needs eyes and a finger on
hardware.

## Known limits

- **Relative links resolve against the address asked for, not the one a
  redirect landed on.** The fetch follows redirects but does not report where
  it ended. Absolute links, and links relative to the site's root, are
  unaffected; a path-relative feed link on a page that redirected to another
  path is not.
- **A post is fetched through the extractor with `HttpDownloader`**, as Hacker
  News fetches articles: one request, its 60 second timeout, and OPENING on the
  panel until it answers.

## Not verified

On hardware, nothing yet: Wi-Fi, TLS fetches of real feeds on the ESP32-S3, the
phone page from a real phone, mDNS, the QR scan, the busy screen and STOP, and
heap during a refresh. In the simulator: all three formats against live feeds,
discovery from a site's address, the extractor, saving and reading back, a
failing feed, a slow one, and the no-Wi-Fi path.

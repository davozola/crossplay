# News

Headlines in five sections (World, Economy, Tech, AI, Long reads), read by the
device straight off each outlet's own RSS or Atom feed, and the article behind
any of them.

## Nothing of ours in the middle

- The feed list is a table in `src/apps_local/news/NewsCore.cpp`. To add,
  drop or fix an outlet, edit that table.
- A section is fetched when REFRESH is tapped, never on its own. Its headlines
  are kept in `/.crosspoint/news/s<N>.tsv`, so the last ones open with no
  network.
- An article goes through the same text extractor HACKER NEWS uses
  (`r.jina.ai`): that third party learns which article was opened. The
  headlines never go near it.
- Every article that opened in full is kept in `/.crosspoint/news/a/` and opens
  from the card after that; its row says ON CARD. Nothing prunes that folder.
- FAV in the reader makes the article a favourite: it is listed under the FAVS
  tab and its text is copied to `/.crosspoint/news/f/`, where it stays until
  FAV is tapped again. The list is `/.crosspoint/news/favourites.tsv`, forty
  at most.
- Behind a paywall, or on a page that needs a browser, the reader shows the
  outlet's own summary from the feed and says that is what it is.

## Reading a feed

A feed can be hundreds of kilobytes. `news::FeedScanner` reads it as it comes
off the socket and keeps a title, an address, a date and the start of a summary
for the first ten items, then cuts the transfer. A section merges its feeds
newest first, one row per address, forty rows at most.

## Verified and not

Verified by `host-tests/news` and in the simulator: the scanner against RSS 2.0
and Atom written to the specifications, dates, the section file, the list, the
lead, paging, the tabs, an article opened from the card, making and listing a
favourite.

**Not verified anywhere**: any real feed. The machine this was built on could
not reach one, so the addresses in the table are unchecked and the fixtures are
hand-written, not captures. The first REFRESH on a device is the first real
test; a section whose feeds all fail says so and keeps what it had.

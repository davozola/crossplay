# News

The day's edition of [Kagi News](https://news.kagi.com), in the sections the
reader chose, kept on the card and read with no network.

## The data, and whose it is

Kagi publishes one batch a day (around 12:00 UTC) through a public API that
needs no key. Each story is Kagi's synthesis of what many outlets reported,
with its sources listed. The data is licensed **CC BY-NC 4.0**: free with
attribution, for non-commercial use. So:

- every story ends with the attribution, the licence and `news.kagi.com`;
- nothing fetched is committed to this repository (the test fixtures are
  hand-written to the API's shape, with invented text);
- this app must not be used commercially without a licence from Kagi.

Two calls are made, both `GET`, both unauthenticated:

1. `https://news.kagi.com/api/batches/latest/categories` — the batch's date
   and the UUID it gives each category. The UUIDs change with every batch, so
   this cannot be skipped.
2. `https://news.kagi.com/api/batches/latest/categories/<uuid>/stories?limit=12`
   — one per chosen section. No `lang` (`lang=source` was refused with a 4xx on
   2026-11-19).

## The sections

A section is one Kagi category, and the reader chooses which: **Sections >
Choose sections** lists every English-language category in the batch (the list
is kept in `categories.tsv`, refreshed with every fetch) and up to eight can be
on. A card that has never chosen starts with World, Business, Technology, AI
and Science (`kDefaultSections`). English only: Kagi does not translate, and a
category in another language is not offered.

Each section keeps at most eight stories. A story is told once in the whole
edition: Kagi marks the same event in other categories (`main_story`,
`other_angles`), and `news::Deduper` drops a story whose event, or whose
headline, an earlier section already has. The transfer of a category is cut as
soon as the section is full.

## How it looks

Not like the rest of the fork, on purpose: a serif masthead over a double rule,
headlines in bold serif set whole and divided by hairlines, and a story that
runs nearly edge to edge with one quiet line of controls (Back, Aa, Save) at
the foot. The first page of a story carries its headline; the pages after are
all text.

A story is one paged document: the summary, a quote, then KEY POINTS,
PERSPECTIVES, CONTEXT, TIMELINE, WHAT COMES NEXT and ORIGINAL SOURCES. A part
Kagi left empty is left out, heading and all. Kagi's `[outlet#3]` citation
marks are removed; the sources are listed at the end instead.

Save keeps a story under Favourites with its own copy of the text; Aa steps the
type through three sizes; a tap on either half of the page, a swipe, or the
page buttons turn it. On a section's front, a sideways swipe moves to the next
section.

## On the card

Everything is under `/.crosspoint/news/`: `sections.cfg` (the reader's choice),
`categories.tsv`, `sec-<hash>.tsv` (a section's stories), `a/` (one text file
per story), `edition-files.txt` and `edition-sections.txt` (what the edition
owns and which sections it was fetched for), `favourites.tsv` and `f/`,
`textsize.cfg`, and `last-refresh.txt`.

Fetching replaces the edition on the card only if every chosen section that is
in today's batch transferred; otherwise the old edition stays and the notice
names the sections that failed.

## Verified and not

Verified by `host-tests/news` and in the simulator: the JSON scanner against
the API's shape in any chunking, the text cleaning, the document, de-duplication,
the section file, the front page, the menu, the chooser, the reader and its
paging, favourites, the type sizes.

Verified by reading the live API on 2026-11-19: both endpoints answer with no
authentication; the key names and nesting; twelve stories per category; that
each category carries its language.

**Not verified**: a fetch from the device itself. The build machine cannot
reach the API, so the first REFRESH on a Sticky is the first end-to-end run.
The notice it ends with, and `last-refresh.txt`, give the stories kept, the
bytes downloaded, the seconds it took and the lowest free memory seen.

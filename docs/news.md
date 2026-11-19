# News

The day's edition of [Kagi News](https://news.kagi.com) in four sections of at
most eight stories, kept on the card and read with no network.

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
2. `https://news.kagi.com/api/batches/latest/categories/<uuid>/stories?limit=10`
   — one per category. No `lang`: a category comes in its own language
   (`lang=source` was refused with a 4xx on 2026-11-19, and Kagi had no Spanish
   translation of the English categories).

## The sections

| Tab | Kagi categories, and how many stories each may give |
| --- | --- |
| ACTUALIDAD | world 4, europe 2, spain 2 (the Spanish-language ones) |
| ECONOMIA | economy 4, business 4 |
| TECNOLOGIA E IA | tech 4, ai 4 |
| CIENCIA Y ENERGIA | science 5, energy 3 |

The table is `kSections` in `NewsCore.cpp`. A story is told once in the whole
edition: Kagi marks the same event in other categories (`main_story`,
`other_angles`), and `news::Deduper` drops a story whose event, or whose
headline, an earlier section already has. The transfer of a category is cut as
soon as its share is filled.

## Reading

A story is one paged document: headline, Kagi's category and the number of
outlets, the summary, a quote, then KEY POINTS, PERSPECTIVES, CONTEXT, TIMELINE,
WHAT COMES NEXT and ORIGINAL SOURCES. A part Kagi left empty is left out,
heading and all. Kagi's `[outlet#3]` citation marks are removed; the sources
are listed at the end instead. Headings are in the story's language.

FAV keeps a story under the FAVS tab with its own copy of the text; Aa steps
the type through three sizes; a tap on either half of the page, a swipe, or the
page buttons turn it.

## On the card

Everything is under `/.crosspoint/news/`: `edition-<N>.tsv` (a section's
stories), `a/` (one text file per story), `edition-files.txt` (which files the
edition owns, so the next one clears them), `favourites.tsv` and `f/`,
`textsize.cfg`, and `last-refresh.txt`.

REFRESH fetches the whole edition. It replaces the one on the card only if
every section got at least three stories; otherwise the old edition stays and
the notice says which categories gave nothing.

## Verified and not

Verified by `host-tests/news` and in the simulator: the JSON scanner against
the API's shape in any chunking, the text cleaning, the document, de-duplication,
the section file, the list, the reader, favourites, the type sizes.

Verified by reading the live API on 2026-11-19: both endpoints answer with no
authentication; the key names and nesting; twelve stories per category; that
`world`, `ai` and `economy` are English and `spain` is Spanish.

**Not verified**: a fetch from the device itself. The build machine cannot
reach the API, so the first REFRESH on a Sticky is the first end-to-end run.
The notice it ends with, and `last-refresh.txt`, give the stories kept, the
bytes downloaded, the seconds it took and the lowest free memory seen.

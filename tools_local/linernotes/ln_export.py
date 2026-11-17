#!/usr/bin/env python3
"""Liner Notes, step 1: export the music library of your own Plex server.

Runs on your computer, on the same network as the server, with nothing but the
Python standard library. It reuses the sign-in of the Jukebox diagnostic
(tools_local/plexamp/plex_diag.py login): no new credentials are asked for.

    python3 ln_export.py                 # every music library on every server
    python3 ln_export.py --server NAME   # only the server whose name contains NAME

It writes, into ./linernotes next to where it runs:

    library.json   every album with its tracks, tags, identifiers and play counts
    summary.txt    what the library is made of, and how complete its metadata is

Both are PRIVATE. They describe your collection and your listening, so keep them
out of any repository. Neither contains a token, a server address or a player:
only what Plex says about the music.
"""

import argparse
import collections
import datetime
import json
import os
import socket
import ssl
import sys
import urllib.error
import urllib.parse
import urllib.request

STATE_PATH = os.path.join(os.path.expanduser("~"), ".config", "crossplay-plexdiag", "state.json")
PLEX_TV = "https://plex.tv"
PAGE = 500
TAG_FIELDS = ("Genre", "Style", "Mood", "Format", "Subformat", "Country", "Collection", "Label")


def load_state():
    try:
        with open(STATE_PATH) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def headers(state, token):
    return {
        "Accept": "application/json",
        "X-Plex-Client-Identifier": state.get("client", "crossplay-linernotes"),
        "X-Plex-Product": "CrossPlay Liner Notes export",
        "X-Plex-Version": "0.1",
        "X-Plex-Platform": "Python",
        "X-Plex-Token": token,
    }


def fetch(url, hdrs, timeout=30):
    req = urllib.request.Request(url, headers=hdrs)
    try:
        with urllib.request.urlopen(req, timeout=timeout, context=ssl.create_default_context()) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, b""
    except (urllib.error.URLError, socket.timeout, ConnectionError, OSError) as e:
        return 0, str(getattr(e, "reason", e)).encode()


def as_json(body):
    try:
        return json.loads(body.decode("utf-8", "replace"))
    except ValueError:
        return None


def servers(state):
    code, body = fetch(PLEX_TV + "/api/v2/resources?includeHttps=1", headers(state, state["token"]))
    if code == 401:
        sys.exit("plex.tv rejected the stored sign-in: run `python3 plex_diag.py login` again.")
    data = as_json(body) if code == 200 else None
    if data is None:
        sys.exit("Could not list your devices at plex.tv (HTTP %s)." % code)
    return [d for d in data if "server" in (d.get("provides") or "")]


def reach(state, device):
    token = device.get("accessToken") or state["token"]
    conns = sorted(device.get("connections", []), key=lambda c: (not c.get("local"), bool(c.get("relay"))))
    for c in conns:
        uri = c.get("uri")
        if not uri:
            continue
        code, _ = fetch(uri + "/identity", headers(state, token), timeout=5)
        if code == 200:
            return uri, token
    return None, token


def get(base, path, hdrs):
    code, body = fetch(base + path, hdrs)
    data = as_json(body) if code == 200 else None
    if data is None:
        raise RuntimeError("%s -> HTTP %s" % (path.split("?")[0], code))
    return data.get("MediaContainer", {})


def paged(base, path, hdrs, label):
    out = []
    start = 0
    sep = "&" if "?" in path else "?"
    while True:
        mc = get(base, "%s%sX-Plex-Container-Start=%d&X-Plex-Container-Size=%d" % (path, sep, start, PAGE), hdrs)
        items = mc.get("Metadata", [])
        out.extend(items)
        total = mc.get("totalSize", len(out))
        print("\r  %s: %d of %d" % (label, len(out), total), end="", flush=True)
        if not items or len(out) >= total:
            break
        start += len(items)
    print()
    return out


def tags(item, field):
    return [t.get("tag") for t in item.get(field, []) if t.get("tag")]


def guids(item):
    found = {}
    for g in item.get("Guid", []):
        gid = g.get("id", "")
        if "://" in gid:
            scheme, value = gid.split("://", 1)
            found[scheme] = value
    return found


def when(epoch):
    return datetime.datetime.utcfromtimestamp(epoch).strftime("%Y-%m-%d") if epoch else None


def export_section(base, hdrs, section):
    sid = section["key"]
    print("\nLibrary \"%s\"" % section.get("title"))
    listed = paged(base, "/library/sections/%s/all?type=9&includeGuids=1" % sid, hdrs, "albums")

    # The list view leaves out styles, moods and formats: ask for each album in full, fifty at a time.
    full = {}
    keys = [a["ratingKey"] for a in listed]
    for i in range(0, len(keys), 50):
        chunk = ",".join(keys[i:i + 50])
        for m in get(base, "/library/metadata/%s?includeGuids=1" % chunk, hdrs).get("Metadata", []):
            full[m["ratingKey"]] = m
        print("\r  album details: %d of %d" % (min(i + 50, len(keys)), len(keys)), end="", flush=True)
    print()

    tracks = paged(base, "/library/sections/%s/all?type=10&includeGuids=1" % sid, hdrs, "tracks")
    artists = paged(base, "/library/sections/%s/all?type=8&includeGuids=1" % sid, hdrs, "artists")
    artist_info = {a["ratingKey"]: {"genres": tags(a, "Genre"), "countries": tags(a, "Country"),
                                    "ids": guids(a)} for a in artists}

    by_album = collections.defaultdict(list)
    for t in tracks:
        by_album[t.get("parentRatingKey")].append({
            "plexKey": t.get("ratingKey"),
            "disc": t.get("parentIndex"),
            "number": t.get("index"),
            "title": t.get("title"),
            "artist": t.get("originalTitle") or None,  # only when it differs from the album artist
            "durationMs": t.get("duration"),
            "plays": t.get("viewCount", 0),
            "lastPlayed": when(t.get("lastViewedAt")),
            "rating": t.get("userRating"),
            "ids": guids(t),
        })

    albums = []
    for a in listed:
        m = full.get(a["ratingKey"], a)
        ts = sorted(by_album.get(a["ratingKey"], []), key=lambda t: (t["disc"] or 0, t["number"] or 0))
        plays = sum(t["plays"] or 0 for t in ts)
        last = max((t["lastPlayed"] for t in ts if t["lastPlayed"]), default=None)
        album = {
            "plexKey": m.get("ratingKey"),
            "section": sid,
            "artist": m.get("parentTitle"),
            "artistPlexKey": m.get("parentRatingKey"),
            "title": m.get("title"),
            "year": m.get("year"),
            "released": m.get("originallyAvailableAt"),
            "label": m.get("studio"),
            "summary": m.get("summary") or None,
            "thumb": m.get("thumb"),
            "added": when(m.get("addedAt")),
            "rating": m.get("userRating"),
            "albumPlays": m.get("viewCount", 0),
            "trackPlays": plays,
            "lastPlayed": last or when(m.get("lastViewedAt")),
            "ids": guids(m),
            "artistInfo": artist_info.get(m.get("parentRatingKey"), {}),
            "tracks": ts,
        }
        for f in TAG_FIELDS:
            album["labelTags" if f == "Label" else f.lower()] = tags(m, f)
        # Plex keeps the label as "studio"; some agents fill Label tags instead.
        if not album["label"] and album["labelTags"]:
            album["label"] = album["labelTags"][0]
        albums.append(album)
    return albums


def summarise(albums):
    lines = []
    n = len(albums)
    tracks = sum(len(a["tracks"]) for a in albums)
    artists = collections.Counter(a["artist"] for a in albums)
    lines.append("%d albums, %d tracks, %d album artists" % (n, tracks, len(artists)))

    def pct(k):
        return "%3d%%" % round(100 * k / n) if n else "  -"

    lines.append("\nMetadata coverage")
    for label, test in [
        ("year", lambda a: a["year"]),
        ("full release date", lambda a: a["released"]),
        ("record label", lambda a: a["label"]),
        ("genre", lambda a: a["genre"]),
        ("style", lambda a: a["style"]),
        ("format (album/EP/live...)", lambda a: a["format"] or a["subformat"]),
        ("MusicBrainz id", lambda a: a["ids"].get("mbid")),
        ("Plex description", lambda a: a["summary"]),
        ("artwork", lambda a: a["thumb"]),
        ("played at least once", lambda a: a["trackPlays"] or a["albumPlays"]),
        ("rated", lambda a: a["rating"]),
    ]:
        lines.append("  %s  %s" % (pct(sum(1 for a in albums if test(a))), label))

    def top(title, counter, k=40):
        lines.append("\n%s" % title)
        for name, c in counter.most_common(k):
            lines.append("  %5d  %s" % (c, name))

    top("Decades", collections.Counter("%d0s" % (a["year"] // 10) if a["year"] else "unknown" for a in albums), 20)
    top("Genres", collections.Counter(g for a in albums for g in a["genre"]))
    top("Styles", collections.Counter(s for a in albums for s in a["style"]))
    top("Formats", collections.Counter(f for a in albums for f in (a["format"] + a["subformat"]) or ["(none)"]), 15)
    top("Labels", collections.Counter(a["label"] for a in albums if a["label"]))
    top("Artists with most albums", artists)
    top("Artist countries", collections.Counter(c for a in albums for c in a["artistInfo"].get("countries", [])), 25)
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--server", help="only the server whose name contains this")
    args = ap.parse_args()

    state = load_state()
    if not state.get("token"):
        sys.exit("Not signed in. Run `python3 plex_diag.py login` first, from the same folder.")
    found = [s for s in servers(state) if not args.server or args.server.lower() in s.get("name", "").lower()]
    if not found:
        sys.exit("No Plex Media Server found on your account.")

    albums = []
    for device in found:
        base, token = reach(state, device)
        if not base:
            print("Server \"%s\": not reachable from here, skipped." % device.get("name"))
            continue
        hdrs = headers(state, token)
        sections = [s for s in get(base, "/library/sections", hdrs).get("Directory", []) if s.get("type") == "artist"]
        print("Server \"%s\": %d music librar%s" % (device.get("name"), len(sections), "y" if len(sections) == 1 else "ies"))
        for section in sections:
            try:
                albums.extend(export_section(base, hdrs, section))
            except RuntimeError as e:
                print("  FAILED: %s" % e)

    if not albums:
        sys.exit("No albums exported.")
    out = os.path.join(os.getcwd(), "linernotes")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "library.json"), "w") as f:
        json.dump({"exported": datetime.datetime.now().isoformat(timespec="seconds"), "albums": albums}, f,
                  ensure_ascii=False, indent=1)
    text = summarise(albums)
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write(text)
    print("\n" + text)
    print("Written to %s (library.json, summary.txt). Private: keep them out of any repository." % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

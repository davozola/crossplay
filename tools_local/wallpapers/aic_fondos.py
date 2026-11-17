#!/usr/bin/env python3
"""Candidates for the reader's wallpapers, from the Art Institute of Chicago's open-access collection.

The museum publishes its public-domain works through a free API with no key
(https://api.artic.edu/docs). This asks it for works in four kinds that suit a
black-and-white e-ink panel held upright, keeps the ones marked public domain
whose picture is taller than wide, and saves a small copy of each to look
through. Nothing is sent anywhere but to the museum.

    python3 aic_fondos.py candidates          # ~160 small pictures into ./fondos_candidatos/
    python3 aic_fondos.py more                # a narrower second pass into ./fondos_candidatos2/
    python3 aic_fondos.py fetch 123 456 ...   # the chosen ones, large, into ./fondos_elegidos/

Standard library only. One request a second, as the museum asks.
"""
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API = "https://api.artic.edu/api/v1/artworks/search"
IIIF = "https://www.artic.edu/iiif/2"
HEADERS = {"AIC-User-Agent": "crossplay-wallpapers (github.com/davozola/crossplay)", "User-Agent": "crossplay-wallpapers/1"}
FIELDS = "id,title,artist_title,date_display,image_id,thumbnail,classification_title,is_public_domain"
PER_KIND = 45
PER_QUERY = 60

KINDS = {
    "japonesa": ["Hokusai", "Hiroshige", "Utamaro", "Ohara Koson", "ukiyo-e landscape woodblock", "Kuniyoshi",
                 "Hiroshige rain", "Hokusai Fuji"],
    "grabados": ["Durer engraving", "Rembrandt etching", "Piranesi", "Gustave Dore", "Goya etching",
                 "Whistler etching", "Meryon etching", "Callot etching"],
    "botanica": ["botanical print", "flowers woodblock print", "birds woodblock print", "Audubon", "insects print",
                 "Redoute", "fish print", "plant study drawing"],
    "paisaje": ["landscape etching", "architecture etching", "view of Rome etching", "cathedral etching",
                "marine etching", "city view print", "mountain landscape print", "bridge etching"],
}


# A second, narrower pass: (what to ask for, a word the artist's name must hold).
# Only prints and drawings are kept, which is what a one-bit panel shows well.
MORE = {
    "japonesa": [("Hiroshige One Hundred Famous Views of Edo", "Hiroshige"),
                 ("Hiroshige Famous Views of the Sixty-odd Provinces", "Hiroshige"),
                 ("Hiroshige snow", "Hiroshige"), ("Hiroshige moon", "Hiroshige"), ("Hiroshige rain", "Hiroshige"),
                 ("Hokusai waterfall", "Hokusai"), ("Hokusai Fuji", "Hokusai"), ("Hokusai bridge", "Hokusai"),
                 ("Hokusai poem", "Hokusai"), ("Hasui", "Hasui"), ("Yoshitoshi moon", "Yoshitoshi"),
                 ("Kuniyoshi", "Kuniyoshi"), ("Hiroshige II", "Hiroshige")],
    "botanica": [("Ohara Koson", "Koson"), ("Ohara Shoson", "Shoson"), ("Hiroshige bird and flower", "Hiroshige"),
                 ("Hokusai flowers", "Hokusai"), ("Ito Jakuchu", "Jakuch"), ("Audubon Birds of America", "Havell"),
                 ("Audubon", "Audubon"), ("Maria Sibylla Merian", "Merian"), ("Redoute roses", "Redout"),
                 ("Utamaro insects", "Utamaro"), ("Bairei birds", "Bairei"), ("Keinen birds", "Keinen"),
                 ("Durer hare plant", "rer"), ("Hiroshige fish", "Hiroshige")],
    "paisaje": [("Meryon Paris etching", "Meryon"), ("Whistler Venice etching", "Whistler"),
                ("Whistler Thames etching", "Whistler"), ("Piranesi Views of Rome", "Piranesi"),
                ("Canaletto etching", "Canal"), ("Samuel Palmer", "Palmer"), ("Seymour Haden", "Haden"),
                ("Felix Buhot", "Buhot"), ("John Taylor Arms", "Arms"), ("Rembrandt landscape", "Rembrandt"),
                ("Hercules Segers", "Segers"), ("Claude Lorrain etching", "Claude"), ("Corot cliche-verre", "Corot"),
                ("Charles Jacque landscape", "Jacque"), ("Bresdin", "Bresdin"), ("Hiroshige mountain", "Hiroshige")],
    "grabados": [("Durer Apocalypse", "rer"), ("Durer Saint Jerome", "rer"), ("Durer Rhinoceros", "rer"),
                 ("Rembrandt self-portrait etching", "Rembrandt"), ("Rembrandt three trees", "Rembrandt"),
                 ("Goya Caprichos", "Goya"), ("Dore", "Dor"), ("Bruegel engraving", "Bruegel"),
                 ("Schongauer", "Schongauer"), ("Lucas van Leyden", "Leyden"), ("Callot", "Callot"),
                 ("Hogarth", "Hogarth"), ("Blake engraving", "Blake")],
}
PRINTS = ("woodblock", "woodcut", "etching", "engraving", "lithograph", "drypoint", "aquatint", "mezzotint",
          "print", "drawing")


def cmd_more():
    first = os.path.join(os.getcwd(), "fondos_candidatos", "candidatos.json")
    seen = set(c["id"] for c in json.load(open(first))) if os.path.exists(first) else set()
    out = os.path.join(os.getcwd(), "fondos_candidatos2")
    os.makedirs(out, exist_ok=True)
    listing = []
    for kind, queries in MORE.items():
        kept = 0
        os.makedirs(os.path.join(out, kind), exist_ok=True)
        for query, name in queries:
            if kept >= 60:
                break
            found = search(query)
            took = 0
            for item in found:
                artist = item.get("artist_title") or ""
                kindOf = (item.get("classification_title") or "").lower()
                if item["id"] in seen or not item.get("image_id") or not item.get("is_public_domain"):
                    continue
                if not upright(item) or name.lower() not in artist.lower() or not any(p in kindOf for p in PRINTS):
                    continue
                if took >= 8:
                    break
                picture = get("%s/%s/full/240,/0/default.jpg" % (IIIF, item["image_id"]), binary=True)
                time.sleep(1.0)
                if not picture or len(picture) < 2000:
                    continue
                seen.add(item["id"])
                with open(os.path.join(out, kind, "%d.jpg" % item["id"]), "wb") as f:
                    f.write(picture)
                thumb = item.get("thumbnail") or {}
                listing.append({"id": item["id"], "kind": kind, "title": item.get("title"), "artist": artist,
                                "date": item.get("date_display"), "image_id": item["image_id"],
                                "w": thumb.get("width"), "h": thumb.get("height"),
                                "class": item.get("classification_title")})
                kept += 1
                took += 1
            print("%-9s %-48s %2d found, %d kept" % (kind, query, len(found), took))
    with open(os.path.join(out, "candidatos.json"), "w") as f:
        json.dump(listing, f, indent=1, ensure_ascii=False)
    print("\n%d pictures in %s" % (len(listing), out))
    return 0 if listing else 1


def get(url, binary=False):
    req = urllib.request.Request(url, headers=HEADERS)
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                data = r.read()
                return data if binary else json.loads(data.decode("utf-8"))
        except urllib.error.HTTPError as e:
            if e.code in (403, 404):
                return None
            time.sleep(3 + attempt * 3)
        except (urllib.error.URLError, OSError, ValueError):
            time.sleep(3 + attempt * 3)
    return None


def search(query):
    url = "%s?q=%s&%s=true&fields=%s&limit=%d" % (
        API, urllib.parse.quote(query), urllib.parse.quote("query[term][is_public_domain]", safe=""), FIELDS, PER_QUERY)
    answer = get(url)
    time.sleep(1.0)
    return (answer or {}).get("data", [])


def upright(item):
    """Taller than wide, near enough the panel's shape to fill it without losing much."""
    thumb = item.get("thumbnail") or {}
    w, h = thumb.get("width") or 0, thumb.get("height") or 0
    return w > 0 and 1.2 <= h / w <= 2.2


def cmd_candidates():
    out = os.path.join(os.getcwd(), "fondos_candidatos")
    os.makedirs(out, exist_ok=True)
    listing, seen = [], set()
    for kind, queries in KINDS.items():
        kept = 0
        os.makedirs(os.path.join(out, kind), exist_ok=True)
        for query in queries:
            if kept >= PER_KIND:
                break
            found = search(query)
            print("%-9s %-30s %d found" % (kind, query, len(found)))
            for item in found:
                if kept >= PER_KIND:
                    break
                if item["id"] in seen or not item.get("image_id") or not item.get("is_public_domain") or not upright(item):
                    continue
                picture = get("%s/%s/full/240,/0/default.jpg" % (IIIF, item["image_id"]), binary=True)
                time.sleep(1.0)
                if not picture or len(picture) < 2000:
                    continue
                seen.add(item["id"])
                with open(os.path.join(out, kind, "%d.jpg" % item["id"]), "wb") as f:
                    f.write(picture)
                thumb = item.get("thumbnail") or {}
                listing.append({"id": item["id"], "kind": kind, "title": item.get("title"),
                                "artist": item.get("artist_title"), "date": item.get("date_display"),
                                "image_id": item["image_id"], "w": thumb.get("width"), "h": thumb.get("height"),
                                "class": item.get("classification_title")})
                kept += 1
        print("%-9s kept %d" % (kind, kept))
    with open(os.path.join(out, "candidatos.json"), "w") as f:
        json.dump(listing, f, indent=1, ensure_ascii=False)
    print("\n%d pictures in %s" % (len(listing), out))
    return 0 if listing else 1


def cmd_fetch(ids):
    by_id = {}
    for folder in ("fondos_candidatos", "fondos_candidatos2"):
        src = os.path.join(os.getcwd(), folder, "candidatos.json")
        if os.path.exists(src):
            by_id.update({str(c["id"]): c for c in json.load(open(src))})
    if not by_id:
        print("Run `candidates` first.")
        return 1
    out = os.path.join(os.getcwd(), "fondos_elegidos")
    os.makedirs(out, exist_ok=True)
    chosen = []
    for one in ids:
        c = by_id.get(str(one))
        if not c:
            print("%s: not among the candidates" % one)
            continue
        # Twice the panel's height, so there is something to crop and dither from.
        picture = get("%s/%s/full/,1600/0/default.jpg" % (IIIF, c["image_id"]), binary=True) or \
            get("%s/%s/full/843,/0/default.jpg" % (IIIF, c["image_id"]), binary=True)
        time.sleep(1.0)
        if not picture:
            print("%s: the museum did not send it" % one)
            continue
        with open(os.path.join(out, "%s.jpg" % one), "wb") as f:
            f.write(picture)
        chosen.append(c)
        print("%s  %s (%d KB)" % (one, c.get("title"), len(picture) // 1024))
    with open(os.path.join(out, "elegidos.json"), "w") as f:
        json.dump(chosen, f, indent=1, ensure_ascii=False)
    print("\n%d pictures in %s" % (len(chosen), out))
    return 0


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "candidates":
        sys.exit(cmd_candidates())
    if len(sys.argv) >= 2 and sys.argv[1] == "more":
        sys.exit(cmd_more())
    if len(sys.argv) >= 3 and sys.argv[1] == "fetch":
        sys.exit(cmd_fetch(sys.argv[2:]))
    print(__doc__)
    sys.exit(2)

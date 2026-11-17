#!/usr/bin/env python3
"""Liner Notes: builds the content pack the LINER NOTES app reads from the card.

    python3 ln_pack.py --selection selection.json --profiles profiles/ --out linernotes/

Copy the resulting folder to the root of the SD card as /linernotes.

The pack is yours: it is made from your own library and your own notes, and it
never goes in this repository. This script is the only public part.

Input
  selection.json  a list of albums: id, artist, title, year, label, plexKeys[]
  profiles/*.json one per album: categories[], evidence{}, connections[], sources[],
                  tracks[], and es{} / en{} with why, context, sound, listen[], curios[]

Output (format version 1)
  index.tsv   line 1: LINERNOTES <tab> 1 <tab> count <tab> built
              then one album per line, sorted by artist:
              id, artist, title, year, label, category bits, plex key,
              es offset, es length, en offset, en length
  es.txt      the profiles in Spanish, one record after another
  en.txt      the same in English

A record is lines of "K<tab>text": W why, C context, S sound, L a thing to listen
for, T an essential track (title <tab> note), U a curiosity, E a category
(name <tab> why), X a related album (its line number in the index <tab> note),
R a source (title <tab> url). The app reads one record at a time by offset, so the
whole pack is never in memory.
"""
import argparse, datetime, glob, json, os, sys, unicodedata

CATEGORIES = ["Essential", "Influential", "Cult classic", "Hidden gem", "Experimental", "Genre-defining",
              "Collector's discovery"]
CATEGORIES_ES = {"Essential": "Esencial", "Influential": "Influyente", "Cult classic": "De culto",
                 "Hidden gem": "Joya oculta", "Experimental": "Experimental", "Genre-defining": "Define un género",
                 "Collector's discovery": "Hallazgo de coleccionista"}

# The reading face on the device covers U+0020..7E and U+00A0..FF. Everything
# else is folded to something inside that, or dropped.
PLAIN = {0x2018: "'", 0x2019: "'", 0x201C: '"', 0x201D: '"', 0x2010: "-", 0x2011: "-", 0x2012: "-", 0x2013: "-",
         0x2014: "-", 0x2026: "...", 0x0131: "i", 0x0153: "oe", 0x0152: "OE", 0x09: " ", 0x0A: " ", 0x0D: " "}
CYRILLIC = dict(zip("абвгдеёжзийклмнопрстуфхцчшщъыьэюя",
                    ["a", "b", "v", "g", "d", "e", "yo", "zh", "z", "i", "y", "k", "l", "m", "n", "o", "p", "r", "s", "t",
                     "u", "f", "kh", "ts", "ch", "sh", "shch", "", "y", "", "e", "yu", "ya"]))


def fold(text):
    out = []
    for ch in str(text):
        if ord(ch) in PLAIN:
            out.append(PLAIN[ord(ch)])
        elif 0x20 <= ord(ch) <= 0x7E or 0xA0 <= ord(ch) <= 0xFF:
            out.append(ch)
        elif ch.lower() in CYRILLIC:
            t = CYRILLIC[ch.lower()]
            out.append(t.capitalize() if ch.isupper() else t)
        else:
            base = unicodedata.normalize("NFKD", ch).encode("ascii", "ignore").decode()
            out.append(base)
    return " ".join("".join(out).split())


def record(profile, lang, line_of):
    t = profile[lang]
    rows = [("W", t["why"]), ("C", t["context"]), ("S", t["sound"])]
    rows += [("L", x) for x in t.get("listen", [])]
    rows += [("T", fold(x["title"]) + "\t" + fold(x[lang])) for x in profile.get("tracks", [])]
    rows += [("U", x) for x in t.get("curios", [])]
    for cat in profile.get("categories", []):
        name = CATEGORIES_ES[cat] if lang == "es" else cat
        rows.append(("E", name + "\t" + fold(profile.get("evidence", {}).get(cat, ""))))
    for c in profile.get("connections", []):
        if c["id"] in line_of:
            rows.append(("X", "%d\t%s" % (line_of[c["id"]], fold(c[lang]))))
    for s in profile.get("sources", []):
        rows.append(("R", fold(s["title"]) + "\t" + s["url"]))
    return "".join("%s\t%s\n" % (k, v if "\t" in v else fold(v)) for k, v in rows).encode("utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selection", required=True)
    ap.add_argument("--profiles", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    selection = {s["id"]: s for s in json.load(open(a.selection, encoding="utf-8"))}
    profiles = [json.load(open(p, encoding="utf-8")) for p in sorted(glob.glob(os.path.join(a.profiles, "*.json")))]
    profiles = [p for p in profiles if p["id"] in selection]
    if not profiles:
        sys.exit("no profiles match the selection")
    key = lambda p: (fold(selection[p["id"]]["artist"]).lower().removeprefix("the "), selection[p["id"]]["year"] or 0)
    profiles.sort(key=key)
    line_of = {p["id"]: i for i, p in enumerate(profiles)}
    os.makedirs(a.out, exist_ok=True)
    blobs = {"es": bytearray(), "en": bytearray()}
    lines = ["LINERNOTES\t1\t%d\t%s" % (len(profiles), datetime.date.today().isoformat())]
    for p in profiles:
        s = selection[p["id"]]
        bits = sum(1 << CATEGORIES.index(c) for c in p.get("categories", []) if c in CATEGORIES)
        spans = []
        for lang in ("es", "en"):
            rec = record(p, lang, line_of)
            spans += [len(blobs[lang]), len(rec)]
            blobs[lang] += rec
        plex = (s.get("plexKeys") or [""])[0]
        lines.append("\t".join([p["id"], fold(s["artist"]), fold(s["title"]), str(s.get("year") or 0),
                                fold(s.get("label") or ""), str(bits), str(plex)] + [str(n) for n in spans]))
    open(os.path.join(a.out, "index.tsv"), "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    for lang, blob in blobs.items():
        open(os.path.join(a.out, lang + ".txt"), "wb").write(blob)
    print("%d albums, es %d bytes, en %d bytes, in %s" % (len(profiles), len(blobs["es"]), len(blobs["en"]), a.out))


if __name__ == "__main__":
    main()

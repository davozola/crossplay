#!/usr/bin/env python3
"""Fetches the pictures the wallpaper pack is made from, for make_pack.py.

    python3 tools_local/wallpapers/fetch_originals.py <folder> [id ...]

elegidos.json, beside this file, lists every picture in the pack with the
museum's image id. This downloads each one from the Art Institute of Chicago's
IIIF server at twice the panel's height (the size aic_fondos.py chose them at)
into <folder>/<id>.jpg and copies the list beside them, which is the folder
make_pack.py asks for. Given ids, only those. A picture already there is kept.
"""
import json
import os
import shutil
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
IIIF = "https://www.artic.edu/iiif/2"
# The museum asks API users to say who they are.
AGENT = "crossplay-fondos (https://github.com/davozola/crossplay)"


def get(url):
    for attempt in range(3):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": AGENT, "AIC-User-Agent": AGENT})
            with urllib.request.urlopen(req, timeout=60) as r:
                return r.read()
        except Exception as error:  # noqa: BLE001 - any failure is "try again, then say so"
            print("  %s (%s)" % (error, url))
            time.sleep(3 * (attempt + 1))
    return None


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    folder = sys.argv[1]
    only = set(sys.argv[2:])
    os.makedirs(folder, exist_ok=True)
    chosen = json.load(open(os.path.join(HERE, "elegidos.json"), encoding="utf-8"))
    missing = 0
    for c in chosen:
        if only and str(c["id"]) not in only:
            continue
        path = os.path.join(folder, "%s.jpg" % c["id"])
        if os.path.exists(path) and os.path.getsize(path) > 1024:
            continue
        data = get("%s/%s/full/,1600/0/default.jpg" % (IIIF, c["image_id"])) or get(
            "%s/%s/full/843,/0/default.jpg" % (IIIF, c["image_id"]))
        if not data:
            print("%s: the museum did not send it" % c["id"])
            missing += 1
            continue
        with open(path, "wb") as f:
            f.write(data)
        print("%s  %d KB" % (c["id"], len(data) // 1024))
        time.sleep(1.0)
    shutil.copyfile(os.path.join(HERE, "elegidos.json"), os.path.join(folder, "elegidos.json"))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Plexamp Jukebox, phase A: can a Plexamp player be watched and controlled
from another machine on the same network, with nothing installed on it?

Runs on a development computer, independently of the firmware, with nothing
but the Python standard library. It signs in with a link code (no password and
no token typed anywhere), finds your Plex Media Servers and players, reads
what is playing, fetches the artwork, and tries the player's own control port.

    python3 plex_diag.py login      # shows a code to type at plex.tv/link
    python3 plex_diag.py report     # everything, read-only
    python3 plex_diag.py control    # pause/play and next/previous on a player
    python3 plex_diag.py probe      # volume, seek, shuffle, repeat: each tried and put back
    python3 plex_diag.py dump       # raw answers, secrets removed, into ./plex_dump
    python3 plex_diag.py logout     # forget the token

The token is kept in ~/.config/crossplay-plexdiag/state.json (mode 600),
outside any repository, and is never printed: only its last four characters.

What each step proves is stated in the report as VERIFIED, FAILED or NOT
TESTED. Nothing is assumed to work.
"""

import argparse
import json
import os
import socket
import ssl
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
import xml.etree.ElementTree as ET

PRODUCT = "CrossPlay Jukebox Diagnostic"
VERSION = "0.1"
STATE_DIR = os.path.join(os.path.expanduser("~"), ".config", "crossplay-plexdiag")
STATE_PATH = os.path.join(STATE_DIR, "state.json")
PLEX_TV = "https://plex.tv"
PLAYER_PORT = 32500  # Plexamp's own control port, when remote control is on

results = []  # (status, what, detail), for the summary at the end


def note(status, what, detail=""):
    results.append((status, what, detail))
    print("  [%s] %s%s" % (status, what, (": " + detail) if detail else ""))


def mask(secret):
    return "…" + secret[-4:] if secret and len(secret) > 8 else "(hidden)"


def load_state():
    try:
        with open(STATE_PATH) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_state(state):
    os.makedirs(STATE_DIR, exist_ok=True)
    tmp = STATE_PATH + ".tmp"
    with open(tmp, "w") as f:
        json.dump(state, f)
    os.chmod(tmp, 0o600)
    os.replace(tmp, STATE_PATH)


def client_id(state):
    if "client" not in state:
        state["client"] = str(uuid.uuid4())
        save_state(state)
    return state["client"]


def headers(state, token=None, extra=None):
    h = {
        "Accept": "application/json",
        "X-Plex-Client-Identifier": client_id(state),
        "X-Plex-Product": PRODUCT,
        "X-Plex-Version": VERSION,
        "X-Plex-Device-Name": "CrossPlay diagnostic",
        "X-Plex-Platform": "Python",
    }
    if token:
        h["X-Plex-Token"] = token
    if extra:
        h.update(extra)
    return h


def fetch(url, hdrs, method="GET", timeout=6, insecure=False):
    """Returns (status, body bytes, content type). Network errors are (0, message, '')."""
    req = urllib.request.Request(url, headers=hdrs, method=method)
    ctx = None
    if url.startswith("https") and insecure:
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
    try:
        with urllib.request.urlopen(req, timeout=timeout, context=ctx) as r:
            return r.status, r.read(), r.headers.get("Content-Type", "")
    except urllib.error.HTTPError as e:
        return e.code, e.read() or b"", e.headers.get("Content-Type", "")
    except (urllib.error.URLError, socket.timeout, ConnectionError, OSError) as e:
        return 0, str(getattr(e, "reason", e)).encode(), ""


def as_json(body):
    try:
        return json.loads(body.decode("utf-8", "replace"))
    except ValueError:
        return None


# --- Signing in: a link code, the way a TV app does it ----------------------


def cmd_login(args):
    state = load_state()
    if not args.resume:
        code, body, _ = fetch(PLEX_TV + "/api/v2/pins?strong=false", headers(state), method="POST")
        pin = as_json(body) if code in (200, 201) else None
        if not pin:
            print("Could not ask plex.tv for a link code (HTTP %s)." % code)
            return 1
        state["pin"] = pin["id"]
        save_state(state)
        print("\n  On a phone or computer, open   https://plex.tv/link")
        print("  sign in, and type this code:   %s\n" % pin["code"])
    if "pin" not in state:
        print("No link code is pending. Run `login` first.")
        return 1
    deadline = time.time() + args.wait
    print("Waiting up to %d s for the code to be entered…" % args.wait)
    while time.time() < deadline:
        code, body, _ = fetch(PLEX_TV + "/api/v2/pins/%s" % state["pin"], headers(state))
        pin = as_json(body) if code == 200 else None
        if code == 404:
            print("That code has expired. Run `login` again.")
            state.pop("pin", None)
            save_state(state)
            return 1
        if pin and pin.get("authToken"):
            state["token"] = pin["authToken"]
            state.pop("pin", None)
            save_state(state)
            print("Signed in. Token stored (%s)." % mask(state["token"]))
            return 0
        time.sleep(2)
    print("Not entered yet. Run `login --resume` to keep waiting for the same code.")
    return 2


def cmd_logout(_args):
    state = load_state()
    state.pop("token", None)
    state.pop("pin", None)
    save_state(state)
    print("Token forgotten on this computer. To revoke it everywhere, remove")
    print("'%s' under plex.tv > Account > Authorized devices." % PRODUCT)
    return 0


# --- Finding servers and players --------------------------------------------


def resources(state):
    code, body, _ = fetch(PLEX_TV + "/api/v2/resources?includeHttps=1&includeRelay=1", headers(state, state["token"]),
                          timeout=12)
    if code == 401:
        note("FAILED", "plex.tv rejected the stored token", "sign in again with `login`")
        return None
    data = as_json(body) if code == 200 else None
    if data is None:
        note("FAILED", "listing your devices at plex.tv", "HTTP %s" % code)
    return data


def reach_server(state, device):
    """The first of a server's addresses that answers, local ones first."""
    token = device.get("accessToken") or state["token"]
    conns = sorted(device.get("connections", []), key=lambda c: (not c.get("local"), bool(c.get("relay"))))
    tried = []
    for c in conns:
        for uri in dict.fromkeys([c.get("uri"), "http://%s:%s" % (c.get("address"), c.get("port"))]):
            if not uri:
                continue
            code, body, _ = fetch(uri + "/identity", headers(state, token), timeout=4)
            tried.append("%s -> %s" % (uri.split("//")[1][:40], code or body.decode()[:40]))
            if code == 200:
                kind = "relay" if c.get("relay") else ("local" if c.get("local") else "remote")
                return uri, token, kind, tried
    return None, token, None, tried


def sessions(state, base, token):
    code, body, _ = fetch(base + "/status/sessions", headers(state, token), timeout=8)
    if code != 200:
        return None, code
    return (as_json(body) or {}).get("MediaContainer", {}).get("Metadata", []), code


def server_clients(state, base, token):
    code, body, _ = fetch(base + "/clients", headers(state, token), timeout=8)
    if code != 200:
        return []
    return (as_json(body) or {}).get("MediaContainer", {}).get("Server", [])


def gdm_players(timeout=3.0):
    """Plex's own LAN discovery: players answer a multicast M-SEARCH on 32412."""
    found = []
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
        s.settimeout(timeout)
        for group in ("239.0.0.250", "255.255.255.255"):
            try:
                if group.startswith("255"):
                    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
                s.sendto(b"M-SEARCH * HTTP/1.1\r\n\r\n", (group, 32412))
            except OSError:
                pass
        end = time.time() + timeout
        while time.time() < end:
            try:
                data, addr = s.recvfrom(2048)
            except socket.timeout:
                break
            fields = {}
            for line in data.decode("utf-8", "replace").splitlines()[1:]:
                if ":" in line:
                    k, v = line.split(":", 1)
                    fields[k.strip().lower()] = v.strip()
            found.append({"address": addr[0], "port": fields.get("port"), "name": fields.get("name"),
                          "product": fields.get("product"), "id": fields.get("resource-identifier")})
        s.close()
    except OSError as e:
        return None, str(e)
    return found, None


# --- A player's own port -----------------------------------------------------

command_id = int(time.time()) % 100000


def player_headers(state, token, target=None):
    extra = {"Accept": "*/*"}
    if target:
        extra["X-Plex-Target-Client-Identifier"] = target
    return headers(state, token, extra)


def timeline(state, host, port, token, target=None):
    """What the player says it is doing: (attributes of its music timeline, error)."""
    global command_id
    command_id += 1
    url = "http://%s:%s/player/timeline/poll?wait=0&includeMetadata=1&commandID=%d" % (host, port, command_id)
    code, body, _ = fetch(url, player_headers(state, token, target), timeout=5)
    if code != 200:
        return None, "HTTP %s %s" % (code, body.decode("utf-8", "replace")[:60] if code == 0 else "")
    try:
        root = ET.fromstring(body)
    except ET.ParseError:
        return None, "unreadable answer"
    for t in root.iter("Timeline"):
        if t.get("type") == "music":
            info = dict(t.attrib)
            track = t.find("Track")
            if track is not None:
                info["_title"] = track.get("title")
                info["_artist"] = track.get("grandparentTitle")
                info["_album"] = track.get("parentTitle")
                info["_thumb"] = track.get("thumb") or track.get("parentThumb")
            return info, None
    return {}, None


def command(state, host, port, token, name, target=None, params=""):
    global command_id
    command_id += 1
    url = "http://%s:%s/player/playback/%s?type=music&commandID=%d%s" % (host, port, name, command_id, params)
    code, body, _ = fetch(url, player_headers(state, token, target), timeout=5)
    return code, body.decode("utf-8", "replace")[:80]


def fmt_ms(ms):
    try:
        s = int(ms) // 1000
    except (TypeError, ValueError):
        return "?"
    return "%d:%02d" % (s // 60, s % 60)


# --- The read-only report ----------------------------------------------------


def survey(state, quiet=False):
    """Everything that can be learned without changing anything. Returns the
    players found, each with the server whose token and artwork go with it."""
    data = resources(state)
    if data is None:
        return []
    servers = [d for d in data if "server" in (d.get("provides") or "")]
    cloud_players = [d for d in data if "player" in (d.get("provides") or "")]
    print("\n1. Servers on this account: %d" % len(servers))
    players = {}

    def add_player(pid, **kw):
        if not pid:
            return
        p = players.setdefault(pid, {"id": pid, "addresses": []})
        for k, v in kw.items():
            if k == "address":
                if v and v not in p["addresses"]:
                    p["addresses"].append(v)
            elif v and not p.get(k):
                p[k] = v

    for d in cloud_players:
        add_player(d.get("clientIdentifier"), name=d.get("name"), product=d.get("product"),
                   platform=d.get("platform"), source="plex.tv")
        for c in d.get("connections", []):
            add_player(d.get("clientIdentifier"), address="%s:%s" % (c.get("address"), c.get("port")))

    reached = []
    for d in servers:
        base, token, kind, tried = reach_server(state, d)
        owner = "yours" if d.get("owned") else "shared with you"
        if not base:
            note("FAILED", "reach server '%s' (%s)" % (d.get("name"), owner), "; ".join(tried)[:160])
            continue
        note("VERIFIED", "server '%s' (%s) answers over a %s address" % (d.get("name"), owner, kind))
        reached.append((d, base, token))

    print("\n2. What is playing (the server's /status/sessions)")
    any_audio = False
    for d, base, token in reached:
        items, code = sessions(state, base, token)
        if items is None:
            note("FAILED", "sessions on '%s'" % d.get("name"),
                 "HTTP %s%s" % (code, " (only the server's owner may read sessions)" if code in (401, 403) else ""))
            continue
        tracks = [m for m in items if m.get("type") == "track"]
        note("VERIFIED", "sessions on '%s' can be read" % d.get("name"),
             "%d playing, %d of them music" % (len(items), len(tracks)))
        for m in tracks:
            any_audio = True
            pl = m.get("Player", {})
            print("       %s / %s / %s" % (m.get("grandparentTitle"), m.get("parentTitle"), m.get("title")))
            print("       %s / %s, %s, on '%s' (%s, %s)" % (fmt_ms(m.get("viewOffset")), fmt_ms(m.get("duration")),
                                                          pl.get("state"), pl.get("title"), pl.get("product"),
                                                          pl.get("platform")))
            add_player(pl.get("machineIdentifier"), name=pl.get("title"), product=pl.get("product"),
                       platform=pl.get("platform"), source="a session", server=(base, token),
                       address=("%s:%d" % (pl.get("address"), PLAYER_PORT)) if pl.get("address") else None,
                       thumb=m.get("thumb") or m.get("parentThumb"))
        for c in server_clients(state, base, token):
            add_player(c.get("machineIdentifier"), name=c.get("name"), product=c.get("product"),
                       source="the server's /clients", server=(base, token),
                       address="%s:%s" % (c.get("host") or c.get("address"), c.get("port")))
    if not any_audio:
        note("NOT TESTED", "reading a track", "nothing is playing music right now; start a track on the player")

    print("\n3. Players")
    found, err = gdm_players()
    if found is None:
        note("NOT TESTED", "LAN discovery (GDM)", err)
    else:
        note("VERIFIED" if found else "NOT TESTED", "LAN discovery (GDM) answered by %d player(s)" % len(found),
             "" if found else "none answered; this machine may not be on the players' network")
        for f in found:
            add_player(f["id"], name=f["name"], product=f["product"], source="LAN discovery",
                       address="%s:%s" % (f["address"], f["port"]))
    for extra in state.get("manual_players", []):
        add_player("manual-" + extra, name=extra, source="typed by hand", address=extra)

    default_server = reached[0][1:] if reached else None
    for p in players.values():
        p.setdefault("server", default_server)
        print("   - %s  [%s, %s]  found via %s" % (p.get("name"), p.get("product"), p.get("platform", "?"),
                                                   p.get("source")))
        p["control"] = None
        token = p["server"][1] if p.get("server") else state["token"]
        for addr in p["addresses"]:
            host, _, port = addr.partition(":")
            if not host or host == "None":
                continue
            target = None if p["id"].startswith("manual-") else p["id"]
            info, err = timeline(state, host, port or PLAYER_PORT, token, target)
            if info is None:
                print("       %s: no answer (%s)" % (addr, err))
                continue
            p["control"] = (host, port or PLAYER_PORT, token, target)
            p["timeline"] = info
            note("VERIFIED", "'%s' answers on its own port at %s" % (p.get("name"), addr),
                 "state=%s %s/%s volume=%s" % (info.get("state"), fmt_ms(info.get("time")),
                                                fmt_ms(info.get("duration")), info.get("volume")))
            if info.get("_title"):
                print("       its own account: %s / %s / %s" % (info.get("_artist"), info.get("_album"),
                                                                info.get("_title")))
            break
        if p["addresses"] and not p["control"]:
            note("FAILED", "'%s' has no reachable control port" % p.get("name"),
                 "tried %s; is Remote Control on in Plexamp's settings, and is this machine on its network?"
                 % ", ".join(p["addresses"]))
        if not p["addresses"]:
            note("NOT TESTED", "control port of '%s'" % p.get("name"), "no address is known for it")

    print("\n4. Artwork")
    done = False
    for p in players.values():
        thumb = p.get("thumb") or (p.get("timeline") or {}).get("_thumb")
        if not thumb or not p.get("server"):
            continue
        base, token = p["server"]
        url = "%s/photo/:/transcode?width=320&height=320&minSize=1&upscale=1&url=%s" % (
            base, urllib.parse.quote(thumb, safe=""))
        code, body, ctype = fetch(url, headers(state, token, {"Accept": "image/jpeg"}), timeout=10)
        if code == 200 and body[:2] == b"\xff\xd8":
            out = os.path.join(STATE_DIR, "artwork.jpg")
            with open(out, "wb") as f:
                f.write(body)
            note("VERIFIED", "album artwork, resized by the server to 320x320", "%d bytes JPEG, saved to %s"
                 % (len(body), out))
        else:
            note("FAILED", "album artwork", "HTTP %s, %s" % (code, ctype))
        done = True
        break
    if not done:
        note("NOT TESTED", "album artwork", "needs a track playing")
    return list(players.values())


def summary():
    print("\n" + "=" * 62 + "\nSummary\n" + "=" * 62)
    for status in ("VERIFIED", "FAILED", "NOT TESTED"):
        rows = [r for r in results if r[0] == status]
        if rows:
            print("\n%s" % status)
            for _, what, detail in rows:
                print("  - %s%s" % (what, (" (" + detail + ")") if detail else ""))


def need_token(state):
    if state.get("token"):
        return True
    print("Not signed in. Run:  python3 plex_diag.py login")
    return False


def cmd_report(_args):
    state = load_state()
    if not need_token(state):
        return 1
    print("Account token in use: %s" % mask(state["token"]))
    survey(state)
    summary()
    return 0


# --- Control: the test that changes something -------------------------------


def wait_state(state, ctl, want, seconds=6):
    end = time.time() + seconds
    last = None
    while time.time() < end:
        info, _ = timeline(state, *ctl)
        if info:
            last = info
            if want(info):
                return True, info
        time.sleep(0.6)
    return False, last


def cmd_control(args):
    state = load_state()
    if not need_token(state):
        return 1
    players = [p for p in survey(state) if p.get("control")]
    if args.player:
        players = [p for p in players if args.player.lower() in (p.get("name") or "").lower()]
    print("\n5. Control")
    if not players:
        note("FAILED", "control", "no player with a reachable control port" +
             (" matching '%s'" % args.player if args.player else ""))
        summary()
        return 1
    p = players[0]
    ctl = p["control"]
    before, _ = timeline(state, *ctl)
    if not before or before.get("state") not in ("playing", "paused"):
        note("NOT TESTED", "control of '%s'" % p.get("name"),
             "it is %s; start a track on it first" % (before or {}).get("state", "idle"))
        summary()
        return 1
    print("   Testing on '%s', which is %s. It will pause for about two seconds," % (p.get("name"), before["state"]))
    print("   skip forward one track and come back.")
    was_playing = before["state"] == "playing"

    first, second = ("pause", "play") if was_playing else ("play", "pause")
    for name, target_state in ((first, "paused" if was_playing else "playing"),
                               (second, "playing" if was_playing else "paused")):
        code, body = command(state, ctl[0], ctl[1], ctl[2], name, ctl[3])
        ok, info = wait_state(state, ctl, lambda i, t=target_state: i.get("state") == t)
        note("VERIFIED" if ok else "FAILED", "%s on '%s'" % (name, p.get("name")),
             "the player reports %s" % (info or {}).get("state") if ok else "HTTP %s %s; player still %s"
             % (code, body, (info or {}).get("state")))
        time.sleep(1.5)

    key = before.get("ratingKey") or before.get("key")
    for name in ("skipNext", "skipPrevious"):
        now, _ = timeline(state, *ctl)
        key = (now or {}).get("ratingKey") or key
        code, body = command(state, ctl[0], ctl[1], ctl[2], name, ctl[3])
        ok, info = wait_state(state, ctl, lambda i, k=key: (i.get("ratingKey") or i.get("key")) not in (None, k))
        note("VERIFIED" if ok else "FAILED", "%s on '%s'" % (name, p.get("name")),
             ("now on '%s'" % (info or {}).get("_title")) if ok else "HTTP %s %s; the track did not change"
             % (code, body))
        time.sleep(1.5)
    summary()
    return 0


# --- The less certain controls, each tried and put back -----------------------


def first_player(state, name=None):
    players = [p for p in survey(state) if p.get("control")]
    if name:
        players = [p for p in players if name.lower() in (p.get("name") or "").lower()]
    return players[0] if players else None


def cmd_probe(args):
    state = load_state()
    if not need_token(state):
        return 1
    p = first_player(state, args.player)
    print("\n5. Other controls")
    if not p:
        note("FAILED", "probe", "no player with a reachable control port")
        summary()
        return 1
    ctl = p["control"]
    before, _ = timeline(state, *ctl)
    if not before or before.get("state") != "playing":
        note("NOT TESTED", "probe", "needs a track playing, not %s" % (before or {}).get("state", "idle"))
        summary()
        return 1
    print("   On '%s'. Each setting is changed for a moment and put back." % p.get("name"))

    def setting(label, key, param, other, restore):
        code, body = command(state, ctl[0], ctl[1], ctl[2], "setParameters", ctl[3], "&%s=%s" % (param, other))
        ok, info = wait_state(state, ctl, lambda i: str(i.get(key)) == str(other), 5)
        note("VERIFIED" if ok else "FAILED", label,
             "the player reports %s=%s" % (key, (info or {}).get(key)) if ok
             else "HTTP %s %s; the player still reports %s=%s" % (code, body, key, (info or {}).get(key)))
        command(state, ctl[0], ctl[1], ctl[2], "setParameters", ctl[3], "&%s=%s" % (param, restore))
        time.sleep(1.0)

    if before.get("volume") is None:
        note("NOT TESTED", "volume", "the player does not report one")
    else:
        vol = int(float(before["volume"]))
        setting("volume", "volume", "volume", vol - 5 if vol >= 10 else vol + 5, vol)
    if before.get("shuffle") is None:
        note("NOT TESTED", "shuffle", "the player does not report it")
    else:
        setting("shuffle", "shuffle", "shuffle", "0" if before["shuffle"] == "1" else "1", before["shuffle"])
    if before.get("repeat") is None:
        note("NOT TESTED", "repeat", "the player does not report it")
    else:
        setting("repeat", "repeat", "repeat", "0" if before["repeat"] != "0" else "2", before["repeat"])

    now, _ = timeline(state, *ctl)
    try:
        at = int(now.get("time"))
        length = int(now.get("duration"))
    except (TypeError, ValueError, AttributeError):
        at = length = None
    if at is None or length < 40000:
        note("NOT TESTED", "seek", "needs a track longer than 40 seconds")
    else:
        target = at + 20000 if at + 30000 < length else at - 20000
        code, body = command(state, ctl[0], ctl[1], ctl[2], "seekTo", ctl[3], "&offset=%d" % target)
        ok, info = wait_state(state, ctl, lambda i: abs(int(i.get("time", -99999)) - target) < 6000, 5)
        note("VERIFIED" if ok else "FAILED", "seek within the track",
             "the player reports %s" % fmt_ms((info or {}).get("time")) if ok else "HTTP %s %s" % (code, body))
        command(state, ctl[0], ctl[1], ctl[2], "seekTo", ctl[3], "&offset=%d" % at)

    # The queue is the server's: the player only names it.
    queue = (now or {}).get("playQueueID")
    if not queue or not p.get("server"):
        note("NOT TESTED", "play queue", "the player names no queue")
    else:
        base, token = p["server"]
        code, body, _ = fetch("%s/playQueues/%s?window=20&own=1" % (base, queue), headers(state, token), timeout=8)
        items = ((as_json(body) or {}).get("MediaContainer", {}).get("Metadata", [])) if code == 200 else None
        note("VERIFIED" if items else "FAILED", "play queue read from the server",
             "%d tracks around the current one" % len(items) if items else "HTTP %s" % code)
    summary()
    return 0


# --- Raw answers, for building the firmware's parsers against the real thing ---

SECRET_KEYS = ("token", "accesstoken", "authtoken", "x-plex-token", "publicaddress", "email", "username",
               "ownerid", "sourcetitle", "home", "httpsrequired")


def scrub_json(value):
    if isinstance(value, dict):
        return {k: ("REDACTED" if k.lower() in SECRET_KEYS else scrub_json(v)) for k, v in value.items()}
    if isinstance(value, list):
        return [scrub_json(v) for v in value]
    return value


def scrub_xml(text):
    import re
    return re.sub(r'(?i)\b(token|X-Plex-Token|accessToken|authToken)="[^"]*"', r'\1="REDACTED"', text)


def cmd_dump(args):
    state = load_state()
    if not need_token(state):
        return 1
    out = os.path.join(os.getcwd(), "plex_dump")
    os.makedirs(out, exist_ok=True)

    def save(name, text):
        with open(os.path.join(out, name), "w") as f:
            f.write(text)
        print("  wrote plex_dump/%s (%d bytes)" % (name, len(text)))

    data = resources(state)
    if data is not None:
        save("resources.json", json.dumps(scrub_json(data), indent=1))
    p = first_player(state, args.player)
    if not p:
        print("No player with a reachable control port; only the account's answer was saved.")
        return 1
    host, port, token, target = p["control"]
    for name, path in (("timeline.xml", "/player/timeline/poll?wait=0&includeMetadata=1&commandID=1"),
                       ("timeline_plain.xml", "/player/timeline/poll?wait=0&commandID=2"),
                       ("player_resources.xml", "/resources")):
        code, body, ctype = fetch("http://%s:%s%s" % (host, port, path), player_headers(state, token, target))
        save(name, "<!-- HTTP %s, %s -->\n%s" % (code, ctype, scrub_xml(body.decode("utf-8", "replace"))))
    if p.get("server"):
        base, stoken = p["server"]
        code, body, _ = fetch(base + "/status/sessions", headers(state, stoken), timeout=8)
        if code == 200:
            save("sessions.json", json.dumps(scrub_json(as_json(body)), indent=1))
        info = p.get("timeline") or {}
        if info.get("playQueueID"):
            code, body, _ = fetch("%s/playQueues/%s?window=10&own=1" % (base, info["playQueueID"]),
                                  headers(state, stoken), timeout=8)
            if code == 200:
                save("playqueue.json", json.dumps(scrub_json(as_json(body)), indent=1))
    print("\nTokens, e-mail and public addresses are replaced with REDACTED. Local addresses and")
    print("your library's track names are in these files: look them over before sharing them.")
    return 0


def cmd_library(args):
    """Browsing, jumping within the queue, and starting an album: what the firmware's next step needs."""
    state = load_state()
    if not need_token(state):
        return 1
    p = first_player(state, args.player)
    print("\n6. Library, queue jump and starting playback")
    if not p or not p.get("server"):
        note("FAILED", "library", "no player with a reachable control port and a server behind it")
        summary()
        return 1
    ctl = p["control"]
    base, stoken = p["server"]
    before, _ = timeline(state, *ctl)
    if not before or before.get("state") != "playing":
        note("NOT TESTED", "library", "needs a track playing, so there is something to put back")
        summary()
        return 1
    out = os.path.join(os.getcwd(), "plex_dump")
    os.makedirs(out, exist_ok=True)

    def get(path, name=None):
        code, body, _ = fetch(base + path, headers(state, stoken), timeout=10)
        data = as_json(body) if code == 200 else None
        if data is not None and name:
            with open(os.path.join(out, name), "w") as f:
                json.dump(scrub_json(data), f, indent=1)
        return code, (data or {}).get("MediaContainer", {})

    page = "X-Plex-Container-Start=0&X-Plex-Container-Size=6"
    code, mc = get("/library/sections", "sections.json")
    music = [d for d in mc.get("Directory", []) if d.get("type") == "artist"]
    note("VERIFIED" if music else "FAILED", "music libraries listed",
         "%d found" % len(music) if music else "HTTP %s" % code)
    if not music:
        summary()
        return 1
    sec = music[0]["key"]
    code, mc = get("/library/sections/%s/recentlyAdded?type=9&%s" % (sec, page), "albums_recent.json")
    albums = mc.get("Metadata", [])
    note("VERIFIED" if albums else "FAILED", "recently added albums", "%d read" % len(albums) if albums else "HTTP %s" % code)
    code, mc = get("/library/sections/%s/all?type=8&%s" % (sec, page), "artists.json")
    artists = mc.get("Metadata", [])
    note("VERIFIED" if artists else "FAILED", "artists, a page at a time",
         "%d of %s" % (len(artists), mc.get("totalSize", "?")) if artists else "HTTP %s" % code)
    if artists:
        code, mc = get("/library/metadata/%s/children" % artists[0]["ratingKey"], "artist_albums.json")
        note("VERIFIED" if mc.get("Metadata") else "FAILED", "an artist's albums",
             "%d read" % len(mc.get("Metadata", [])) if mc.get("Metadata") else "HTTP %s" % code)
    code, mc = get("/playlists?playlistType=audio&%s" % page, "playlists.json")
    note("VERIFIED" if code == 200 else "FAILED", "audio playlists", "%d read" % len(mc.get("Metadata", [])) if code == 200 else "HTTP %s" % code)
    tracks = []
    if albums:
        code, mc = get("/library/metadata/%s/children" % albums[0]["ratingKey"], "album_tracks.json")
        tracks = mc.get("Metadata", [])
        note("VERIFIED" if tracks else "FAILED", "an album's tracks", "%d read" % len(tracks) if tracks else "HTTP %s" % code)

    def key_now(info):
        return str((info or {}).get("ratingKey"))

    # Jumping to another track of the queue that is already playing.
    orig_key, orig_queue = key_now(before), before.get("playQueueID")
    try:
        orig_at = int(before.get("time") or 0)
    except ValueError:
        orig_at = 0
    code, mc = get("/playQueues/%s?window=10&own=1" % orig_queue)
    others = [m for m in mc.get("Metadata", []) if str(m.get("ratingKey")) != orig_key]
    if not others:
        note("NOT TESTED", "jump within the queue", "the queue has one track")
    else:
        want = str(others[-1]["ratingKey"])
        code, body = command(state, ctl[0], ctl[1], ctl[2], "skipTo", ctl[3], "&key=%s" % urllib.parse.quote("/library/metadata/" + want, safe=""))
        ok, info = wait_state(state, ctl, lambda i: key_now(i) == want, 6)
        note("VERIFIED" if ok else "FAILED", "jump within the queue (skipTo)",
             "the player moved to the track asked for" if ok else "HTTP %s %s" % (code, body))
        command(state, ctl[0], ctl[1], ctl[2], "skipTo", ctl[3], "&key=%s" % urllib.parse.quote("/library/metadata/" + orig_key, safe=""))
        wait_state(state, ctl, lambda i: key_now(i) == orig_key, 6)
        command(state, ctl[0], ctl[1], ctl[2], "seekTo", ctl[3], "&offset=%d" % orig_at)

    # Starting something new, then putting back what was playing.
    machine = before.get("machineIdentifier")

    def play(queue_id, track_key, offset):
        params = "&providerIdentifier=com.plexapp.plugins.library&machineIdentifier=%s&protocol=%s&address=%s&port=%s" % (
            machine, before.get("protocol"), before.get("address"), before.get("port"))
        params += "&containerKey=%s&key=%s&offset=%d&token=%s" % (
            urllib.parse.quote("/playQueues/%s?own=1" % queue_id, safe=""),
            urllib.parse.quote("/library/metadata/%s" % track_key, safe=""), offset, stoken)
        return command(state, ctl[0], ctl[1], ctl[2], "playMedia", ctl[3], params)

    if not albums or not tracks or not args.start:
        note("NOT TESTED", "start an album", "run again with --start: it plays a few seconds of '%s' and then puts your music back"
             % (albums[0].get("title") if albums else "an album"))
    else:
        uri = "server://%s/com.plexapp.plugins.library/library/metadata/%s" % (machine, albums[0]["ratingKey"])
        code, body, _ = fetch("%s/playQueues?type=audio&shuffle=0&repeat=0&continuous=0&own=1&uri=%s" % (
            base, urllib.parse.quote(uri, safe="")), headers(state, stoken), method="POST", timeout=10)
        made = (as_json(body) or {}).get("MediaContainer", {}) if code == 200 else {}
        with open(os.path.join(out, "playqueue_created.json"), "w") as f:
            json.dump(scrub_json({"MediaContainer": made}), f, indent=1)
        new_queue = made.get("playQueueID")
        note("VERIFIED" if new_queue else "FAILED", "play queue made on the server", "for one album" if new_queue else "HTTP %s" % code)
        if new_queue:
            first = str(tracks[0]["ratingKey"])
            code, body = play(new_queue, first, 0)
            ok, info = wait_state(state, ctl, lambda i: key_now(i) == first and i.get("state") == "playing", 10)
            note("VERIFIED" if ok else "FAILED", "start an album on the player (playMedia)",
                 "the player is playing its first track" if ok else "HTTP %s %s; the player reports %s" % (code, body, key_now(info)))
            time.sleep(3)
            code, body = play(orig_queue, orig_key, orig_at)
            ok, _ = wait_state(state, ctl, lambda i: key_now(i) == orig_key, 10)
            note("VERIFIED" if ok else "FAILED", "your music put back", "same track, same place" if ok else "HTTP %s %s: start it again in Plexamp" % (code, body))
    print("\n  Answers saved in plex_dump/ (tokens removed; they hold names from your library).")
    summary()
    return 0


def cmd_tls(_args):
    """Who signs the certificates the firmware has to verify."""
    import socket
    import ssl
    state = load_state()
    hosts = ["plex.tv"]
    if state.get("token"):
        code, body, _ = fetch(PLEX_TV + "/api/v2/resources?includeHttps=1", headers(state))
        for dev in (as_json(body) or []) if code == 200 else []:
            if "server" not in (dev.get("provides") or ""):
                continue
            for conn in dev.get("connections") or []:
                uri = conn.get("uri") or ""
                if uri.startswith("https://") and conn.get("local"):
                    hosts.append(uri[8:].split(":")[0] + ":" + str(conn.get("port") or 32400))
    for target in hosts:
        host, _, port = target.partition(":")
        try:
            ctx = ssl.create_default_context()
            with socket.create_connection((host, int(port or 443)), timeout=8) as raw:
                with ctx.wrap_socket(raw, server_hostname=host) as tls:
                    issuer = dict(x[0] for x in tls.getpeercert().get("issuer", ()))
            shown = host if host == "plex.tv" else "<server>.plex.direct"
            print("  %-22s issued by: %s / %s" % (shown, issuer.get("organizationName"), issuer.get("commonName")))
        except Exception as err:  # noqa: BLE001 - a diagnostic reports whatever went wrong
            print("  %-22s could not be checked: %s" % (host.split(".")[0] + "...", type(err).__name__))
    print("\nThe firmware verifies against DigiCert Global Root G2, Let's Encrypt (ISRG) and Google Trust Services roots.")
    return 0


def cmd_add_player(args):
    state = load_state()
    state.setdefault("manual_players", [])
    addr = args.address if ":" in args.address else "%s:%d" % (args.address, PLAYER_PORT)
    if addr not in state["manual_players"]:
        state["manual_players"].append(addr)
    save_state(state)
    print("Will also try a player at %s." % addr)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    lg = sub.add_parser("login", help="sign in with a link code")
    lg.add_argument("--wait", type=int, default=240)
    lg.add_argument("--resume", action="store_true", help="keep waiting for the code already shown")
    lg.set_defaults(fn=cmd_login)
    sub.add_parser("logout", help="forget the token").set_defaults(fn=cmd_logout)
    sub.add_parser("report", help="read-only survey").set_defaults(fn=cmd_report)
    ct = sub.add_parser("control", help="pause/play and next/previous on a player")
    ct.add_argument("--player", help="part of the player's name, when there are several")
    ct.set_defaults(fn=cmd_control)
    pr = sub.add_parser("probe", help="volume, seek, shuffle, repeat and the queue, each put back")
    pr.add_argument("--player")
    pr.set_defaults(fn=cmd_probe)
    dp = sub.add_parser("dump", help="raw answers with secrets removed, into ./plex_dump")
    dp.add_argument("--player")
    dp.set_defaults(fn=cmd_dump)
    lb = sub.add_parser("library", help="browse the library, jump within the queue, and (with --start) start an album")
    lb.add_argument("--player", help="player name, when there is more than one")
    lb.add_argument("--start", action="store_true", help="also play a few seconds of an album, then put your music back")
    lb.set_defaults(fn=cmd_library)
    sub.add_parser("tls", help="who signs the certificates the firmware must verify").set_defaults(fn=cmd_tls)
    ad = sub.add_parser("add-player", help="a player's IP address, when discovery does not find it")
    ad.add_argument("address")
    ad.set_defaults(fn=cmd_add_player)
    args = ap.parse_args()
    sys.exit(args.fn(args))


if __name__ == "__main__":
    main()

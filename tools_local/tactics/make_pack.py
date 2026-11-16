#!/usr/bin/env python3
"""Turns Lichess puzzle CSVs into the pack TACTICS reads from the SD card.

    python3 make_pack.py -o puzzles.pack lichess_db_puzzle.csv [more.csv ...]

The input is the format of https://database.lichess.org/#puzzles (CC0):
PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,...
A .csv.zst works too when the `zstandard` module is installed. Copy the result
to the card as /tactics/puzzles.pack.

The pack is a 32-byte header and one 72-byte record a puzzle, sorted by rating
so the device finds a rating band with a binary search:

    header  "TACT", u16 version (1), u16 record size (72), u32 count, name[20]
    record  id[5], flags (bit 0 white to move, bits 1-4 castling KQkq),
            en-passant file or 255, move count, u16 rating, u32 themes,
            2 spare, 32 bytes of board (a nibble a square from a1, the piece
            type with 8 added for black), 12 moves as u16 (from, to << 6,
            promotion type << 12)
"""

import argparse
import csv
import io
import random
import struct
import sys

# Bit order of the themes word. TacticsCore.cpp holds the same list.
THEMES = [
    "mate", "mateIn1", "mateIn2", "mateIn3", "fork", "pin", "skewer", "discoveredAttack",
    "doubleCheck", "sacrifice", "deflection", "attraction", "hangingPiece", "trappedPiece",
    "backRankMate", "smotheredMate", "promotion", "endgame", "middlegame", "opening",
    "quietMove", "defensiveMove", "zugzwang", "intermezzo", "clearance", "interference",
    "xRayAttack", "capturingDefender", "kingsideAttack", "queensideAttack", "exposedKing",
    "advancedPawn",
]
PIECES = {"p": 1, "n": 2, "b": 3, "r": 4, "q": 5, "k": 6}
MAX_MOVES = 12
RECORD = 72


def square(text):
    return (ord(text[0]) - 97) + (int(text[1]) - 1) * 8


def encode(row):
    pid, fen, moves = row["PuzzleId"], row["FEN"], row["Moves"].split()
    if len(pid) != 5 or not 2 <= len(moves) <= MAX_MOVES or len(moves) % 2:
        return None
    board, side, castling, ep = fen.split()[:4]
    squares = [0] * 64
    rank, file = 7, 0
    for c in board:
        if c == "/":
            rank, file = rank - 1, 0
        elif c.isdigit():
            file += int(c)
        else:
            squares[rank * 8 + file] = PIECES[c.lower()] | (8 if c.islower() else 0)
            file += 1
    flags = (1 if side == "w" else 0) | sum(b << (i + 1) for i, b in enumerate(k in castling for k in "KQkq"))
    themes = sum(1 << THEMES.index(t) for t in row["Themes"].split() if t in THEMES)
    packed = bytes(squares[i] | (squares[i + 1] << 4) for i in range(0, 64, 2))
    words = []
    for m in moves:
        promo = PIECES[m[4]] if len(m) > 4 else 0
        words.append(square(m[0:2]) | (square(m[2:4]) << 6) | (promo << 12))
    words += [0] * (MAX_MOVES - len(words))
    rating = int(row["Rating"])
    record = pid.encode("ascii") + struct.pack(
        "<BBBHIH", flags, 255 if ep == "-" else ord(ep[0]) - 97, len(moves), rating, themes, 0
    ) + packed + struct.pack("<%dH" % MAX_MOVES, *words)
    assert len(record) == RECORD
    return rating, record


def rows(path):
    if path.endswith(".zst"):
        import zstandard

        stream = io.TextIOWrapper(zstandard.ZstdDecompressor().stream_reader(open(path, "rb")), encoding="utf-8")
    else:
        stream = open(path, encoding="utf-8", errors="replace")
    yield from csv.DictReader(stream)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+")
    ap.add_argument("-o", "--out", default="puzzles.pack")
    ap.add_argument("--name", default="LICHESS", help="shown on the device, 19 characters")
    ap.add_argument("--popularity", type=int, default=75, help="leave out puzzles players liked less than this")
    ap.add_argument("--plays", type=int, default=50, help="and those played fewer times than this")
    ap.add_argument("--per-band", type=int, default=0, help="at most this many in each 100 points of rating")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    kept = {}
    for path in args.csv:
        for row in rows(path):
            try:
                if int(row["Popularity"]) < args.popularity or int(row["NbPlays"]) < args.plays:
                    continue
                item = encode(row)
            except (KeyError, ValueError, IndexError):
                continue
            if item:
                kept.setdefault(row["PuzzleId"], item)
    items = list(kept.values())
    if args.per_band:
        rng = random.Random(args.seed)
        bands = {}
        for item in items:
            bands.setdefault(item[0] // 100, []).append(item)
        items = []
        for band in sorted(bands):
            pool = bands[band]
            rng.shuffle(pool)
            items += pool[: args.per_band]
    items.sort(key=lambda item: (item[0], item[1]))
    with open(args.out, "wb") as out:
        out.write(b"TACT" + struct.pack("<HHI", 1, RECORD, len(items)) + args.name.encode("ascii")[:19].ljust(20, b"\0"))
        for _, record in items:
            out.write(record)
    print("%d puzzles, %d bytes -> %s" % (len(items), 32 + RECORD * len(items), args.out), file=sys.stderr)
    low = items[0][0] if items else 0
    high = items[-1][0] if items else 0
    print("ratings %d to %d" % (low, high), file=sys.stderr)


if __name__ == "__main__":
    main()

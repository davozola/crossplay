#!/usr/bin/env bash
# Draws every teaching figure into one SVG to look at (and a PNG beside it,
# when cairosvg is installed).
#   host-tests/fmguide/figures.sh [out.svg]
set -euo pipefail
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/fmguide-figures"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/fmguide
OUT="${1:-$BUILD_DIR/figures.svg}"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC figures.cpp $SRC/SynthFigures.cpp -o "$BUILD_DIR/figures"
"$BUILD_DIR/figures" "$OUT"
if python3 -c 'import cairosvg' 2>/dev/null; then
  python3 -c "import cairosvg,sys; cairosvg.svg2png(url=sys.argv[1], write_to=sys.argv[1][:-4]+'.png', scale=1)" "$OUT"
  echo "${OUT%.svg}.png"
fi

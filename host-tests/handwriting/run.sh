#!/bin/sh
# Builds and runs the HANDWRITING tests. InkCore is freestanding C++17, so the
# page model, undo, the rasteriser and the BMP export run on a
# laptop.
#
#   host-tests/handwriting/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-handwriting-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/handwriting
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_ink.cpp $SRC/InkCore.cpp -o "$BUILD_DIR/test_ink"
"$BUILD_DIR/test_ink"

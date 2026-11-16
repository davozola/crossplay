#!/bin/bash
# Builds and runs the PINTA tests. PaintCore is freestanding C++17, so
# the brush and the stamp are checked on a laptop.
#
#   host-tests/paint/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-paint-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/paint
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_paint.cpp $SRC/PaintCore.cpp -o "$BUILD_DIR/test_paint"
"$BUILD_DIR/test_paint"

#!/bin/sh
# Builds and runs the CHESS CLOCK tests. ClockCore is freestanding C++17 with
# the time handed in, so a whole game of presses, pauses and flags runs on a
# laptop.
#
#   host-tests/chessclock/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-chessclock-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/chessclock
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_clock.cpp $SRC/ClockCore.cpp -o "$BUILD_DIR/test_clock"
"$BUILD_DIR/test_clock"

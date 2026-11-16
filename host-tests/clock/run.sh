#!/bin/bash
# Builds and runs the RELOJ tests. ClockCore is freestanding C++17, so
# the words and the numerals are checked on a laptop.
#
#   host-tests/clock/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-clock-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/clock
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_clock.cpp $SRC/ClockCore.cpp -o "$BUILD_DIR/test_clock"
"$BUILD_DIR/test_clock"

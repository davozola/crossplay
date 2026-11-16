#!/bin/sh
# Builds and runs the NAVAL WEATHER tests. NavalCore is freestanding C++17, so the
# Open-Meteo readers run on a laptop against the fixtures beside this script.
#
#   host-tests/naval/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-naval-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/naval
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_naval.cpp $SRC/NavalCore.cpp -o "$BUILD_DIR/test_naval"
"$BUILD_DIR/test_naval"

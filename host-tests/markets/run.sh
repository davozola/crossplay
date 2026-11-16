#!/bin/sh
# Builds and runs the MARKETS tests. MarketsCore is freestanding C++17, so the
# Yahoo chart reader run on a laptop against the fixtures beside this script.
#
#   host-tests/markets/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-markets-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/markets
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_markets.cpp $SRC/MarketsCore.cpp -o "$BUILD_DIR/test_markets"
"$BUILD_DIR/test_markets"

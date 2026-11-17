#!/bin/bash
# Builds and runs EFEMERIDES' tests: the feed in Wikipedia's shape read a few
# bytes at a time, and the sorting of what comes out of it.
#
#   host-tests/history/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-history-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/history
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_history.cpp $SRC/HistoryCore.cpp ../../src/apps_local/discover/DiscoverCore.cpp -o "$BUILD_DIR/test_history"
"$BUILD_DIR/test_history"

#!/bin/bash
# Builds and runs the MANCALA tests. MancalaCore is freestanding C++17, so
# whole games of Kalah are played on a laptop.
#
#   host-tests/mancala/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-mancala-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/mancala
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_mancala.cpp $SRC/MancalaCore.cpp -o "$BUILD_DIR/test_mancala"
"$BUILD_DIR/test_mancala"

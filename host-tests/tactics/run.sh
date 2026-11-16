#!/bin/bash
# Builds and runs the TACTICS tests. TacticsCore and ChessCore are freestanding
# C++17, so every puzzle of the pack that ships is solved on a laptop.
#
#   host-tests/tactics/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-tactics-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/tactics
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_tactics.cpp $SRC/TacticsCore.cpp ../../src/apps_local/chess/ChessCore.cpp -o "$BUILD_DIR/test_tactics"
"$BUILD_DIR/test_tactics" ../../tools_local/tactics/puzzles.pack

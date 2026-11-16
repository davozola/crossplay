#!/bin/bash
# Builds and runs the MISTERIOS tests. MysteryCore is freestanding C++17, so
# the cases are checked for solvability and for fitting their screens on a laptop.
#
#   host-tests/mystery/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-mystery-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/mystery
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_mystery.cpp $SRC/MysteryCore.cpp $SRC/MysteryCases.cpp -o "$BUILD_DIR/test_mystery"
"$BUILD_DIR/test_mystery"

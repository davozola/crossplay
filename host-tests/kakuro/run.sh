#!/bin/sh
# Builds and runs the KAKURO tests. KakuroCore is freestanding C++17, so every puzzle
# in the bank is re-proved here (one solution, by exhaustive count) on a
# laptop.
#
#   host-tests/kakuro/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-kakuro-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/kakuro
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_kakuro.cpp $SRC/KakuroCore.cpp -o "$BUILD_DIR/test_kakuro"
"$BUILD_DIR/test_kakuro"

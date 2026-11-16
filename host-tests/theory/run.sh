#!/bin/sh
# Builds and runs the MUSIC THEORY LAB tests. TheoryCore is freestanding C++17, so the
# scales, chords and rhythms are spelt and checked on a laptop.
#
#   host-tests/theory/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-theory-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/theory
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_theory.cpp $SRC/TheoryCore.cpp -o "$BUILD_DIR/test_theory"
"$BUILD_DIR/test_theory"

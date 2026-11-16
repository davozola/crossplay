#!/bin/sh
# Builds and runs the FM SYNTH LAB tests. FmCore is freestanding C++17, so the
# the algorithm table and the spectrum arithmetic run on a laptop.
#
#   host-tests/fmlab/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-fmlab-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/fmlab
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_fm.cpp $SRC/FmCore.cpp -o "$BUILD_DIR/test_fm"
"$BUILD_DIR/test_fm"

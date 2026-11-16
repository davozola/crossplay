#!/bin/sh
# Builds and runs the INNER CLOCK tests. InnerCore is freestanding C++17 with
# the time handed in, so rounds, records and the two-decimal formatting run on a
# laptop.
#
#   host-tests/innerclock/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-innerclock-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/innerclock
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_inner.cpp $SRC/InnerCore.cpp -o "$BUILD_DIR/test_inner"
"$BUILD_DIR/test_inner"

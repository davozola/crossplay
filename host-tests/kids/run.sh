#!/bin/sh
# Builds and runs the ILYA tests. KidsCore is freestanding C++17, so the
# rounds are dealt and checked on a laptop.
#
#   host-tests/kids/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-kids-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/kids
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_kids.cpp $SRC/KidsCore.cpp -o "$BUILD_DIR/test_kids"
"$BUILD_DIR/test_kids"

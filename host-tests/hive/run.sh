#!/bin/sh
# Builds and runs the HIVE tests. HiveCore and HiveEngine are freestanding
# C++17, so the rules and the opponent run on a laptop.
#
#   host-tests/hive/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-hive-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/hive
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_hive.cpp $SRC/HiveCore.cpp $SRC/HiveEngine.cpp -o "$BUILD_DIR/test_hive"
"$BUILD_DIR/test_hive"

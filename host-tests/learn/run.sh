#!/bin/bash
# Builds and runs APRENDE's tests: every lesson read and checked, and the
# day-by-day bookkeeping.
#
#   host-tests/learn/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-learn-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/learn
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_learn.cpp $SRC/LearnCore.cpp -o "$BUILD_DIR/test_learn"
"$BUILD_DIR/test_learn"

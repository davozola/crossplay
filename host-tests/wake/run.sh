#!/bin/bash
# Builds and runs the wake-plan tests. WakePlan is freestanding C++17, so
# the timer arithmetic is checked on a laptop.
#
#   host-tests/wake/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-wake-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/wake
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_wake.cpp $SRC/WakePlan.cpp -o "$BUILD_DIR/test_wake"
"$BUILD_DIR/test_wake"

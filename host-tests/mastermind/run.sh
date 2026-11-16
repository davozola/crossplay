#!/bin/sh
# Builds and runs the MASTERMIND tests. MastermindCore is freestanding C++17, so
# the scoring (repeats included) and whole games are checked on a
# laptop.
#
#   host-tests/mastermind/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-mastermind-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/mastermind
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_mastermind.cpp $SRC/MastermindCore.cpp -o "$BUILD_DIR/test_mastermind"
"$BUILD_DIR/test_mastermind"

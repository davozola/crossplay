#!/bin/sh
# Builds and runs the Boxes tests: the rules and the opponent are freestanding
# C++17, so whole games are played on a laptop.
#
#   host-tests/boxes/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-boxes-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/boxes
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_boxes.cpp -o "$BUILD_DIR/test_boxes"
"$BUILD_DIR/test_boxes"

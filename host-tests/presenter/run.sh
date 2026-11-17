#!/bin/sh
# Builds and runs the Presenter tests: the clock and the key reports are freestanding
# C++17, so a whole talk is timed on a laptop.
#
#   host-tests/presenter/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-presenter-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/presenter
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_presenter.cpp -o "$BUILD_DIR/test_presenter"
"$BUILD_DIR/test_presenter"

#!/bin/sh
# Builds and runs the Liner Notes tests: the pack's index, a record laid out as
# lines, and the reader's marks are freestanding C++17.
#
#   host-tests/linernotes/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-linernotes-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/linernotes
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_linernotes.cpp $SRC/LinerNotesCore.cpp -o "$BUILD_DIR/test_linernotes"
"$BUILD_DIR/test_linernotes"

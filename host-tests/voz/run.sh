#!/bin/bash
# Builds and runs the VOZ tests. VozCore is freestanding C++17.
# Checked on a laptop.
#
#   host-tests/voz/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-voz-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/voz
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_voz.cpp -o "$BUILD_DIR/test_voz"
"$BUILD_DIR/test_voz"

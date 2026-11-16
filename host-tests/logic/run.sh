#!/bin/sh
# Builds and runs the LOGIC tests: every shipped Slitherlink, Shikaku, Akari and
# Hashi board is solved again with the generator's solvers and played through
# the device's own state.
#
#   host-tests/logic/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-logic-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/logic
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_logic.cpp $SRC/LogicCore.cpp -o "$BUILD_DIR/test_logic"
"$BUILD_DIR/test_logic"

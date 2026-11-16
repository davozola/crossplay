#!/bin/sh
# Builds and runs the RPN 12C tests. RpnCore is freestanding C++17, so the
# keys are pressed and answers checked on a laptop.
#
#   host-tests/rpn/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-rpn-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/rpn
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_rpn.cpp $SRC/RpnCore.cpp -o "$BUILD_DIR/test_rpn"
"$BUILD_DIR/test_rpn"

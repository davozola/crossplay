#!/bin/bash
# Builds and runs the CASA tests. CasaCore is freestanding C++17, so
# the log and its arithmetic are checked on a laptop.
#
#   host-tests/casa/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-casa-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/casa
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_casa.cpp $SRC/CasaCore.cpp -o "$BUILD_DIR/test_casa"
"$BUILD_DIR/test_casa"

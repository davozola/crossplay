#!/bin/sh
# Builds and runs the FONDOS tests against the pack that ships.
#
#   host-tests/fondos/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-fondos-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/fondos
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_fondos.cpp -o "$BUILD_DIR/test_fondos"
"$BUILD_DIR/test_fondos" $SRC/fondos.pack

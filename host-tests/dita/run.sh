#!/usr/bin/env bash
# DITA's sayings and its day arithmetic, checked on a laptop.
#   host-tests/dita/run.sh
set -euo pipefail
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-dita-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/dita
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 -I$SRC test_dita.cpp -o "$BUILD_DIR/test_dita"
"$BUILD_DIR/test_dita"

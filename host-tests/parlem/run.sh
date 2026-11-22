#!/usr/bin/env bash
# PARLEM's phrases, checked on a laptop.
#   host-tests/parlem/run.sh
set -euo pipefail
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-parlem-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/parlem
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 -I$SRC test_parlem.cpp -o "$BUILD_DIR/test_parlem"
"$BUILD_DIR/test_parlem"

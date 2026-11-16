#!/bin/bash
# Builds and runs DISCOVER's tests: the search it asks for and an answer in
# GitHub's shape.
#
#   host-tests/discover/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-discover-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/discover
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_discover.cpp $SRC/DiscoverCore.cpp -o "$BUILD_DIR/test_discover"
"$BUILD_DIR/test_discover"

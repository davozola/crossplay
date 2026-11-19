#!/bin/bash
# Builds and runs the tests of WIKIPEDIA's network side: an article answer in
# the API's shape read a few bytes at a time into the reader's XHTML, the
# search answers, and the matching of saved titles.
#
#   host-tests/wikionline/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-wikionline-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/wikipedia
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_wikionline.cpp $SRC/WikiOnlineCore.cpp -o "$BUILD_DIR/test_wikionline"
"$BUILD_DIR/test_wikionline"

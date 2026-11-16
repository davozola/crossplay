#!/bin/bash
# Builds and runs THE BATCH's tests: the feed as its keeper publishes it, a
# front page and an issue in the extractor's shape.
#
#   host-tests/batch/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-batch-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/batch
HN=../../src/apps_local/hackernews
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC -I../../lib/Utf8 \
  test_batch.cpp $SRC/BatchCore.cpp $HN/HackerNewsCore.cpp ../../lib/Utf8/Utf8.cpp -o "$BUILD_DIR/test_batch"
"$BUILD_DIR/test_batch"

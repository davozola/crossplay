#!/bin/sh
# Builds and runs the TRIPLES tests. TriplesCore is freestanding C++17, so the
# whole games are played on a laptop.
#
#   host-tests/triples/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-triples-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/triples
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_triples.cpp $SRC/TriplesCore.cpp -o "$BUILD_DIR/test_triples"
"$BUILD_DIR/test_triples"

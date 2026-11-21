#!/bin/bash
# Builds and runs the DECK tests. DeckCore is freestanding C++17, so the
# templates are checked on a laptop.
#
#   host-tests/deck/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-deck-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/deck
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_deck.cpp -o "$BUILD_DIR/test_deck"
"$BUILD_DIR/test_deck"

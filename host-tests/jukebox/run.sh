#!/bin/sh
# Builds and runs the JUKEBOX tests. JukeboxCore is freestanding C++17, so its
# reading of Plex and Plexamp runs on a laptop against the answers in
# fixtures/, which were captured from a real Plexamp (4.12.4 on Android) and a
# real Plex Media Server, then had their tokens, identifiers and public
# addresses replaced.
#
#   host-tests/jukebox/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-jukebox-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/jukebox
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_jukebox.cpp $SRC/JukeboxCore.cpp -o "$BUILD_DIR/test_jukebox"
"$BUILD_DIR/test_jukebox"

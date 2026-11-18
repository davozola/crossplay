#!/bin/bash
# Builds and runs the FM-1 GUIAS tests. The guides are data and GuideCore is
# freestanding C++17, so every block of every guide is checked on a laptop:
# inside Latin-1, within the lengths its place on the page allows, and every
# drawn grid and flow well formed.
#
#   host-tests/fmguide/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-fmguide-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/fmguide
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_fmguide.cpp $SRC/GuideCore.cpp $SRC/Guides.cpp $SRC/GuideGeneral.cpp $SRC/GuideSloop.cpp \
  $SRC/GuideFelucca.cpp $SRC/GuideBaud.cpp $SRC/GuideX0x.cpp $SRC/GuideJiant.cpp -o "$BUILD_DIR/test_fmguide"
"$BUILD_DIR/test_fmguide"

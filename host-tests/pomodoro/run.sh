#!/bin/sh
# Builds and runs the POMODORO timer tests. PomodoroCore is freestanding C++17,
# so the whole cycle is checked on a laptop with the clock handed in.
#
#   host-tests/pomodoro/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-pomodoro-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/pomodoro
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_pomodoro.cpp $SRC/PomodoroCore.cpp -o "$BUILD_DIR/test_pomodoro"
"$BUILD_DIR/test_pomodoro"

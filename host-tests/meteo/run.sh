#!/bin/sh
# Builds and runs the WEATHER tests. WeatherCore is freestanding C++17, so the
# Open-Meteo readers run on a laptop against the fixtures beside this script.
#
#   host-tests/meteo/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-meteo-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/meteo
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_weather.cpp $SRC/WeatherCore.cpp -o "$BUILD_DIR/test_weather"
"$BUILD_DIR/test_weather"

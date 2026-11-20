#!/bin/bash
# Host tests for NEWS: the feed scanner, dates, the section cache.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-news-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror \
  -I../../lib/Utf8 \
  ../../lib/Utf8/Utf8.cpp \
  ../../src/apps_local/news/NewsCore.cpp \
  test_news.cpp -o "$BUILD_DIR/test_news"
"$BUILD_DIR/test_news"

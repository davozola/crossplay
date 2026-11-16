// PINTA host tests: the brush, the rubber and the stamp on a small sheet.

#include <cstdio>
#include <vector>

#include "PaintCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
int count(const paint::Sheet& s) {
  int n = 0;
  for (int y = 0; y < s.height(); ++y) {
    for (int x = 0; x < s.width(); ++x) n += s.at(x, y) ? 1 : 0;
  }
  return n;
}
}  // namespace

int main() {
  using paint::Sheet;
  std::vector<uint8_t> bits(13 * 80, 0xAA);
  Sheet s(bits.data(), 100, 80);
  check(s.rowBytes() == 13, "row bytes");
  s.clear();
  check(s.empty() && count(s) == 0, "clear");

  s.dot(50, 40, 10, true);
  check(s.at(50, 40) && s.at(60, 40) && s.at(50, 30) && !s.at(61, 40) && !s.at(58, 48), "a disc");
  const int disc = count(s);
  check(disc > 290 && disc < 340, "about pi r squared");
  s.dot(50, 40, 4, false);
  check(!s.at(50, 40) && s.at(57, 40) && count(s) < disc, "the rubber is the brush in white");

  s.clear();
  s.dot(0, 0, 10, true);
  s.dot(99, 79, 10, true);
  s.dot(-50, -50, 10, true);
  check(s.at(0, 0) && s.at(99, 79) && !s.at(50, 40), "clipped at the edges");

  s.clear();
  s.line(10, 10, 90, 70, 5, true);
  bool joined = true;
  for (int i = 0; i <= 80; ++i) joined = joined && s.at(10 + i, 10 + i * 60 / 80);
  check(joined, "a line has no gaps");
  check(!s.at(90, 10) && !s.at(10, 70), "and stays a line");
  s.line(30, 30, 30, 30, 5, true);
  check(s.at(30, 30), "a line of no length is a dot");

  // A 4x2 picture: ink is the clear bits. 1010.... / 0101....
  s.clear();
  const uint8_t pic[2] = {0xAF, 0x5F};
  s.stamp(pic, 4, 2, 50, 40, 3);
  check(count(s) == 4 * 9, "a stamp lays down only its ink, scaled");
  check(s.at(47, 37) && !s.at(44, 37) && s.at(44, 40) && !s.at(47, 40), "where the picture has it");
  s.dot(48, 38, 2, true);
  const int before = count(s);
  s.stamp(pic, 4, 2, 50, 40, 3);
  check(count(s) == before, "stamping over paint rubs nothing out");
  s.stamp(pic, 4, 2, -500, 40, 3);
  s.stamp(nullptr, 4, 2, 50, 40, 3);
  check(count(s) == before, "off the sheet or no picture: nothing");

  if (failures == 0) std::printf("paint: all tests passed\n");
  return failures == 0 ? 0 : 1;
}

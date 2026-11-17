// FONDOS host tests: the pack that ships is what the app expects, picture by
// picture, and the two settings survive the card.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "FondosCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
}  // namespace

int main(int argc, char** argv) {
  using namespace fondos;
  std::ifstream in(argc > 1 ? argv[1] : "fondos.pack", std::ios::binary);
  const std::vector<uint8_t> pack((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const Header h = parseHeader(pack.data(), pack.size());
  check(h.ok(), "the pack that ships has a header");
  check(h.count >= 40 && h.count <= kMaxPictures, "and a sensible number of pictures");
  check(h.packBytes() == pack.size(), "and is exactly as long as it says");
  // 480 by 800 at one bit, rows padded to four bytes, behind a 62-byte header.
  check(h.pictureBytes == 62u + 60u * 800u, "every picture is a whole panel");
  for (int i = 0; i < h.count; ++i) {
    const uint8_t* bmp = pack.data() + h.pictureAt(i);
    const int32_t width = bmp[18] | (bmp[19] << 8);
    const int32_t height = bmp[22] | (bmp[23] << 8);
    check(bmp[0] == 'B' && bmp[1] == 'M' && width == 480 && height == 800 && bmp[28] == 1, "each is a one-bit 480x800 BMP");
    const uint8_t* text = pack.data() + h.textAt(i);
    check(text[0] != 0 && text[kTitleBytes - 1] == 0 && text[kTextBytes - 1] == 0, "each has a title, and its text ends");
    // Not blank, not solid: somewhere between a tenth and nine tenths ink.
    long ink = 0;
    for (uint32_t b = 62; b < h.pictureBytes; ++b) ink += 8 - __builtin_popcount(bmp[b]);
    const long all = 480L * 800L;
    check(ink > all / 20 && ink < all * 19 / 20, "each is a picture, neither white nor black");
  }

  check(!parseHeader(pack.data(), 9).ok(), "nine bytes are not a pack");
  check(!parseHeader(pack.data(), pack.size() - 1).ok(), "a pack cut short is refused");
  const uint8_t wrong[10] = {'F', 'N', 'D', '2', 1, 0, 62, 0, 0, 0};
  check(!parseHeader(wrong, sizeof(wrong)).ok(), "another magic is not a pack");

  uint8_t bytes[kStateBytes];
  packState(Mode::One, 300 % h.count, bytes);
  Mode mode = Mode::Off;
  int chosen = -1;
  check(unpackState(bytes, kStateBytes, h.count, mode, chosen) && mode == Mode::One && chosen == 300 % h.count,
        "the choice survives the card");
  packState(Mode::Every, 0, bytes);
  check(unpackState(bytes, kStateBytes, h.count, mode, chosen) && mode == Mode::Every, "and so does the rotation");
  packState(Mode::One, 5, bytes);
  check(!unpackState(bytes, kStateBytes, 3, mode, chosen) && mode == Mode::Off, "a choice past a smaller pack is forgotten");
  check(step(0, -1, 5) == 4 && step(4, 1, 5) == 0 && step(2, 1, 5) == 3, "stepping goes round");

  // The order of a visit: everything once, different from visit to visit.
  {
    uint8_t a[kMaxPictures];
    uint8_t b[kMaxPictures];
    shuffled(a, h.count, 12345u);
    shuffled(b, h.count, 99991u);
    int seen[kMaxPictures] = {};
    int same = 0;
    int inPlace = 0;
    for (int i = 0; i < h.count; ++i) {
      ++seen[a[i]];
      same += a[i] == b[i];
      inPlace += a[i] == i;
    }
    bool once = true;
    for (int i = 0; i < h.count; ++i) once = once && seen[i] == 1;
    check(once, "a visit shows every picture exactly once");
    check(same < h.count / 3 && inPlace < h.count / 3, "two visits are in different orders, neither the pack's");
    shuffled(a, h.count, 777u, 40 % h.count);
    check(a[0] == 40 % h.count, "the picture in use leads");
    int again[kMaxPictures] = {};
    for (int i = 0; i < h.count; ++i) ++again[a[i]];
    once = true;
    for (int i = 0; i < h.count; ++i) once = once && again[i] == 1;
    check(once, "and nothing is lost by putting it first");
  }

  if (failures == 0) std::printf("fondos: all tests passed (%d pictures)\n", h.count);
  return failures == 0 ? 0 : 1;
}

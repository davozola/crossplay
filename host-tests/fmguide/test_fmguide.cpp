#include <cstdio>
#include <cstring>

#include "GuideCore.h"

static int failures = 0;
static int checks = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    ++checks;                                                    \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

using namespace fmguide;

int main() {
  // Every guide, every chapter, every block: nothing the reader cannot set.
  int blocks = 0;
  int bytes = 0;
  for (int g = 0; g < kGuides; ++g) {
    const Guide& guide = guideAt(g);
    CHECK(checkLabel(guide.name, 14) == nullptr);
    CHECK(checkProse(guide.tagline, 70) == nullptr);
    CHECK(checkLabel(guide.version, 10) == nullptr);
    // The chapter list has a row a chapter, and one more to resume.
    CHECK(guide.count >= 3 && guide.count <= 11);
    for (int c = 0; c < guide.count; ++c) {
      const Chapter& chapter = guide.chapters[c];
      CHECK(checkProse(chapter.title, 28) == nullptr);
      CHECK(chapter.count >= 4 && chapter.count <= 250);
      // A heading has something under it.
      CHECK(chapter.blocks[chapter.count - 1].kind != Kind::Head);
      for (int b = 0; b < chapter.count; ++b) {
        const Block& block = chapter.blocks[b];
        const char* why = checkBlock(block);
        ++checks;
        if (why) {
          ++failures;
          std::printf("FAIL %s / %s / block %d: %s\n  %.60s\n", guide.name, chapter.title, b, why,
                      block.a ? block.a : "");
        }
        if (block.kind == Kind::Head && b + 1 < chapter.count) CHECK(chapter.blocks[b + 1].kind != Kind::Head);
        ++blocks;
        bytes +=
            static_cast<int>(block.a ? std::strlen(block.a) : 0) + static_cast<int>(block.b ? std::strlen(block.b) : 0);
      }
    }
  }
  CHECK(blocks > 400);

  {  // the checks themselves
    CHECK(checkProse("Instalaci\xC3\xB3n", 20) == nullptr);
    CHECK(checkProse("\xE2\x86\x92", 20) != nullptr);  // an arrow is outside Latin-1
    CHECK(checkProse("", 20) != nullptr && checkProse("abcdef", 5) != nullptr);
    CHECK(checkLabel("OCT+", 6) == nullptr && checkLabel("oct", 6) != nullptr);
    CHECK(checkGrid("BD:x---x---x---x---") == nullptr);
    CHECK(checkGrid("BD:x---x---x---x--") != nullptr && checkGrid("x---x---x---x---") != nullptr);
    CHECK(checkGrid("BD:x---x---x---x--q") != nullptr);
    CHECK(checkFlow("A>B>C|D>E") == nullptr && checkFlow("A>>B") != nullptr && checkFlow("a>b") != nullptr);
    CHECK(checkFlow("A>B>C>D>E>F") != nullptr);
    CHECK(rowsOf("A|B|C") == 3 && rowsOf("") == 0);
    char row[8];
    CHECK(rowAt("AB|CD|EF", 1, row, sizeof(row)) && std::strcmp(row, "CD") == 0);
    CHECK(!rowAt("AB|CD", 2, row, sizeof(row)));
    CHECK(!rowAt("ABCDEFGHIJ|K", 0, row, sizeof(row)));
  }
  {  // keys and controls
    CHECK(white(1) == 1u && white(16) == 0x8000u && black(1) == 0x10000u && black(11) == (1u << 26));
    CHECK((kAllWhite & kAllBlack) == 0u);
    for (int i = 1; i < kBlackKeys; ++i) CHECK(kBlackAfter[i] > kBlackAfter[i - 1]);
    CHECK(kBlackAfter[kBlackKeys - 1] < kWhiteKeys - 1);
    for (int i = 0; i < kControls; ++i) CHECK(checkLabel(controlLabel(i), 8) == nullptr);
    CHECK(std::strcmp(controlLabel(10), "FX") == 0 && std::strcmp(controlLabel(21), "REC") == 0);
    CHECK((cFx << 11) == cRec && (cKnob1 << 3) == cKnob4);
  }
  {  // where the reader was
    Place places[kGuides];
    places[1] = {3, 2};
    places[4] = {9, 0};
    uint8_t bytesOut[kSaveBytes];
    pack(places, 4, bytesOut);
    Place back[kGuides];
    int last = -1;
    CHECK(unpack(bytesOut, kSaveBytes, back, last) && last == 4 && back[1].chapter == 3 && back[1].page == 2 &&
          back[4].chapter == 9);
    bytesOut[2] = 200;  // a chapter the first guide does not have
    CHECK(!unpack(bytesOut, kSaveBytes, back, last));
    CHECK(!unpack(bytesOut, kSaveBytes - 1, back, last));
    pack(places, -1, bytesOut);
    CHECK(unpack(bytesOut, kSaveBytes, back, last) && last == -1);
    // A save written when there were five guides keeps its places, and the
    // guide added since starts at its first page.
    pack(places, 4, bytesOut);
    back[kGuides - 1] = {7, 7};
    const int firstSave = 2 + kGuidesFirstSave * 2;
    CHECK(unpack(bytesOut, firstSave, back, last) && last == 4 && back[1].chapter == 3 && back[4].chapter == 9 &&
          back[kGuides - 1].chapter == 0 && back[kGuides - 1].page == 0);
    CHECK(!unpack(bytesOut, firstSave - 2, back, last));
  }
  if (failures == 0)
    std::printf("fmguide: all tests passed (%d checks, %d blocks, %d bytes of text)\n", checks, blocks, bytes);
  return failures == 0 ? 0 : 1;
}

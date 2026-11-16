#pragma once

// The guides, one a file. Each exports its Guide; Guides.cpp puts them in the
// order the home page shows.

#include "GuideCore.h"

#define FMGUIDE_CHAPTER(title, blocks) {title, blocks, static_cast<int>(sizeof(blocks) / sizeof(blocks[0]))}

namespace fmguide {

extern const Guide kGuideGeneral;
extern const Guide kGuideSloop;
extern const Guide kGuideFelucca;
extern const Guide kGuideBaud;
extern const Guide kGuideX0x;

}  // namespace fmguide

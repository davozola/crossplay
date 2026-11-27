#include "GuideData.h"

namespace fmguide {

const Guide& guideAt(const int index) {
  static const Guide* const kAll[kGuides] = {&kGuideGeneral, &kGuideSloop, &kGuideFelucca, &kGuideBaud,
                                             &kGuideX0x,     &kGuideJiant, &kGuideShrub};
  return *kAll[index >= 0 && index < kGuides ? index : 0];
}

}  // namespace fmguide

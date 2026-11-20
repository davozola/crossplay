#pragma once

// One city with the panel on its side, set in its own type rather than the
// shelf's and in Spanish: an editorial page in a serif and in a grotesque, and
// an instrument panel, each on paper and on black. Drawn straight onto the
// renderer.

#include "WeatherScreens.h"

class GfxRenderer;

namespace meteowide {

constexpr int kFaces = 6;
// The foot of every face is three places to tap: cities, auto, refresh.
constexpr int kFootHeight = 48;
constexpr int kFootSide = 220;

// `face`: 0 editorial on paper, 1 editorial on black, 2 instruments on black,
// 3 instruments on paper, 4 the editorial page in a grotesque on paper, 5 the
// same on black.
void draw(GfxRenderer& renderer, const meteoui::ForecastModel& model, int face);

}  // namespace meteowide

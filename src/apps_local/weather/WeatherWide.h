#pragma once

// One city with the panel on its side, set in its own type rather than the
// shelf's: an editorial page and an instrument panel, each on paper and on
// black. Drawn straight onto the renderer.

#include "WeatherScreens.h"

class GfxRenderer;

namespace weatherwide {

constexpr int kFaces = 4;
// The foot of every face is three places to tap: cities, auto, refresh.
constexpr int kFootHeight = 48;
constexpr int kFootSide = 220;

// `face`: 0 editorial on paper, 1 editorial on black, 2 instruments on black,
// 3 instruments on paper.
void draw(GfxRenderer& renderer, const weatherui::ForecastModel& model, int face);

}  // namespace weatherwide

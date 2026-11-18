#pragma once

// RELOJ's faces. Each draws the whole panel for one moment, in whichever
// orientation the renderer is turned to, straight onto the framebuffer: this
// app keeps its own look and uses none of the Toybox screen furniture.

#include <GfxRenderer.h>

#include "ClockCore.h"

namespace clockfaces {

struct Moment {
  bool valid = false;  // the device's clock has been set
  int hour = 0;
  int minute = 0;
  int weekday = 0;  // 0 Sunday
  int day = 1;
  int month = 1;
  int year = 2026;
  int yearDay = 0;  // 0 on the first of January
};

void draw(GfxRenderer& renderer, int face, const Moment& now);

// One numeral of the clock's own face, for another app that wants the time in
// the same hand: drawn with a round pen inside a box `boxW` wide and 1.6 times
// as tall, whose top-left corner is (x, y).
void drawNumeral(GfxRenderer& renderer, int digit, float x, float y, float boxW, float pen, bool ink = true);

// A strip along the bottom edge saying how the app is driven.
void drawHint(GfxRenderer& renderer, const char* text);

}  // namespace clockfaces

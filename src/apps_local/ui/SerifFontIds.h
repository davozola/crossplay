#pragma once

// FORK CHANGE: Noto Serif as a fork-owned built-in face.
//
// CrossPoint 2.0 moved Noto Serif out of the firmware into an optional SD
// font (upstream #4022). Hoy, Jukebox and Wikipedia are drawn in it, so the
// fork keeps its own copy: the cuts in ui/fonts/notoserif_*.h, generated with
// upstream's lib/EpdFont/scripts/fontconvert.py from the same TTFs, and
// registered by toybox::registerFonts(). The ids are the ones upstream used
// before the move, so nothing that stored one changes meaning.
#define NOTOSERIF_12_FONT_ID (778531645)
#define NOTOSERIF_14_FONT_ID (-928381217)
#define NOTOSERIF_16_FONT_ID (17214534)
#define NOTOSERIF_18_FONT_ID (840051567)

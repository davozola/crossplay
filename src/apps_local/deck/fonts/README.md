# DECK's type

Three cuts of **Instrument Serif** (SIL Open Font License 1.1; the licence is
`site/assets/fonts/OFL-InstrumentSerif.txt`), baked at 1 bit by
`lib/EpdFont/scripts/fontconvert.py` from subsets of the site's own web fonts,
so nothing is taken that the rest of the device does not already carry:

| Header | From | Size | Characters |
| --- | --- | --- | --- |
| `deck_numeral_52.h` | `instrumentserif.woff2` | 52 | 0-9, `:` and `|` |
| `deck_italic_24.h` | `instrumentserif-italic.woff2` | 24 | A-Z a-z 0-9, space, ÁÉÍÓÚÑ áéíóúñ, `|` |
| `deck_italic_16.h` | `instrumentserif-italic.woff2` | 16 | the same |

The converter reads its line metrics from `|`, which is why a font of numerals
carries one. To rebuild: unpack the woff2 to TTF with fontTools, subset it with
`pyftsubset --unicodes=...` to the characters above, and run the converter
with the name and size in the table.

// The teaching figures drawn on a laptop, to be looked at before they ship.
// Writes one SVG page with every figure at the reader's column width, each
// under its name, at the device's pixel scale (480 wide):
//   host-tests/fmguide/figures.sh [out.svg]
#include <cstdio>
#include <cstring>
#include <string>

#include "SynthFigures.h"

namespace {

// The pixel face is about 8 px a capital on the device; close enough to see
// whether a label fits.
constexpr int kCharW = 8;

class SvgCanvas final : public synthfig::Canvas {
 public:
  std::string out;
  void line(int x0, int y0, int x1, int y1, int width) override {
    char b[160];
    std::snprintf(b, sizeof(b), "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke='#000' stroke-width='%d'/>\n", x0, y0, x1,
                  y1, width);
    out += b;
  }
  void fill(int x, int y, int w, int h, bool ink) override {
    char b[160];
    std::snprintf(b, sizeof(b), "<rect x='%d' y='%d' width='%d' height='%d' fill='%s'/>\n", x, y, w, h,
                  ink ? "#000" : "#fff");
    out += b;
  }
  void gray(int x, int y, int w, int h) override {
    char b[160];
    std::snprintf(b, sizeof(b), "<rect x='%d' y='%d' width='%d' height='%d' fill='#888'/>\n", x, y, w, h);
    out += b;
  }
  void frame(int x, int y, int w, int h, int width) override {
    char b[200];
    std::snprintf(b, sizeof(b),
                  "<rect x='%.1f' y='%.1f' width='%d' height='%d' fill='none' stroke='#000' stroke-width='%d'/>\n",
                  x + width / 2.0, y + width / 2.0, w - width, h - width, width);
    out += b;
  }
  void text(int x, int cy, const char* t, synthfig::Align a, bool ink) override {
    const int w = textWidth(t);
    int left = x;
    if (a == synthfig::Align::Centre) left = x - w / 2;
    if (a == synthfig::Align::Right) left = x - w;
    std::string safe;
    for (const char* p = t; *p; ++p) safe += *p == '&' ? std::string("&amp;") : std::string(1, *p);
    char b[400];
    std::snprintf(b, sizeof(b),
                  "<text x='%d' y='%d' font-family='DejaVu Sans Mono' font-weight='bold' font-size='13' "
                  "textLength='%d' lengthAdjust='spacingAndGlyphs' fill='%s'>%s</text>\n",
                  left, cy + 6, w, ink ? "#000" : "#fff", safe.c_str());
    out += b;
  }
  int textWidth(const char* t) override { return static_cast<int>(std::strlen(t)) * kCharW; }
};

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "figures.svg";
  const int width = 480 - 32;  // the reader's column
  SvgCanvas canvas;
  int y = 10;
  for (int f = 0; f < synthfig::kFigures; ++f) {
    canvas.text(16, y + 8, synthfig::name(f), synthfig::Align::Left, true);
    y += 26;
    const int h = synthfig::height(f, width);
    canvas.frame(14, y - 2, width + 4, h + 4, 1);
    synthfig::draw(canvas, f, 16, y, width);
    y += h + 24;
  }
  FILE* file = std::fopen(path, "w");
  if (!file) return 1;
  std::fprintf(file, "<svg xmlns='http://www.w3.org/2000/svg' width='480' height='%d' viewBox='0 0 480 %d'>\n", y, y);
  std::fprintf(file, "<rect width='480' height='%d' fill='#fff'/>\n%s</svg>\n", y, canvas.out.c_str());
  std::fclose(file);
  std::printf("%d figures, %d px tall: %s\n", synthfig::kFigures, y, path);
  return 0;
}

#pragma once

// Prose in the reading serif, wrapped at spaces. Shared by the apps that set
// text other people wrote: a newsletter, a README.

#include <GfxRenderer.h>

#include <string>
#include <vector>

namespace toyboxserif {

// Wraps `text` to `width` in `font`, a line at a time into `out`. At most
// `maxLines` (0 for all of it); the last allowed line ends in dots when more follows.
inline void wrap(const GfxRenderer& r, const int font, const char* text, const int width, const int maxLines,
                 std::vector<std::string>& out) {
  std::string line;
  const char* p = text;
  int made = 0;
  while (*p) {
    while (*p == ' ') ++p;
    const char* end = p;
    while (*end && *end != ' ') ++end;
    if (end == p) break;
    std::string candidate = line;
    if (!candidate.empty()) candidate.push_back(' ');
    candidate.append(p, static_cast<size_t>(end - p));
    if (!line.empty() && r.getTextWidth(font, candidate.c_str()) > width) {
      if (maxLines > 0 && made + 1 == maxLines) {
        // The dots have to fit too: words come off the end until they do.
        while (r.getTextWidth(font, (line + "...").c_str()) > width) {
          const size_t space = line.rfind(' ');
          if (space == std::string::npos) break;
          line.resize(space);
        }
        line.append("...");
        out.push_back(std::move(line));
        return;
      }
      out.push_back(std::move(line));
      ++made;
      line.assign(p, static_cast<size_t>(end - p));
    } else {
      line = std::move(candidate);
    }
    p = end;
  }
  if (!line.empty()) out.push_back(std::move(line));
}

// Draws up to `maxLines` of wrapped text; returns the height used.
inline int draw(const GfxRenderer& r, const int font, const int lineHeight, const int x, const int y, const int width,
                const char* text, const int maxLines, const bool black = true) {
  std::vector<std::string> lines;
  lines.reserve(4);
  wrap(r, font, text, width, maxLines, lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    r.drawText(font, x, y + static_cast<int>(i) * lineHeight, lines[i].c_str(), black);
  }
  return static_cast<int>(lines.size()) * lineHeight;
}

}  // namespace toyboxserif

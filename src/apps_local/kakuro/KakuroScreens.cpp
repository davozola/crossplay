#include "KakuroScreens.h"

#include <cstdio>
#include <cstring>

#include "../ui/ToyboxFormat.h"

namespace kakuroui {

namespace {

namespace kk = kakuro;

constexpr int16_t kBoardSide = 448;
constexpr int16_t kPadKeyH = 56;
constexpr int16_t kHintH = 52;

void toyboxChrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = rightLabel;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

int16_t cellSize(const int side) { return static_cast<int16_t>(kBoardSide / side); }
int16_t boardLeft(const fui::DeviceContext& device, const int side) {
  return static_cast<int16_t>((device.width - cellSize(side) * side) / 2);
}
int16_t boardTop() { return static_cast<int16_t>(toybox::kChromeHeight + toybox::kGutter + toybox::kFrame); }
int16_t boardBottom(const int side) { return static_cast<int16_t>(boardTop() + cellSize(side) * side); }
int16_t padTop(const fui::DeviceContext& device) {
  return static_cast<int16_t>(device.height - 40 - toybox::kGutter - kPadKeyH * 2 - toybox::kGutter / 2);
}

void text(toybox::Screen& screen, const fui::Rect& box, const char* s, const fui::FontId font,
          const toybox::CutMetrics& cut, const fui::Color color, const fui::TextAlign align = fui::TextAlign::Center) {
  fui::TextStyle style;
  style.font = font;
  style.color = color;
  style.align = align;
  screen.target().text(toybox::inkCentred(box, cut), s, style);
}

// A clue block: black, split by a white diagonal, the across sum in the upper
// right and the down sum in the lower left, as every printed Kakuro draws it.
// A clue whose run is full and wrong is drawn inverted, white with black.
void clueCell(toybox::Screen& screen, const fui::Rect& cell, const int across, const int down, const bool acrossWrong,
              const bool downWrong) {
  auto& t = screen.target();
  t.fill(cell, fui::Paint::solid(fui::Color::Black));
  if (across == 0 && down == 0) return;
  const int16_t c = cell.width;
  t.line(fui::Point{static_cast<int16_t>(cell.x + 2), static_cast<int16_t>(cell.y + 2)},
         fui::Point{static_cast<int16_t>(cell.right() - 3), static_cast<int16_t>(cell.bottom() - 3)}, 2,
         fui::Paint::solid(fui::Color::White));
  char n[toybox::kIntTextChars];
  const int16_t box = static_cast<int16_t>(c / 2);
  if (across) {
    std::snprintf(n, sizeof(n), "%d", across);
    const fui::Rect r =
        fui::makeRect(static_cast<int16_t>(cell.right() - box - 1), static_cast<int16_t>(cell.y + 1), box, box);
    if (acrossWrong) t.fill(r, fui::Paint::solid(fui::Color::White));
    text(screen, r, n, toybox::kTileFont, toybox::kTileCut, acrossWrong ? fui::Color::Black : fui::Color::White);
  }
  if (down) {
    std::snprintf(n, sizeof(n), "%d", down);
    const fui::Rect r =
        fui::makeRect(static_cast<int16_t>(cell.x + 1), static_cast<int16_t>(cell.bottom() - box - 1), box, box);
    if (downWrong) t.fill(r, fui::Paint::solid(fui::Color::White));
    text(screen, r, n, toybox::kTileFont, toybox::kTileCut, downWrong ? fui::Color::Black : fui::Color::White);
  }
}

void comboLine(const int sum, const int length, char* out, const int outSize) {
  uint16_t combos[12];
  const int count = kk::combinations(sum, length, combos, 12);
  int at = std::snprintf(out, static_cast<size_t>(outSize), "%d IN %d: ", sum, length);
  if (count > 4) {
    std::snprintf(out + at, static_cast<size_t>(outSize - at), "%d WAYS", count);
    return;
  }
  for (int i = 0; i < count && at < outSize - 12; ++i) {
    if (i) out[at++] = ' ';
    for (int d = 1; d <= 9; ++d)
      if (combos[i] & (1u << d)) out[at++] = static_cast<char>('0' + d);
  }
  out[at < outSize ? at : outSize - 1] = '\0';
}

}  // namespace

const char* sizeName(const kk::Size size) {
  switch (size) {
    case kk::Size::Small:
      return "SMALL";
    case kk::Size::Medium:
      return "MEDIUM";
    case kk::Size::Large:
      return "LARGE";
  }
  return "SMALL";
}

fui::Rect cellRect(const fui::DeviceContext& device, const int side, const int cell) {
  const int16_t c = cellSize(side);
  return fui::makeRect(static_cast<int16_t>(boardLeft(device, side) + (cell % side) * c),
                       static_cast<int16_t>(boardTop() + (cell / side) * c), c, c);
}

int cellAt(const fui::DeviceContext& device, const int side, const int x, const int y) {
  const int16_t c = cellSize(side);
  const int dx = x - boardLeft(device, side);
  const int dy = y - boardTop();
  if (dx < 0 || dy < 0 || dx >= c * side || dy >= c * side) return -1;
  return (dy / c) * side + dx / c;
}

// Two rows of six: 1 2 3 4 5 NOTES / 6 7 8 9 ERASE (two wide).
fui::Rect keyRect(const fui::DeviceContext& device, const int key) {
  const int16_t gap = 8;
  const int16_t left = toybox::kMargin;
  const int16_t w = static_cast<int16_t>((device.width - toybox::kMargin * 2 - gap * 5) / 6);
  const int16_t top = padTop(device);
  int row = 0;
  int col = 0;
  int span = 1;
  if (key >= 1 && key <= 5) {
    col = key - 1;
  } else if (key == kKeyNotes) {
    col = 5;
  } else if (key >= 6 && key <= 9) {
    row = 1;
    col = key - 6;
  } else {
    row = 1;
    col = 4;
    span = 2;
  }
  return fui::makeRect(static_cast<int16_t>(left + col * (w + gap)),
                       static_cast<int16_t>(top + row * (kPadKeyH + toybox::kGutter / 2)),
                       static_cast<int16_t>(w * span + gap * (span - 1)), kPadKeyH);
}

int keyAt(const fui::DeviceContext& device, const int x, const int y) {
  for (int key = 1; key <= kKeyErase; ++key) {
    const fui::Rect r = keyRect(device, key);
    if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom()) return key;
  }
  return -1;
}

int howToPages() { return 3; }

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toyboxChrome(screen, "KAKURO");
  char record[64];
  int solved = 0;
  int total = 0;
  for (int i = 0; i < kk::kSizeCount; ++i) {
    solved += model.solved[i];
    total += model.total[i];
  }
  std::snprintf(record, sizeof(record), "%d OF %d SOLVED", solved, total);
  const fui::Rect line = screen.takeTop(26);
  fui::TextStyle small;
  small.font = toybox::kTileFont;
  screen.target().text(line, record, small);
  screen.target().fill(fui::makeRect(line.x, static_cast<int16_t>(line.bottom() + 6), line.width, toybox::kRule),
                       fui::Paint::solid(fui::Color::Black));

  static const char* const kSizes[kk::kSizeCount] = {"SMALL 5X5", "MEDIUM 7X7", "LARGE 9X9"};
  char values[kk::kSizeCount][2 * toybox::kIntChars + 2];
  char cont[40];
  fui::ListItem rows[static_cast<int>(MenuRow::Count)] = {};
  int count = 0;
  if (model.canContinue) {
    std::snprintf(cont, sizeof(cont), "CONTINUE %s", sizeName(model.continueSize));
    rows[count].label = cont;
    rows[count].actionValue = static_cast<int16_t>(MenuRow::Continue);
    ++count;
  }
  for (int i = 0; i < kk::kSizeCount; ++i) {
    std::snprintf(values[i], sizeof(values[i]), "%d/%d", model.solved[i], model.total[i]);
    rows[count].label = kSizes[i];
    rows[count].value = values[i];
    rows[count].actionValue = static_cast<int16_t>(static_cast<int>(MenuRow::Small) + i);
    ++count;
  }
  rows[count].label = "HOW TO PLAY";
  rows[count].actionValue = static_cast<int16_t>(MenuRow::HowTo);
  ++count;

  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<uint16_t>(count);
  list.selectedIndex = 0;
  list.action = ActionMenuRow;
  const int16_t listHeight =
      static_cast<int16_t>(count * toybox::kRowHeight + (count - 1) * toybox::kGutter / 2 + toybox::kGutter);
  screen.list(list, listHeight, fui::LayoutAnchor::Bottom);
}

void buildHowTo(toybox::Screen& screen, const HowToModel& model) {
  const int page = model.page < 0 ? 0 : (model.page >= howToPages() ? howToPages() - 1 : model.page);
  char progress[toybox::kOfCounterChars];
  std::snprintf(progress, sizeof(progress), "%d OF %d", page + 1, howToPages());
  toyboxChrome(screen, "HOW TO PLAY", progress);
  fui::ButtonProps next;
  next.label = page + 1 < howToPages() ? "NEXT" : "GOT IT";
  next.action = ActionHowToNext;
  screen.button(next, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
  static const char* const kLines[] = {
      "FILL EVERY WHITE SQUARE WITH A DIGIT FROM 1 TO 9. THE NUMBER IN A BLACK SQUARE IS THE SUM OF THE RUN BESIDE "
      "IT: TOP RIGHT FOR THE RUN ACROSS, BOTTOM LEFT FOR THE RUN DOWN.",
      "NO DIGIT REPEATS INSIDE A RUN. A REPEAT IS UNDERLINED, AND A FULL RUN THAT DOES NOT ADD UP HAS ITS SUM TURNED "
      "WHITE.",
      "TAP A SQUARE, THEN A DIGIT. NOTES PENCILS DIGITS IN INSTEAD. UNDER THE BOARD YOU SEE THE WAYS THE SQUARE'S TWO "
      "RUNS CAN ADD UP: 16 IN 2 CAN ONLY BE 7 AND 9.",
  };
  const fui::Rect area = screen.body();
  fui::TextStyle body;
  body.font = toybox::kBodyFont;
  body.align = fui::TextAlign::Center;
  body.maxLines = 10;
  screen.target().text(fui::makeRect(area.x, area.y, area.width, 400), kLines[page], body);
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  const kk::Layout& l = *model.layout;
  const kk::Progress& p = *model.progress;
  char right[24];
  std::snprintf(right, sizeof(right), "%s %d", sizeName(model.size), model.number);
  toyboxChrome(screen, "KAKURO", right);

  const fui::DeviceContext device = screen.device();
  auto& t = screen.target();
  const int side = l.side;
  const int16_t c = cellSize(side);
  const int16_t left = boardLeft(device, side);
  const int16_t top = boardTop();
  t.stroke(fui::makeRect(static_cast<int16_t>(left - toybox::kFrame), static_cast<int16_t>(top - toybox::kFrame),
                         static_cast<int16_t>(c * side + toybox::kFrame * 2),
                         static_cast<int16_t>(c * side + toybox::kFrame * 2)),
           fui::Paint::solid(fui::Color::Black), toybox::kFrame);

  // The selected square's two runs, lightly dithered, so you can see what the
  // sums under the board are talking about.
  uint64_t lit[2] = {0, 0};
  auto light = [&](const int cell) { lit[cell / 64] |= 1ull << (cell % 64); };
  if (model.selected >= 0 && kk::white(l, model.selected)) {
    const kk::Run* runs[2] = {&l.runs[l.acrossRun[model.selected]], &l.runs[l.downRun[model.selected]]};
    for (const kk::Run* run : runs)
      for (int i = 0; i < run->length; ++i) light(kk::runCell(l, *run, i));
  }

  const fui::FontId digitFont = c >= 56 ? toybox::kDisplayFont : toybox::kUiFont;
  const toybox::CutMetrics& digitCut = c >= 56 ? toybox::kDisplayCut : toybox::kUiCut;
  for (int cell = 0; cell < side * side; ++cell) {
    const fui::Rect r = cellRect(device, side, cell);
    if (!kk::white(l, cell)) {
      bool acrossWrong = false;
      bool downWrong = false;
      if (l.acrossClue[cell] && cell + 1 < side * side) acrossWrong = kk::runWrong(l, p, l.acrossRun[cell + 1]);
      if (l.downClue[cell] && cell + side < side * side) downWrong = kk::runWrong(l, p, l.downRun[cell + side]);
      clueCell(screen, r, l.acrossClue[cell], l.downClue[cell], acrossWrong, downWrong);
      continue;
    }
    if (lit[cell / 64] & (1ull << (cell % 64))) t.fill(r, fui::Paint::dither(fui::Color::LightGray));
    t.stroke(r, fui::Paint::solid(fui::Color::Black), 1);
    if (p.entry[cell]) {
      char d[2] = {static_cast<char>('0' + p.entry[cell]), '\0'};
      text(screen, r, d, digitFont, digitCut, fui::Color::Black);
      if (kk::duplicate(l, p, cell)) {
        t.fill(fui::makeRect(static_cast<int16_t>(r.x + c / 4), static_cast<int16_t>(r.bottom() - c / 6),
                             static_cast<int16_t>(c / 2), 4),
               fui::Paint::solid(fui::Color::Black));
      }
    } else if (p.notes[cell]) {
      // Pencil marks in the keypad's places. Digits where they fit, dots on the
      // largest board where they would not.
      const int16_t step = static_cast<int16_t>(c / 3);
      for (int d = 1; d <= 9; ++d) {
        if (!(p.notes[cell] & (1u << d))) continue;
        const fui::Rect slot = fui::makeRect(static_cast<int16_t>(r.x + ((d - 1) % 3) * step),
                                             static_cast<int16_t>(r.y + ((d - 1) / 3) * step), step, step);
        if (c >= 56) {
          char s[2] = {static_cast<char>('0' + d), '\0'};
          text(screen, slot, s, toybox::kTileFont, toybox::kTileCut, fui::Color::Black);
        } else {
          toybox::disc(screen, static_cast<int16_t>(slot.x + step / 2), static_cast<int16_t>(slot.y + step / 2), 3,
                       fui::Color::Black);
        }
      }
    }
  }
  if (model.selected >= 0) {
    const fui::Rect r = cellRect(device, side, model.selected);
    t.stroke(fui::makeRect(static_cast<int16_t>(r.x + 1), static_cast<int16_t>(r.y + 1),
                           static_cast<int16_t>(r.width - 2), static_cast<int16_t>(r.height - 2)),
             fui::Paint::solid(fui::Color::Black), 5);
  }

  // Under the board: the ways the selected square's runs can add up.
  const int16_t hintTop = static_cast<int16_t>(boardBottom(side) + toybox::kFrame + toybox::kGutter / 2);
  const fui::Rect content = screen.contentRect();
  if (model.solved) {
    text(screen, fui::makeRect(content.x, hintTop, content.width, kHintH), "SOLVED", toybox::kDisplayFont,
         toybox::kDisplayCut, fui::Color::Black);
  } else if (model.selected >= 0 && kk::white(l, model.selected)) {
    const kk::Run& a = l.runs[l.acrossRun[model.selected]];
    const kk::Run& d = l.runs[l.downRun[model.selected]];
    char lineA[64];
    char lineD[64];
    char tmp[48];
    comboLine(a.sum, a.length, tmp, sizeof(tmp));
    std::snprintf(lineA, sizeof(lineA), "ACROSS %s", tmp);
    comboLine(d.sum, d.length, tmp, sizeof(tmp));
    std::snprintf(lineD, sizeof(lineD), "DOWN %s", tmp);
    text(screen, fui::makeRect(content.x, hintTop, content.width, kHintH / 2), lineA, toybox::kTileFont,
         toybox::kTileCut, fui::Color::Black, fui::TextAlign::Left);
    text(screen, fui::makeRect(content.x, static_cast<int16_t>(hintTop + kHintH / 2), content.width, kHintH / 2), lineD,
         toybox::kTileFont, toybox::kTileCut, fui::Color::Black, fui::TextAlign::Left);
  } else {
    char filled[2 * toybox::kIntChars + toybox::literalChars(" OF  SQUARES FILLED. TAP ONE.") + 1];
    std::snprintf(filled, sizeof(filled), "%d OF %d SQUARES FILLED. TAP ONE.", kk::filledCount(l, p),
                  kk::whiteCount(l));
    text(screen, fui::makeRect(content.x, hintTop, content.width, kHintH), filled, toybox::kTileFont, toybox::kTileCut,
         fui::Color::Black);
  }

  if (model.solved) {
    const fui::Rect pad = keyRect(device, 1);
    const fui::Rect last = keyRect(device, kKeyErase);
    fui::ButtonProps next;
    next.label = "NEXT PUZZLE";
    next.action = ActionNext;
    screen.button(next, fui::makeRect(pad.x, pad.y, static_cast<int16_t>(last.right() - pad.x),
                                      static_cast<int16_t>(last.bottom() - pad.y)));
    return;
  }
  for (int key = 1; key <= kKeyErase; ++key) {
    const fui::Rect r = keyRect(device, key);
    const bool on = key == kKeyNotes && model.notes;
    if (on) {
      t.fill(r, fui::Paint::solid(fui::Color::Black), 10);
    } else {
      t.stroke(r, fui::Paint::solid(fui::Color::Black), 2, 10);
    }
    char label[8];
    if (key <= 9) {
      label[0] = static_cast<char>('0' + key);
      label[1] = '\0';
      text(screen, r, label, toybox::kDisplayFont, toybox::kDisplayCut, fui::Color::Black);
    } else {
      std::snprintf(label, sizeof(label), "%s", key == kKeyNotes ? "NOTES" : "ERASE");
      text(screen, r, label, toybox::kTileFont, toybox::kTileCut, on ? fui::Color::White : fui::Color::Black);
    }
  }
}

}  // namespace kakuroui

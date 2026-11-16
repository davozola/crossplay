#include "MastermindScreens.h"

#include <cstdio>

#include "../ui/ToyboxFormat.h"

namespace mastermindui {

namespace {

namespace mm = mastermind;

constexpr int16_t kRowsTop = toybox::kChromeHeight + toybox::kGutter;
constexpr int16_t kPaletteH = 60;
constexpr int16_t kNumberW = 34;

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

// Where the palette and the two buttons sit, from the bottom up.
int16_t buttonsTop(const fui::DeviceContext& device) {
  return static_cast<int16_t>(device.height - 40 - toybox::kGutter - toybox::kPillHeight);
}
int16_t paletteTop(const fui::DeviceContext& device) {
  return static_cast<int16_t>(buttonsTop(device) - toybox::kGutter - kPaletteH);
}
int16_t rowHeight(const fui::DeviceContext& device, const mm::Game& game) {
  const mm::Rules r = mm::rulesFor(game.mode);
  // The code row, when shown, takes one more.
  return static_cast<int16_t>((paletteTop(device) - toybox::kGutter - kRowsTop) / (r.rows + 1));
}
int16_t rowTop(const fui::DeviceContext& device, const mm::Game& game, const int row) {
  return static_cast<int16_t>(kRowsTop + rowHeight(device, game) * (row + 1));
}
int16_t pegSize(const fui::DeviceContext& device, const mm::Game& game) {
  return static_cast<int16_t>(rowHeight(device, game) - 6);
}
int16_t slotX(const fui::DeviceContext& device, const mm::Game& game, const int slot) {
  (void)device;
  const int16_t size = pegSize(device, game);
  return static_cast<int16_t>(toybox::kMargin + kNumberW + slot * (size + 10));
}

void label(toybox::Screen& screen, const fui::Rect& box, const char* s, const fui::Color color = fui::Color::Black,
           const fui::TextAlign align = fui::TextAlign::Center) {
  fui::TextStyle style;
  style.font = toybox::kTileFont;
  style.color = color;
  style.align = align;
  screen.target().text(toybox::inkCentred(box, toybox::kTileCut), s, style);
}

// The answer pegs for one row: solid for black, hollow for white, laid out in
// two rows beside the guess.
void feedback(toybox::Screen& screen, const int16_t x, const int16_t y, const int16_t h, const mm::Score s,
              const int pegs) {
  const int perRow = (pegs + 1) / 2;
  const int16_t r = static_cast<int16_t>(h / 5 < 4 ? 4 : h / 5);
  const int16_t step = static_cast<int16_t>(r * 3);
  for (int i = 0; i < pegs; ++i) {
    const int16_t cx = static_cast<int16_t>(x + r + (i % perRow) * step);
    const int16_t cy = static_cast<int16_t>(y + h / 2 + (i / perRow == 0 ? -r - 2 : r + 2));
    if (i < s.black) {
      toybox::disc(screen, cx, cy, r, fui::Color::Black);
    } else if (i < s.black + s.white) {
      toybox::ring(screen, cx, cy, r, 2, fui::Color::Black, fui::Color::White);
    } else {
      toybox::disc(screen, cx, cy, 2, fui::Color::Black);
    }
  }
}

}  // namespace

void drawSymbol(toybox::Screen& screen, const fui::Rect& box, const int symbol) {
  auto& t = screen.target();
  const int16_t side = box.width < box.height ? box.width : box.height;
  const int16_t inset = static_cast<int16_t>(side / 8);
  const int16_t s = static_cast<int16_t>(side - inset * 2);
  const int16_t x = static_cast<int16_t>(box.x + (box.width - s) / 2);
  const int16_t y = static_cast<int16_t>(box.y + (box.height - s) / 2);
  const bool solid = symbol % 2 == 0;
  const int16_t w = static_cast<int16_t>(s / 7 < 3 ? 3 : s / 7);
  const fui::Paint black = fui::Paint::solid(fui::Color::Black);
  const fui::Paint white = fui::Paint::solid(fui::Color::White);
  switch (symbol / 2) {
    case 0:  // disc
      toybox::ring(screen, static_cast<int16_t>(x + s / 2), static_cast<int16_t>(y + s / 2),
                   static_cast<int16_t>(s / 2), w, fui::Color::Black, solid ? fui::Color::Black : fui::Color::White);
      break;
    case 1:  // square
      t.fill(fui::makeRect(x, y, s, s), solid ? black : white, 4);
      t.stroke(fui::makeRect(x, y, s, s), black, static_cast<uint8_t>(w), 4);
      break;
    case 2: {  // triangle
      const fui::Point a{static_cast<int16_t>(x + s / 2), y};
      const fui::Point b{x, static_cast<int16_t>(y + s)};
      const fui::Point c{static_cast<int16_t>(x + s), static_cast<int16_t>(y + s)};
      t.triangle(a, b, c, black);
      if (!solid) {
        const int16_t k = static_cast<int16_t>(w * 2);
        t.triangle(fui::Point{a.x, static_cast<int16_t>(a.y + k + w)},
                   fui::Point{static_cast<int16_t>(b.x + k + w / 2), static_cast<int16_t>(b.y - w)},
                   fui::Point{static_cast<int16_t>(c.x - k - w / 2), static_cast<int16_t>(c.y - w)}, white);
      }
      break;
    }
    default: {  // diamond
      const fui::Point top{static_cast<int16_t>(x + s / 2), y};
      const fui::Point left{x, static_cast<int16_t>(y + s / 2)};
      const fui::Point right{static_cast<int16_t>(x + s), static_cast<int16_t>(y + s / 2)};
      const fui::Point bottom{static_cast<int16_t>(x + s / 2), static_cast<int16_t>(y + s)};
      t.triangle(top, left, right, black);
      t.triangle(bottom, left, right, black);
      if (!solid) {
        const int16_t k = static_cast<int16_t>(w + w / 2);
        const fui::Point t2{top.x, static_cast<int16_t>(top.y + k)};
        const fui::Point l2{static_cast<int16_t>(left.x + k), left.y};
        const fui::Point r2{static_cast<int16_t>(right.x - k), right.y};
        const fui::Point b2{bottom.x, static_cast<int16_t>(bottom.y - k)};
        t.triangle(t2, l2, r2, white);
        t.triangle(b2, l2, r2, white);
      }
      break;
    }
  }
}

fui::Rect slotRect(const fui::DeviceContext& device, const mm::Game& game, const int slot) {
  const int row = game.rowsUsed;
  const int16_t size = pegSize(device, game);
  return fui::makeRect(slotX(device, game, slot), static_cast<int16_t>(rowTop(device, game, row) + 3), size, size);
}

int slotAt(const fui::DeviceContext& device, const mm::Game& game, const int x, const int y) {
  const mm::Rules r = mm::rulesFor(game.mode);
  for (int i = 0; i < r.pegs; ++i) {
    const fui::Rect s = slotRect(device, game, i);
    if (x >= s.x - 4 && x < s.right() + 4 && y >= s.y - 4 && y < s.bottom() + 4) return i;
  }
  return -1;
}

fui::Rect paletteRect(const fui::DeviceContext& device, const mm::Game& game, const int symbol) {
  const mm::Rules r = mm::rulesFor(game.mode);
  const int16_t gap = 8;
  const int16_t w = static_cast<int16_t>((device.width - toybox::kMargin * 2 - gap * (r.symbols - 1)) / r.symbols);
  return fui::makeRect(static_cast<int16_t>(toybox::kMargin + symbol * (w + gap)), paletteTop(device), w, kPaletteH);
}

int paletteAt(const fui::DeviceContext& device, const mm::Game& game, const int x, const int y) {
  const mm::Rules r = mm::rulesFor(game.mode);
  for (int i = 0; i < r.symbols; ++i) {
    const fui::Rect p = paletteRect(device, game, i);
    if (x >= p.x && x < p.right() && y >= p.y && y < p.bottom()) return i;
  }
  return -1;
}

int howToPages() { return 3; }

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toyboxChrome(screen, "MASTERMIND");
  fui::TextStyle hint;
  hint.font = toybox::kBodyFont;
  hint.align = fui::TextAlign::Center;
  hint.maxLines = 6;
  screen.target().text(screen.takeTop(150), "THE DEVICE HIDES A CODE OF SHAPES. CRACK IT BEFORE YOU RUN OUT OF ROWS.",
                       hint);

  static const char* const kModes[mm::kModeCount] = {"EASY", "CLASSIC", "HARD"};
  char values[mm::kModeCount][2 * toybox::kIntChars + 8];
  char cont[32];
  fui::ListItem rows[static_cast<int>(MenuRow::Count)] = {};
  int count = 0;
  if (model.canContinue) {
    std::snprintf(cont, sizeof(cont), "CONTINUE %s", kModes[static_cast<int>(model.continueMode)]);
    rows[count].label = cont;
    rows[count].actionValue = static_cast<int16_t>(MenuRow::Continue);
    ++count;
  }
  for (int i = 0; i < mm::kModeCount; ++i) {
    std::snprintf(values[i], sizeof(values[i]), "WON %u/%u", static_cast<unsigned>(model.stats[i].won),
                  static_cast<unsigned>(model.stats[i].played));
    rows[count].label = kModes[i];
    rows[count].value = values[i];
    rows[count].actionValue = static_cast<int16_t>(static_cast<int>(MenuRow::Easy) + i);
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
      "THE DEVICE PICKS A SECRET ROW OF SHAPES. TAP SHAPES AT THE BOTTOM TO BUILD A GUESS, TAP A SHAPE IN YOUR ROW TO "
      "TAKE IT BACK, THEN GUESS.",
      "BESIDE EACH GUESS: A SOLID DOT FOR EVERY SHAPE IN THE RIGHT PLACE, A HOLLOW DOT FOR EVERY RIGHT SHAPE IN THE "
      "WRONG PLACE. THE DOTS DO NOT SAY WHICH.",
      "EASY: FOUR OF SIX, NO REPEATS. CLASSIC: FOUR OF SIX, SHAPES MAY REPEAT. HARD: FIVE OF EIGHT, WITH REPEATS, IN "
      "TWELVE ROWS.",
  };
  const fui::Rect area = screen.body();
  fui::TextStyle body;
  body.font = toybox::kBodyFont;
  body.align = fui::TextAlign::Center;
  body.maxLines = 10;
  screen.target().text(fui::makeRect(area.x, area.y, area.width, 300), kLines[page], body);
  if (page == 1) {
    // The two dots, drawn as they appear on the board.
    const int16_t y = static_cast<int16_t>(area.y + 340);
    toybox::disc(screen, static_cast<int16_t>(area.x + area.width / 2 - 60), y, 12, fui::Color::Black);
    toybox::ring(screen, static_cast<int16_t>(area.x + area.width / 2 + 60), y, 12, 3, fui::Color::Black,
                 fui::Color::White);
  }
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  const mm::Game& g = *model.game;
  const mm::Rules r = mm::rulesFor(g.mode);
  char right[2 * toybox::kIntChars + 4];
  const int shownRow = g.rowsUsed < r.rows ? g.rowsUsed + 1 : r.rows;
  std::snprintf(right, sizeof(right), "%d/%d", g.state == mm::State::Playing ? shownRow : g.rowsUsed, r.rows);
  toyboxChrome(screen, "MASTERMIND", right);

  const fui::DeviceContext device = screen.device();
  auto& t = screen.target();
  const int16_t h = rowHeight(device, g);
  const int16_t size = pegSize(device, g);
  const int16_t feedbackX = static_cast<int16_t>(slotX(device, g, r.pegs) + 6);
  const bool over = g.state != mm::State::Playing;

  // The top row is the code: hidden behind blocks until the game ends.
  {
    const int16_t y = kRowsTop;
    label(screen, fui::makeRect(toybox::kMargin, y, kNumberW, h), "?", fui::Color::Black, fui::TextAlign::Left);
    for (int i = 0; i < r.pegs; ++i) {
      const fui::Rect box = fui::makeRect(slotX(device, g, i), static_cast<int16_t>(y + 3), size, size);
      if (over) {
        drawSymbol(screen, box, g.code[i]);
      } else {
        t.fill(box, fui::Paint::dither(fui::Color::DarkGray), 6);
      }
    }
    const char* note = g.state == mm::State::Won ? "CRACKED" : (g.state == mm::State::Lost ? "THE CODE" : "");
    label(screen, fui::makeRect(feedbackX, y, static_cast<int16_t>(device.width - toybox::kMargin - feedbackX), h),
          note, fui::Color::Black, fui::TextAlign::Left);
    t.fill(fui::makeRect(toybox::kMargin, static_cast<int16_t>(y + h - 2),
                         static_cast<int16_t>(device.width - toybox::kMargin * 2), 2),
           fui::Paint::solid(fui::Color::Black));
  }

  for (int row = 0; row < r.rows; ++row) {
    const int16_t y = rowTop(device, g, row);
    char n[toybox::kIntTextChars];
    std::snprintf(n, sizeof(n), "%d", row + 1);
    label(screen, fui::makeRect(toybox::kMargin, y, kNumberW, h), n, fui::Color::Black, fui::TextAlign::Left);
    const bool past = row < g.rowsUsed;
    const bool current = row == g.rowsUsed && !over;
    if (current) {
      t.stroke(fui::makeRect(static_cast<int16_t>(slotX(device, g, 0) - 6), static_cast<int16_t>(y + 1),
                             static_cast<int16_t>(r.pegs * (size + 10) + 2), static_cast<int16_t>(h - 2)),
               fui::Paint::solid(fui::Color::Black), 3, 8);
    }
    for (int i = 0; i < r.pegs; ++i) {
      const fui::Rect box = fui::makeRect(slotX(device, g, i), static_cast<int16_t>(y + 3), size, size);
      const uint8_t symbol = past ? g.guesses[row][i] : (current ? g.current[i] : mm::kEmpty);
      if (symbol != mm::kEmpty) {
        drawSymbol(screen, box, symbol);
      } else {
        toybox::disc(screen, static_cast<int16_t>(box.x + size / 2), static_cast<int16_t>(box.y + size / 2), 3,
                     fui::Color::Black);
      }
    }
    if (past) feedback(screen, feedbackX, y, h, g.scores[row], r.pegs);
  }

  // The palette: every symbol, tap to add to the row. Without repeats, one
  // already in the row is dimmed rather than removed.
  for (int i = 0; i < r.symbols; ++i) {
    const fui::Rect box = paletteRect(device, g, i);
    bool used = false;
    if (!r.repeats)
      for (int k = 0; k < r.pegs; ++k) used |= g.current[k] == i;
    t.stroke(box, fui::Paint::solid(fui::Color::Black), 2, 10);
    if (over) {
      t.fill(box, fui::Paint::dither(fui::Color::LightGray), 10);
    }
    drawSymbol(screen,
               fui::makeRect(static_cast<int16_t>(box.x + 6), static_cast<int16_t>(box.y + 6),
                             static_cast<int16_t>(box.width - 12), static_cast<int16_t>(box.height - 12)),
               i);
    if (used) t.fill(box, fui::Paint::dither(fui::Color::LightGray), 10);
  }

  const fui::Rect buttons =
      fui::makeRect(toybox::kMargin, buttonsTop(device), static_cast<int16_t>(device.width - toybox::kMargin * 2),
                    toybox::kPillHeight);
  const int16_t half = static_cast<int16_t>((buttons.width - toybox::kGutter) / 2);
  fui::ButtonProps a;
  fui::ButtonProps b;
  if (over) {
    a.label = "MENU";
    a.action = ActionMenu;
    a.styles = toybox::rowStyles();
    b.label = "NEW GAME";
    b.action = ActionAgain;
  } else {
    a.label = "CLEAR";
    a.action = ActionClear;
    a.styles = toybox::rowStyles();
    b.label = "GUESS";
    b.action = ActionGuess;
    b.enabled = mm::rowFull(g);
    if (!b.enabled) {
      b.styles = toybox::rowStyles();
      b.state = fui::StateDisabled;
    }
  }
  screen.button(a, fui::makeRect(buttons.x, buttons.y, half, buttons.height));
  screen.button(b, fui::makeRect(static_cast<int16_t>(buttons.right() - half), buttons.y, half, buttons.height));
}

}  // namespace mastermindui

#include "TakScreens.h"

#include <cstdio>
#include <cstring>

#include "../ui/ToyboxFormat.h"

namespace takui {

namespace {

namespace tk = tak;

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

constexpr int16_t kBoardSide = 448;
constexpr int16_t kGrid = 2;

int16_t cellFor(const int size) { return static_cast<int16_t>(kBoardSide / size); }

int16_t boardLeft(const fui::DeviceContext& device, const int size) {
  return static_cast<int16_t>((device.width - cellFor(size) * size) / 2);
}

int16_t boardTop() { return static_cast<int16_t>(toybox::kChromeHeight + toybox::kGutter * 2 + toybox::kBoardFrame); }

int16_t boardBottom(const int size) { return static_cast<int16_t>(boardTop() + cellFor(size) * size); }

fui::Paint ink(const uint8_t color) {
  return fui::Paint::solid(color == tk::kBlack ? fui::Color::Black : fui::Color::White);
}

// One piece, centred in `box`. White is outlined, black is solid: filled
// means whose, as on every other board in the fork.
//   flat  a square tile
//   wall  a tall narrow slab
//   cap   a disc
void drawPiece(toybox::Screen& screen, const fui::Rect& box, const tk::Stone stone, const uint8_t color,
               const int16_t weight) {
  auto& t = screen.target();
  const int16_t cx = static_cast<int16_t>(box.x + box.width / 2);
  const int16_t cy = static_cast<int16_t>(box.y + box.height / 2);
  const int16_t side = box.width < box.height ? box.width : box.height;
  switch (stone) {
    case tk::Stone::Flat: {
      const fui::Rect r =
          fui::makeRect(static_cast<int16_t>(cx - side / 2), static_cast<int16_t>(cy - side / 2), side, side);
      t.fill(r, ink(color), static_cast<uint8_t>(side / 6));
      t.stroke(r, fui::Paint::solid(fui::Color::Black), static_cast<uint8_t>(weight), static_cast<uint8_t>(side / 6));
      break;
    }
    case tk::Stone::Wall: {
      const int16_t w = static_cast<int16_t>(side * 3 / 10 < 6 ? 6 : side * 3 / 10);
      const fui::Rect r = fui::makeRect(static_cast<int16_t>(cx - w / 2), static_cast<int16_t>(cy - side / 2), w, side);
      t.fill(r, ink(color), static_cast<uint8_t>(w / 3));
      t.stroke(r, fui::Paint::solid(fui::Color::Black), static_cast<uint8_t>(weight), static_cast<uint8_t>(w / 3));
      break;
    }
    case tk::Stone::Cap:
      toybox::ring(screen, cx, cy, static_cast<int16_t>(side / 2), weight, fui::Color::Black,
                   color == tk::kBlack ? fui::Color::Black : fui::Color::White);
      // A dot of the other ink, so a capstone never reads as a big flat.
      toybox::disc(screen, cx, cy, static_cast<int16_t>(side / 7 < 3 ? 3 : side / 7),
                   color == tk::kBlack ? fui::Color::White : fui::Color::Black);
      break;
  }
}

void heightBadge(toybox::Screen& screen, const fui::Rect& cell, const int height) {
  char text[toybox::kIntTextChars];
  std::snprintf(text, sizeof(text), "%d", height);
  const int16_t w = static_cast<int16_t>(height >= 10 ? 30 : 22);
  const fui::Rect badge =
      fui::makeRect(static_cast<int16_t>(cell.right() - w - 3), static_cast<int16_t>(cell.y + 3), w, 22);
  screen.target().fill(badge, fui::Paint::solid(fui::Color::Black), 6);
  fui::TextStyle s;
  s.font = toybox::kTileFont;
  s.align = fui::TextAlign::Center;
  s.color = fui::Color::White;
  screen.target().text(toybox::inkCentred(badge, toybox::kTileCut), text, s);
}

// The contents of one square: the top piece, and a count once there is
// anything under it. Who owns the inside of a stack is shown in the tray when
// you lift it, which is when it matters.
void drawStack(toybox::Screen& screen, const fui::Rect& cell, const tk::Square& sq) {
  if (tk::empty(sq)) return;
  const int16_t c = cell.width;
  const int16_t weight = static_cast<int16_t>(c >= 80 ? 4 : 3);
  const int16_t piece = static_cast<int16_t>(c * 58 / 100);
  drawPiece(screen,
            fui::makeRect(static_cast<int16_t>(cell.x + (c - piece) / 2),
                          static_cast<int16_t>(cell.y + (c - piece) / 2), piece, piece),
            sq.top, tk::topColor(sq), weight);
  if (sq.height > 1) heightBadge(screen, cell, sq.height);
}

void drawBoard(toybox::Screen& screen, const tk::Game& game, const uint64_t marked, const int selected,
               const uint64_t road) {
  const fui::DeviceContext device = screen.device();
  const int size = game.size;
  const int16_t c = cellFor(size);
  const int16_t left = boardLeft(device, size);
  const int16_t top = boardTop();
  auto& t = screen.target();
  const fui::Rect frame =
      fui::makeRect(static_cast<int16_t>(left - toybox::kBoardFrame), static_cast<int16_t>(top - toybox::kBoardFrame),
                    static_cast<int16_t>(c * size + toybox::kBoardFrame * 2),
                    static_cast<int16_t>(c * size + toybox::kBoardFrame * 2));
  t.stroke(frame, fui::Paint::solid(fui::Color::Black), toybox::kBoardFrame);
  for (int i = 0; i < size * size; ++i) {
    const fui::Rect cell = squareRect(device, size, i);
    // The last move's squares are dithered, the way the fork marks "just
    // happened" without stealing the ink that means "whose".
    if (marked & (1ull << i)) t.fill(cell, fui::Paint::dither(fui::Color::LightGray));
  }
  for (int i = 1; i < size; ++i) {
    t.fill(fui::makeRect(static_cast<int16_t>(left + i * c - kGrid / 2), top, kGrid, static_cast<int16_t>(c * size)),
           fui::Paint::solid(fui::Color::Black));
    t.fill(fui::makeRect(left, static_cast<int16_t>(top + i * c - kGrid / 2), static_cast<int16_t>(c * size), kGrid),
           fui::Paint::solid(fui::Color::Black));
  }
  for (int i = 0; i < size * size; ++i) {
    const fui::Rect cell = squareRect(device, size, i);
    if (road & (1ull << i)) {
      t.fill(fui::makeRect(static_cast<int16_t>(cell.x + 2), static_cast<int16_t>(cell.y + 2),
                           static_cast<int16_t>(cell.width - 4), static_cast<int16_t>(cell.height - 4)),
             fui::Paint::dither(fui::Color::DarkGray));
    }
    drawStack(screen, cell, game.sq[i]);
  }
  // The held stack's square gets corner brackets, drawn last so neighbours
  // cannot paint over them.
  if (selected >= 0) {
    const fui::Rect cell = squareRect(device, size, selected);
    toybox::bracket(screen,
                    fui::makeRect(static_cast<int16_t>(cell.x + 3), static_cast<int16_t>(cell.y + 3),
                                  static_cast<int16_t>(cell.width - 6), static_cast<int16_t>(cell.height - 6)),
                    static_cast<int16_t>(c / 4), 5);
  }
  // File and rank letters would cost a margin the 6x6 board does not have;
  // the PTN of the last move is printed under the board instead.
}

void reserve(toybox::Screen& screen, const fui::Rect& box, const char* who, const uint8_t color, const int stones,
             const int caps, const bool hasCaps, const bool toMove) {
  auto& t = screen.target();
  fui::TextStyle label;
  label.font = toybox::kTileFont;
  label.align = fui::TextAlign::Left;
  const int16_t chip = 22;
  int16_t x = box.x;
  if (toMove) {
    // A small arrow on the side whose move it is.
    const int16_t cy = static_cast<int16_t>(box.y + box.height / 2);
    t.triangle(fui::Point{x, static_cast<int16_t>(cy - 8)}, fui::Point{x, static_cast<int16_t>(cy + 8)},
               fui::Point{static_cast<int16_t>(x + 10), cy}, fui::Paint::solid(fui::Color::Black));
  }
  x = static_cast<int16_t>(x + 16);
  const int16_t labelW = 74;
  t.text(toybox::inkCentred(fui::makeRect(x, box.y, labelW, box.height), toybox::kTileCut), who, label);
  x = static_cast<int16_t>(x + labelW);
  drawPiece(screen, fui::makeRect(x, static_cast<int16_t>(box.y + (box.height - chip) / 2), chip, chip),
            tk::Stone::Flat, color, 3);
  char n[toybox::kIntTextChars];
  std::snprintf(n, sizeof(n), "%d", stones);
  x = static_cast<int16_t>(x + chip + 6);
  t.text(toybox::inkCentred(fui::makeRect(x, box.y, 40, box.height), toybox::kTileCut), n, label);
  if (!hasCaps) return;
  x = static_cast<int16_t>(x + 40);
  drawPiece(screen, fui::makeRect(x, static_cast<int16_t>(box.y + (box.height - chip) / 2), chip, chip), tk::Stone::Cap,
            color, 3);
  std::snprintf(n, sizeof(n), "%d", caps);
  t.text(toybox::inkCentred(fui::makeRect(static_cast<int16_t>(x + chip + 6), box.y, 30, box.height), toybox::kTileCut),
         n, label);
}

const char* sideName(const bool vsComputer, const uint8_t human, const uint8_t color) {
  if (vsComputer) return color == human ? "YOU" : "CPU";
  return color == tk::kWhite ? "WHITE" : "BLACK";
}

}  // namespace

fui::Rect squareRect(const fui::DeviceContext& device, const int size, const int square) {
  const int16_t c = cellFor(size);
  return fui::makeRect(static_cast<int16_t>(boardLeft(device, size) + (square % size) * c),
                       static_cast<int16_t>(boardTop() + (square / size) * c), c, c);
}

int squareAt(const fui::DeviceContext& device, const int size, const int x, const int y) {
  const int16_t c = cellFor(size);
  const int dx = x - boardLeft(device, size);
  const int dy = y - boardTop();
  if (dx < 0 || dy < 0 || dx >= c * size || dy >= c * size) return -1;
  return (dy / c) * size + dx / c;
}

int howToPages() { return 5; }

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toyboxChrome(screen, "TAK");

  char record[48];
  std::snprintf(record, sizeof(record), "%d PLAYED   %d WON", model.played, model.won);
  const fui::Rect line = screen.takeTop(26);
  fui::TextStyle small;
  small.font = toybox::kTileFont;
  small.align = fui::TextAlign::Left;
  screen.target().text(line, record, small);
  screen.target().fill(fui::makeRect(line.x, static_cast<int16_t>(line.bottom() + 6), line.width, toybox::kRule),
                       fui::Paint::solid(fui::Color::Black));

  static const char* const kLevels[] = {"EASY", "MEDIUM", "HARD"};
  char sizeLabel[2 * toybox::kIntChars + toybox::literalChars("BOARD X") + 1];
  std::snprintf(sizeLabel, sizeof(sizeLabel), "BOARD %dX%d", model.size, model.size);
  char levelLabel[40];
  std::snprintf(levelLabel, sizeof(levelLabel), "LEVEL %s  %d-%d", kLevels[static_cast<int>(model.level) % 3],
                model.levelWon % 10000, model.levelLost % 10000);

  fui::ListItem rows[static_cast<int>(MenuRow::Count)] = {};
  int count = 0;
  auto add = [&](const char* label, const MenuRow row) {
    rows[count].label = label;
    rows[count].actionValue = static_cast<int16_t>(row);
    ++count;
  };
  if (model.canResume) add("RESUME", MenuRow::Resume);
  add("PLAY THE COMPUTER", MenuRow::PlayComputer);
  add("TWO PLAYERS", MenuRow::PlayTwo);
  add(sizeLabel, MenuRow::Size);
  add(levelLabel, MenuRow::Level);
  add("HOW TO PLAY", MenuRow::HowTo);

  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<uint16_t>(count);
  list.selectedIndex = 0;
  list.action = ActionMenuRow;
  const int16_t listHeight =
      static_cast<int16_t>(count * toybox::kRowHeight + (count - 1) * toybox::kGutter / 2 + toybox::kGutter);
  const fui::Rect content = screen.contentRect();
  const int16_t listTop = static_cast<int16_t>(content.bottom() - listHeight);
  screen.list(list, listHeight, fui::LayoutAnchor::Bottom);

  // The game under RESUME, small, between the record and the doors: ornament
  // made of the app's own material and the player's own data.
  if (model.current == nullptr) return;
  const tk::Game& g = *model.current;
  const int16_t areaTop = static_cast<int16_t>(line.bottom() + 6 + toybox::kRule + toybox::kGutter);
  const int16_t captionH = 26;
  const int16_t room = static_cast<int16_t>(listTop - areaTop - captionH - toybox::kGutter * 2);
  if (room < 90) return;
  const int16_t cell = static_cast<int16_t>(room / g.size > 40 ? 40 : room / g.size);
  const int16_t side = static_cast<int16_t>(cell * g.size);
  const int16_t left = static_cast<int16_t>(content.x + (content.width - side) / 2);
  const int16_t top = static_cast<int16_t>(areaTop + (room - side) / 2);
  auto& t = screen.target();
  t.stroke(
      fui::makeRect(static_cast<int16_t>(left - toybox::kFrame), static_cast<int16_t>(top - toybox::kFrame),
                    static_cast<int16_t>(side + toybox::kFrame * 2), static_cast<int16_t>(side + toybox::kFrame * 2)),
      fui::Paint::solid(fui::Color::Black), toybox::kFrame);
  for (int i = 1; i < g.size; ++i) {
    t.fill(fui::makeRect(static_cast<int16_t>(left + i * cell), top, 1, side), fui::Paint::solid(fui::Color::Black));
    t.fill(fui::makeRect(left, static_cast<int16_t>(top + i * cell), side, 1), fui::Paint::solid(fui::Color::Black));
  }
  for (int i = 0; i < g.size * g.size; ++i) {
    const tk::Square& sq = g.sq[i];
    if (tk::empty(sq)) continue;
    const int16_t p = static_cast<int16_t>(cell * 6 / 10);
    drawPiece(screen,
              fui::makeRect(static_cast<int16_t>(left + (i % g.size) * cell + (cell - p) / 2),
                            static_cast<int16_t>(top + (i / g.size) * cell + (cell - p) / 2), p, p),
              sq.top, tk::topColor(sq), 2);
  }
  char caption[2 * toybox::kIntChars + toybox::literalChars("X AGAINST THE COMPUTER") + 1];
  if (model.vsComputer) {
    std::snprintf(caption, sizeof(caption), "%dX%d AGAINST THE COMPUTER", g.size, g.size);
  } else {
    std::snprintf(caption, sizeof(caption), "%dX%d, TWO PLAYERS", g.size, g.size);
  }
  fui::TextStyle cap;
  cap.font = toybox::kTileFont;
  cap.align = fui::TextAlign::Center;
  t.text(fui::makeRect(content.x, static_cast<int16_t>(top + side + toybox::kGutter), content.width, captionH), caption,
         cap);
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
      "EACH TURN, PLACE ONE PIECE ON AN EMPTY SQUARE OR MOVE ONE STACK YOU OWN. JOIN TWO OPPOSITE EDGES WITH A ROAD "
      "OF YOUR PIECES TO WIN.",
      "YOUR VERY FIRST MOVE PLACES ONE OF YOUR OPPONENT'S FLATS, ANYWHERE. FROM THEN ON YOU PLACE YOUR OWN.",
      "A WALL STANDS UP. IT BLOCKS ROADS AND STACKS AND IS NOT PART OF A ROAD. THE CAPSTONE IS ROAD, AND MOVING ALONE "
      "IT FLATTENS A WALL.",
      "TO MOVE A STACK WHOSE TOP IS YOURS, TAP IT, THEN TAP SQUARES IN A STRAIGHT LINE. EACH TAP DROPS ONE PIECE. TAP "
      "THE SAME SQUARE TO DROP ANOTHER, OR THE STACK TO LIFT ONE FEWER.",
      "IF THE BOARD FILLS OR ANYONE RUNS OUT OF PIECES, THE ONE WITH MORE FLATS ON TOP WINS.",
  };
  const fui::Rect area = screen.body();
  fui::TextStyle body;
  body.font = toybox::kBodyFont;
  body.align = fui::TextAlign::Center;
  body.maxLines = 8;
  screen.target().text(fui::makeRect(area.x, area.y, area.width, 260), kLines[page], body);

  // A small picture of the pieces on the page that names them.
  if (page == 2) {
    const int16_t side = 72;
    const int16_t y = static_cast<int16_t>(area.y + 300);
    const int16_t gap = 36;
    const int16_t x0 = static_cast<int16_t>(area.x + (area.width - side * 3 - gap * 2) / 2);
    static const tk::Stone kStones[3] = {tk::Stone::Flat, tk::Stone::Wall, tk::Stone::Cap};
    static const char* const kNames[3] = {"FLAT", "WALL", "CAPSTONE"};
    fui::TextStyle cap;
    cap.font = toybox::kTileFont;
    cap.align = fui::TextAlign::Center;
    for (int i = 0; i < 3; ++i) {
      const int16_t x = static_cast<int16_t>(x0 + i * (side + gap));
      drawPiece(screen, fui::makeRect(x, y, side, side), kStones[i], tk::kBlack, 4);
      drawPiece(screen, fui::makeRect(x, static_cast<int16_t>(y + side + 20), side, side), kStones[i], tk::kWhite, 4);
      screen.target().text(fui::makeRect(static_cast<int16_t>(x - gap / 2), static_cast<int16_t>(y + side * 2 + 34),
                                         static_cast<int16_t>(side + gap), 26),
                           kNames[i], cap);
    }
  }
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  const tk::Game& g = model.game;
  char sizeLabel[2 * toybox::kIntChars + toybox::literalChars("X") + 1];
  std::snprintf(sizeLabel, sizeof(sizeLabel), "%dX%d", g.size, g.size);
  toyboxChrome(screen, "TAK", sizeLabel);

  drawBoard(screen, model.shown, model.lastMove, model.holding ? model.from : -1, 0);

  const fui::DeviceContext device = screen.device();
  const fui::Rect content = screen.contentRect();
  auto& t = screen.target();
  int16_t y = static_cast<int16_t>(boardBottom(g.size) + toybox::kBoardFrame + toybox::kGutter);

  // Status: whose move, and the last one played.
  char status[48];
  const bool humanToMove = !model.vsComputer || g.turn == model.human;
  if (model.thinking) {
    std::snprintf(status, sizeof(status), "THINKING...");
  } else if (model.vsComputer) {
    std::snprintf(status, sizeof(status), "%s", humanToMove ? "YOUR MOVE" : "CPU TO MOVE");
  } else {
    std::snprintf(status, sizeof(status), "%s TO MOVE", g.turn == tk::kWhite ? "WHITE" : "BLACK");
  }
  if (tk::openingPly(g) && !model.thinking && humanToMove) {
    std::snprintf(status, sizeof(status), "PLACE A %s FLAT", g.turn == tk::kWhite ? "BLACK" : "WHITE");
  }
  fui::TextStyle st;
  st.font = toybox::kTileFont;
  st.align = fui::TextAlign::Left;
  const fui::Rect statusBox = fui::makeRect(content.x, y, content.width, 30);
  t.text(toybox::inkCentred(statusBox, toybox::kTileCut), status, st);
  if (model.lastPtn[0] != '\0') {
    char last[24];
    std::snprintf(last, sizeof(last), "LAST %s", model.lastPtn);
    fui::TextStyle right = st;
    right.align = fui::TextAlign::Right;
    t.text(toybox::inkCentred(statusBox, toybox::kTileCut), last, right);
  }
  y = static_cast<int16_t>(y + 30 + toybox::kGutter / 2);

  // Reserves, both sides, the mover marked.
  const bool caps = tk::capsFor(g.size) > 0;
  const int16_t half = static_cast<int16_t>(content.width / 2);
  const uint8_t first = model.vsComputer ? model.human : tk::kWhite;
  const uint8_t second = tk::other(first);
  reserve(screen, fui::makeRect(content.x, y, half, 34), sideName(model.vsComputer, model.human, first), first,
          g.stones[first], g.caps[first], caps, g.turn == first);
  reserve(screen, fui::makeRect(static_cast<int16_t>(content.x + half), y, half, 34),
          sideName(model.vsComputer, model.human, second), second, g.stones[second], g.caps[second], caps,
          g.turn == second);
  y = static_cast<int16_t>(y + 34 + toybox::kGutter);

  // The tray: what a tap on an empty square will place, or what is in hand.
  fui::Rect tray = fui::makeRect(content.x, y, content.width, toybox::kPillHeight);
  if (model.canUndo && !model.holding) {
    // UNDO takes the right end of the tray; the stones share what is left.
    const int16_t undoW = 110;
    fui::ButtonProps undo;
    undo.label = "UNDO";
    undo.action = ActionUndo;
    undo.styles = toybox::rowStyles();
    screen.button(undo, fui::makeRect(static_cast<int16_t>(tray.right() - undoW), tray.y, undoW, tray.height));
    tray.width = static_cast<int16_t>(tray.width - undoW - toybox::kGutter);
  }
  if (model.holding) {
    const int16_t cancelW = 150;
    fui::ButtonProps cancel;
    cancel.label = "CANCEL";
    cancel.action = ActionCancel;
    cancel.styles = toybox::rowStyles();
    screen.button(cancel, fui::makeRect(static_cast<int16_t>(tray.right() - cancelW), tray.y, cancelW, tray.height));
    // The hand, bottom piece first, left to right, the top piece as itself.
    const int16_t chip = 30;
    int16_t x = tray.x;
    fui::TextStyle lbl = st;
    t.text(toybox::inkCentred(fui::makeRect(x, tray.y, 70, tray.height), toybox::kTileCut), "HAND", lbl);
    x = static_cast<int16_t>(x + 74);
    for (int i = 0; i < model.handCount && i < 8; ++i) {
      const uint8_t color = static_cast<uint8_t>((model.handColors >> i) & 1u);
      const tk::Stone s = i == model.handCount - 1 ? model.handTop : tk::Stone::Flat;
      drawPiece(screen, fui::makeRect(x, static_cast<int16_t>(tray.y + (tray.height - chip) / 2), chip, chip), s, color,
                3);
      x = static_cast<int16_t>(x + chip + 4);
    }
  } else if (model.thinking || !humanToMove) {
    fui::TextStyle mid = st;
    mid.align = fui::TextAlign::Center;
    t.text(toybox::inkCentred(tray, toybox::kTileCut), model.thinking ? "THE COMPUTER IS THINKING" : "", mid);
  } else {
    const int n = caps ? 3 : 2;
    const int16_t gap = toybox::kGutter;
    const int16_t w = static_cast<int16_t>((tray.width - gap * (n - 1)) / n);
    static const char* const kNames[3] = {"FLAT", "WALL", "CAP"};
    for (int i = 0; i < n; ++i) {
      const tk::Stone s = static_cast<tk::Stone>(i);
      fui::ButtonProps b;
      b.label = kNames[i];
      b.action = ActionTray;
      b.value = static_cast<int16_t>(i);
      b.styles = toybox::rowStyles();
      const bool available = tk::trayAvailable(g, s);
      // Dimmed, never removed: a missing CAP would read as a different screen.
      b.styles.disabled.background = fui::Paint::dither(fui::Color::LightGray);
      b.enabled = available;
      b.state =
          (model.tray == s && available) ? fui::StateSelected : (available ? fui::StateNormal : fui::StateDisabled);
      screen.button(b, fui::makeRect(static_cast<int16_t>(tray.x + i * (w + gap)), tray.y, w, tray.height));
    }
  }
  (void)device;
}

void buildResult(toybox::Screen& screen, const ResultModel& model) {
  const tk::Game& g = model.game;
  toyboxChrome(screen, "TAK", "GAME OVER");
  drawBoard(screen, g, 0, -1, model.road);

  const fui::Rect content = screen.contentRect();
  int16_t y = static_cast<int16_t>(boardBottom(g.size) + toybox::kBoardFrame + toybox::kGutter);
  const uint8_t w = tk::winner(g.result);
  char headline[40];
  if (w == 0xFF) {
    std::snprintf(headline, sizeof(headline), "A DRAW ON FLATS");
  } else if (model.vsComputer) {
    std::snprintf(headline, sizeof(headline), "%s %s", w == model.human ? "YOU WIN" : "THE COMPUTER WINS",
                  tk::roadWin(g.result) ? "BY ROAD" : "ON FLATS");
  } else {
    std::snprintf(headline, sizeof(headline), "%s WINS %s", w == tk::kWhite ? "WHITE" : "BLACK",
                  tk::roadWin(g.result) ? "BY ROAD" : "ON FLATS");
  }
  fui::TextStyle head;
  head.font = toybox::kDisplayFont;
  head.align = fui::TextAlign::Center;
  screen.target().text(toybox::inkCentred(fui::makeRect(content.x, y, content.width, 44), toybox::kDisplayCut),
                       headline, head);
  y = static_cast<int16_t>(y + 44 + toybox::kGutter / 2);
  char flats[2 * toybox::kIntChars + toybox::literalChars("FLATS  WHITE   BLACK ") + 1];
  std::snprintf(flats, sizeof(flats), "FLATS  WHITE %d  BLACK %d", tk::flatCount(g, tk::kWhite),
                tk::flatCount(g, tk::kBlack));
  fui::TextStyle small;
  small.font = toybox::kTileFont;
  small.align = fui::TextAlign::Center;
  screen.target().text(toybox::inkCentred(fui::makeRect(content.x, y, content.width, 28), toybox::kTileCut), flats,
                       small);

  const fui::Rect buttons = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
  const int16_t half = static_cast<int16_t>((buttons.width - toybox::kGutter) / 2);
  fui::ButtonProps again;
  again.label = "PLAY AGAIN";
  again.action = ActionAgain;
  screen.button(again, fui::makeRect(buttons.x, buttons.y, half, buttons.height));
  fui::ButtonProps done;
  done.label = "DONE";
  done.action = ActionDone;
  done.styles = toybox::rowStyles();
  screen.button(done, fui::makeRect(static_cast<int16_t>(buttons.right() - half), buttons.y, half, buttons.height));
}

}  // namespace takui

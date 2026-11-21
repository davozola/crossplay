#include "RssScreens.h"

#include <cstdio>

namespace rssui {
namespace {

constexpr int kSide = 18;  // the page's side margin
constexpr int kTop = 12;
constexpr int kBar = 44;  // the line of controls at the foot
constexpr int kHair = 1;
constexpr int kRule = 3;
constexpr int kRowPad = 11;
constexpr int kKickerGap = 3;
constexpr int kMark = 8;  // the square in front of an unread post's kicker
constexpr int kMarkGap = 8;
constexpr int kMenuRow = 58;

fui::TextStyle style(const fui::FontId slot, const fui::TextAlign align = fui::TextAlign::Left,
                     const uint8_t maxLines = 1, const fui::Color color = fui::Color::Black) {
  fui::TextStyle s;
  s.font = slot;
  s.align = align;
  s.maxLines = maxLines;
  s.color = color;
  return s;
}

fui::Rect rectOf(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h < 0 ? 0 : h));
}

void fill(fui::DrawTarget& target, const int x, const int y, const int w, const int h) {
  target.fill(rectOf(x, y, w, h), fui::Paint::solid(fui::Color::Black));
}

// A place that takes a tap and draws nothing: what is seen there is set over it.
void hit(toybox::Screen& screen, const fui::Rect rect, const fui::ActionId action, const int value = 0) {
  fui::StyleSet plain;
  plain.explicitlySet = true;
  plain.normal.background = fui::Paint::solid(fui::Color::White);
  plain.normal.foreground = fui::Paint::solid(fui::Color::Black);
  plain.normal.border = fui::Paint::solid(fui::Color::White);
  plain.normal.borderWidth = 0;
  plain.selected = plain.normal;
  plain.focused = plain.normal;
  plain.active = plain.normal;
  plain.disabled = plain.normal;
  fui::ButtonProps button;
  button.label = "";
  button.action = action;
  button.value = static_cast<int16_t>(value);
  button.styles = plain;
  button.radius = 0;
  screen.button(button, rect);
}

int wrappedHeight(const fui::DrawTarget& target, const char* text, const fui::TextStyle& s, const int width) {
  const int line = target.lineHeight(s.font);
  const int full = fui::measureWrappedText(target, text, s, static_cast<int16_t>(width)).height;
  const int cap = line * s.maxLines;
  return full > cap ? cap : (full < line ? line : full);
}

int mastheadBottom(const Ink& ink) { return kTop + ink.page.lineHeight(fui::FONT_SLOT_TITLE) + 2 + kRule + 2 + kHair; }

// The masthead: a name in the display serif, a few words opposite it, and the
// double rule a paper puts under its title. Returns where the page starts.
int masthead(toybox::Screen& screen, const Ink& ink, const char* name, const char* right, const fui::ActionId action) {
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  const int nameH = ink.page.lineHeight(fui::FONT_SLOT_TITLE);
  const int smallH = ink.page.lineHeight(fui::FONT_SLOT_SMALL);
  if (action != fui::NO_ACTION) hit(screen, rectOf(0, 0, device.width, kTop + nameH + 8), action);
  int rightW = 0;
  if (right != nullptr && right[0] != '\0') {
    const fui::TextStyle s = style(fui::FONT_SLOT_SMALL, fui::TextAlign::Right);
    rightW = ink.page.measureText(s.font, right, s).width + 12;
    ink.page.text(rectOf(kSide, kTop + nameH - smallH - 6, width, smallH), right, s);
  }
  // A feed's name can be longer than the line: it stops short of the words
  // opposite rather than running under them.
  ink.page.text(rectOf(kSide, kTop, width - rightW, nameH), name, style(fui::FONT_SLOT_TITLE));
  const int y = kTop + nameH + 2;
  fill(ink.page, kSide, y, width, kRule);
  fill(ink.page, kSide, y + kRule + 2, width, kHair);
  return y + kRule + 2 + kHair;
}

// The foot: up to three words, each a control, over a hairline.
struct Word {
  const char* text = nullptr;
  fui::ActionId action = fui::NO_ACTION;
};
void foot(toybox::Screen& screen, const Ink& ink, const Word* words, const int count) {
  const fui::DeviceContext& device = screen.device();
  const int y = device.height - kBar;
  const int each = device.width / (count > 0 ? count : 1);
  for (int i = 0; i < count; ++i) {
    if (words[i].text != nullptr && words[i].action != fui::NO_ACTION) {
      hit(screen, rectOf(i * each, y, each, kBar), words[i].action);
    }
  }
  fill(ink.page, kSide, y, device.width - 2 * kSide, kHair);
  const int lineH = ink.heads.lineHeight(fui::FONT_SLOT_SMALL);
  const int textY = y + (kBar - lineH) / 2 + 1;
  for (int i = 0; i < count; ++i) {
    if (words[i].text == nullptr) continue;
    const fui::TextAlign align = count == 1       ? fui::TextAlign::Center
                                 : i == 0         ? fui::TextAlign::Left
                                 : i == count - 1 ? fui::TextAlign::Right
                                                  : fui::TextAlign::Center;
    const int x = i * each + (i == 0 ? kSide : 0);
    const int w = each - (i == 0 || i == count - 1 ? kSide : 0);
    ink.heads.text(rectOf(x, textY, w, lineH), words[i].text, style(fui::FONT_SLOT_SMALL, align));
  }
}

// A headline, a short rule and a sentence, the way NEWS sets a notice. Returns
// the y under them.
int statement(const Ink& ink, const int width, int y, const char* headline, const char* message) {
  const fui::TextStyle head = style(fui::FONT_SLOT_TITLE, fui::TextAlign::Left, 4);
  const int headH = wrappedHeight(ink.heads, headline, head, width);
  ink.heads.text(rectOf(kSide, y, width, headH), headline, head);
  y += headH + 14;
  fill(ink.page, kSide, y, 56, kRule);
  y += kRule + 16;
  if (message != nullptr && message[0] != '\0') {
    const fui::TextStyle body = style(fui::FONT_SLOT_BODY, fui::TextAlign::Left, 12);
    const int bodyH = wrappedHeight(ink.page, message, body, width);
    ink.page.text(rectOf(kSide, y, width, bodyH), message, body);
    y += bodyH;
  }
  return y;
}

// An unread headline is bold, an opened one the same size in the plain face.
fui::TextStyle headlineStyle(const bool unread) {
  return style(unread ? fui::FONT_SLOT_BODY : fui::FONT_SLOT_SMALL, fui::TextAlign::Left, 3);
}

bool hasKicker(const Row& row) { return row.kicker != nullptr && row.kicker[0] != '\0'; }

struct RowSize {
  int kicker = 0;
  int title = 0;
  int height = 0;
};
RowSize rowSize(const Ink& ink, const int width, const Row& row) {
  RowSize size;
  size.kicker = hasKicker(row) || row.unread ? ink.page.lineHeight(fui::FONT_SLOT_SMALL) : 0;
  size.title = wrappedHeight(ink.heads, row.title, headlineStyle(row.unread), width);
  size.height = kRowPad + (size.kicker > 0 ? size.kicker + kKickerGap : 0) + size.title + kRowPad;
  return size;
}

}  // namespace

// --- The front -----------------------------------------------------------

int frontFit(const Ink& ink, const fui::DeviceContext& device, const Row* rows, const int count) {
  const int width = device.width - 2 * kSide;
  const int bottom = device.height - kBar;
  int y = mastheadBottom(ink);
  int shown = 0;
  for (int i = 0; i < count && shown < kMaxRows; ++i) {
    const int height = rowSize(ink, width, rows[i]).height;
    if (y + height > bottom && shown > 0) break;
    y += height;
    ++shown;
  }
  return shown;
}

Empty emptyFor(const Tab tab, const bool haveFeeds) {
  Empty empty;
  if (tab == Tab::Saved) {
    empty.headline = "Nothing saved yet";
    empty.message = "Open a post and tap Save. It stays here, and its text stays on the card.";
    return empty;
  }
  if (!haveFeeds) {
    // The ordinary first screen, and the whole of how feeds arrive: there is
    // no keyboard worth typing an address on, so the phone does it.
    empty.headline = "No feeds yet";
    empty.message = "Add the sites you read from your phone. Their posts arrive here as text.";
    empty.actionLabel = "Add feeds";
    empty.action = ActionAddFeeds;
    return empty;
  }
  if (tab == Tab::Feeds) {
    empty.headline = "Nothing here";
    empty.message = "This feed had no posts when it was last read.";
    return empty;
  }
  empty.headline = "Nothing yet";
  empty.message = "One refresh over Wi-Fi brings the posts of every feed. After that they read with no network.";
  empty.actionLabel = "Refresh";
  empty.action = ActionRefresh;
  return empty;
}

int buildFront(toybox::Screen& screen, const Ink& ink, const FrontModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  int y = masthead(screen, ink, model.title, model.right, ActionMenu);
  const int bottom = device.height - kBar;

  // Tapping it goes on a page, and from the last one back to the first.
  const bool paged = model.more || model.page > 1;
  char pageText[24];
  std::snprintf(pageText, sizeof(pageText), "Page %d", model.page);
  const bool noted = model.note != nullptr && model.note[0] != '\0';
  const Word words[] = {{"Feeds", ActionMenu},
                        {noted ? model.note : (model.canRefresh ? "Refresh" : nullptr),
                         noted ? fui::NO_ACTION : static_cast<fui::ActionId>(ActionRefresh)},
                        {paged ? pageText : nullptr, ActionNext}};
  foot(screen, ink, words, 3);

  if (model.count <= 0) {
    y = statement(ink, width, y + 36, model.emptyHeadline != nullptr ? model.emptyHeadline : "Nothing here",
                  model.emptyMessage);
    if (model.emptyActionLabel != nullptr && model.emptyAction != fui::NO_ACTION) {
      y += 24;
      const fui::TextStyle go = style(fui::FONT_SLOT_BODY, fui::TextAlign::Center, 1, fui::Color::White);
      const int lineH = ink.heads.lineHeight(go.font);
      hit(screen, rectOf(0, y - 8, device.width, lineH + 38), model.emptyAction);
      fill(ink.page, kSide, y, width, lineH + 22);
      ink.heads.text(rectOf(kSide, y + 11, width, lineH), model.emptyActionLabel, go);
    }
    return 0;
  }

  const int fits = frontFit(ink, device, model.rows, model.count);
  const int smallH = ink.page.lineHeight(fui::FONT_SLOT_SMALL);
  for (int n = 0; n < fits; ++n) {
    const Row& row = model.rows[n];
    const RowSize size = rowSize(ink, width, row);
    hit(screen, rectOf(0, y + 1, device.width, size.height - 1), ActionOpenPost, model.first + n);
    int textY = y + kRowPad;
    if (size.kicker > 0) {
      int x = kSide;
      if (row.unread) {
        fill(ink.page, kSide, textY + (smallH - kMark) / 2, kMark, kMark);
        x += kMark + kMarkGap;
      }
      ink.page.text(rectOf(x, textY, width - (x - kSide), smallH), hasKicker(row) ? row.kicker : "NEW",
                    style(fui::FONT_SLOT_SMALL));
      textY += size.kicker + kKickerGap;
    }
    ink.heads.text(rectOf(kSide, textY, width, size.title), row.title, headlineStyle(row.unread));
    y += size.height;
    if (y < bottom - 2) fill(ink.page, kSide, y, width, kHair);
  }
  return fits;
}

// --- The menu ------------------------------------------------------------

int menuRows(const Ink& ink, const fui::DeviceContext& device) {
  const int rows = (device.height - kBar - mastheadBottom(ink)) / kMenuRow;
  return rows < 1 ? 1 : (rows > kMaxRows ? kMaxRows : rows);
}

void buildMenu(toybox::Screen& screen, const Ink& ink, const MenuModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  int y = masthead(screen, ink, "RSS", model.right, fui::NO_ACTION);

  char pageText[24];
  std::snprintf(pageText, sizeof(pageText), "%d / %d", model.page, model.pages);
  // Adding and removing both happen on the phone, so one word says both.
  const Word words[] = {{"Add or remove", ActionAddFeeds},
                        {"Refresh", ActionRefresh},
                        {model.pages > 1 ? pageText : nullptr, ActionNext}};
  foot(screen, ink, words, 3);

  const int lineH = ink.heads.lineHeight(fui::FONT_SLOT_BODY);
  const int smallH = ink.heads.lineHeight(fui::FONT_SLOT_SMALL);
  const int fits = menuRows(ink, device);
  for (int i = 0; i < model.count && i < fits; ++i) {
    const MenuRow& row = model.rows[i];
    hit(screen, rectOf(0, y + 1, device.width, kMenuRow - 1), ActionView, row.view);
    const int textY = y + (kMenuRow - lineH) / 2;
    if (row.current) fill(ink.page, kSide, y + kMenuRow / 2 - kMark / 2, kMark, kMark);
    const int indent = row.current ? kMark + kMarkGap + 2 : 0;
    int rightW = 0;
    if (row.right != nullptr && row.right[0] != '\0') {
      const fui::TextStyle s = style(fui::FONT_SLOT_SMALL, fui::TextAlign::Right);
      rightW = ink.heads.measureText(s.font, row.right, s).width + 12;
      ink.heads.text(rectOf(kSide, y + (kMenuRow - smallH) / 2, width, smallH), row.right, s);
    }
    ink.heads.text(rectOf(kSide + indent, textY, width - indent - rightW, lineH), row.name, style(fui::FONT_SLOT_BODY));
    y += kMenuRow;
    fill(ink.page, kSide, y, width, kHair);
  }
}

// --- The notice ----------------------------------------------------------

void buildNotice(toybox::Screen& screen, const Ink& ink, const NoticeModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  const int y = masthead(screen, ink, "RSS", "", fui::NO_ACTION);
  if (model.actionLabel != nullptr && model.action != fui::NO_ACTION) {
    const Word words[] = {{model.actionLabel, model.action}};
    foot(screen, ink, words, 1);
  }
  statement(ink, width, y + 36, model.headline, model.message);
}

// --- The phone -----------------------------------------------------------

fui::Rect buildPhone(toybox::Screen& screen, const Ink& ink, const PhoneModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  int y = masthead(screen, ink, "Add feeds", "", fui::NO_ACTION);
  const Word words[] = {{"Done", ActionPhoneDone}};
  foot(screen, ink, words, 1);

  const fui::TextStyle line = style(fui::FONT_SLOT_BODY, fui::TextAlign::Center);
  const int lineH = ink.page.lineHeight(line.font);
  const int boldH = ink.heads.lineHeight(fui::FONT_SLOT_BODY);
  y += 22;
  ink.page.text(rectOf(kSide, y, width, lineH), "Scan this with your phone.", line);
  y += lineH + 20;

  // The code takes about half the panel, so the address under it -- the one
  // string somebody may have to type -- is not pushed into a corner.
  const int bottom = device.height - kBar;
  int side = bottom - y - 16 - boldH - 12 - 2 * lineH - 16;
  if (side > width) side = width;
  if (side > 300) side = 300;
  if (side < 120) side = 120;
  const fui::Rect qr = rectOf((device.width - side) / 2, y, side, side);

  y += side + 16;
  ink.heads.text(rectOf(kSide, y, width, boldH), model.readable, style(fui::FONT_SLOT_BODY, fui::TextAlign::Center));
  y += boldH + 12;
  // The state says whether anything has happened. Two lines, because it
  // carries a feed's own name and some names are long.
  ink.page.text(rectOf(kSide, y, width, 2 * lineH), model.status,
                style(fui::FONT_SLOT_BODY, fui::TextAlign::Center, 2));
  return qr;
}

}  // namespace rssui

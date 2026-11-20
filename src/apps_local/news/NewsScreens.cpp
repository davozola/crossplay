#include "NewsScreens.h"

#include <cstdio>

namespace newsui {
namespace {

constexpr int kSide = 18;      // the page's side margin
constexpr int kReadSide = 14;  // a story's: narrower, the text earns it
constexpr int kTop = 12;
constexpr int kBar = 44;  // the line of controls at the foot
constexpr int kHair = 1;
constexpr int kRule = 3;

fui::TextStyle style(const fui::FontId slot, const fui::TextAlign align = fui::TextAlign::Left,
                     const uint8_t maxLines = 1, const fui::Color color = fui::Color::Black) {
  fui::TextStyle s;
  s.font = slot;
  s.align = align;
  s.maxLines = maxLines;
  s.color = color;
  return s;
}

// The faces, by what they are for.
fui::TextStyle display(const fui::TextAlign align = fui::TextAlign::Left) { return style(fui::FONT_SLOT_TITLE, align); }
fui::TextStyle label(const fui::TextAlign align = fui::TextAlign::Left, const fui::Color color = fui::Color::Black) {
  return style(fui::FONT_SLOT_SMALL, align, 1, color);
}

fui::Rect rectOf(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h < 0 ? 0 : h));
}

void fill(fui::DrawTarget& target, const int x, const int y, const int w, const int h) {
  target.fill(rectOf(x, y, w, h), fui::Paint::solid(fui::Color::Black));
}

// A place that takes a tap and draws nothing: what is seen there is set over it.
void hit(toybox::Screen& screen, const fui::Rect rect, const fui::ActionId action) {
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

// The masthead: a name in the display serif, a date opposite it, and the
// double rule a paper puts under its title. Returns where the page starts.
int masthead(toybox::Screen& screen, const Ink& ink, const char* name, const char* right, const fui::ActionId action) {
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  const int nameH = ink.page.lineHeight(fui::FONT_SLOT_TITLE);
  const int smallH = ink.page.lineHeight(fui::FONT_SLOT_SMALL);
  if (action != fui::NO_ACTION) hit(screen, rectOf(0, 0, device.width, kTop + nameH + 8), action);
  ink.page.text(rectOf(kSide, kTop, width, nameH), name, display());
  if (right != nullptr && right[0] != '\0') {
    ink.page.text(rectOf(kSide, kTop + nameH - smallH - 6, width, smallH), right, label(fui::TextAlign::Right));
  }
  const int y = kTop + nameH + 2;
  fill(ink.page, kSide, y, width, kRule);
  fill(ink.page, kSide, y + kRule + 2, width, kHair);
  return y + kRule + 2 + kHair;
}

// The foot: up to three words, each a control, over a hairline.
struct Word {
  const char* text = nullptr;
  fui::ActionId action = fui::NO_ACTION;
  bool marked = false;  // set white on black: the state is on
};
void foot(toybox::Screen& screen, const Ink& ink, const Word* words, const int count) {
  const fui::DeviceContext& device = screen.device();
  const int y = device.height - kBar;
  const int each = device.width / (count > 0 ? count : 1);
  for (int i = 0; i < count; ++i) {
    if (words[i].action != fui::NO_ACTION) hit(screen, rectOf(i * each, y, each, kBar), words[i].action);
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
    fui::TextStyle s = style(fui::FONT_SLOT_SMALL, align);
    if (words[i].marked) {
      const int textW = ink.heads.measureText(s.font, words[i].text, s).width + 20;
      // The box stays inside the margin, so the word moves in by its padding.
      const int boxX = align == fui::TextAlign::Left    ? x
                       : align == fui::TextAlign::Right ? x + w - textW
                                                        : x + (w - textW) / 2;
      fill(ink.page, boxX, y + 8, textW, kBar - 15);
      s.color = fui::Color::White;
      s.align = fui::TextAlign::Center;
      ink.heads.text(rectOf(boxX, textY, textW, lineH), words[i].text, s);
      continue;
    }
    ink.heads.text(rectOf(x, textY, w, lineH), words[i].text, s);
  }
}

void emptyPage(toybox::Screen& screen, const Ink& ink, const int top, const char* headline, const char* message,
               const bool canFetch) {
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  const fui::TextStyle head = style(fui::FONT_SLOT_TITLE, fui::TextAlign::Left, 3);
  const fui::TextStyle body = style(fui::FONT_SLOT_BODY, fui::TextAlign::Left, 8);
  int y = top + 40;
  if (canFetch) hit(screen, rectOf(0, y - 10, device.width, 300), ActionRefresh);
  const int headH = wrappedHeight(ink.heads, headline, head, width);
  ink.heads.text(rectOf(kSide, y, width, headH), headline, head);
  y += headH + 14;
  const int bodyH = wrappedHeight(ink.page, message, body, width);
  ink.page.text(rectOf(kSide, y, width, bodyH), message, body);
  y += bodyH + 24;
  if (canFetch) {
    const fui::TextStyle go = style(fui::FONT_SLOT_BODY, fui::TextAlign::Center, 1, fui::Color::White);
    const int lineH = ink.heads.lineHeight(go.font);
    fill(ink.page, kSide, y, width, lineH + 22);
    ink.heads.text(rectOf(kSide, y + 11, width, lineH), "Get today's edition", go);
  }
}

constexpr int kRowPad = 13;

fui::TextStyle headlineStyle(const bool lead) {
  // Headlines are set whole: as many lines as they need, within reason.
  return lead ? style(fui::FONT_SLOT_TITLE, fui::TextAlign::Left, 5)
              : style(fui::FONT_SLOT_BODY, fui::TextAlign::Left, 4);
}
fui::TextStyle summaryStyle() { return style(fui::FONT_SLOT_SMALL, fui::TextAlign::Left, 4); }

struct RowSize {
  int title = 0;
  int summary = 0;
  int height = 0;
};
RowSize rowSize(const Ink& ink, const int width, const Headline& item, const bool lead) {
  RowSize size;
  size.title = wrappedHeight(ink.heads, item.title, headlineStyle(lead), width);
  const bool withSummary = lead && item.summary != nullptr && item.summary[0] != '\0';
  size.summary = withSummary ? wrappedHeight(ink.heads, item.summary, summaryStyle(), width) : 0;
  size.height = kRowPad + size.title + (withSummary ? 8 + size.summary : 0) + kRowPad;
  return size;
}

int frontTop(const Ink& ink) { return kTop + ink.page.lineHeight(fui::FONT_SLOT_TITLE) + 2 + kRule + 2 + kHair; }

}  // namespace

int frontFit(const Ink& ink, const fui::DeviceContext& device, const Headline* items, const int count, const int top) {
  const int width = device.width - 2 * kSide;
  const int bottom = device.height - kBar;
  int y = frontTop(ink);
  int shown = 0;
  for (int i = top; i < count && shown < kMaxRows; ++i) {
    const int height = rowSize(ink, width, items[i], i == 0).height;
    if (y + height > bottom && shown > 0) break;
    y += height;
    ++shown;
  }
  return shown;
}

int buildFront(toybox::Screen& screen, const Ink& ink, const FrontModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  int y = masthead(screen, ink, model.section, model.edition, ActionMenu);
  const int bottom = device.height - kBar;

  char pageText[24];
  std::snprintf(pageText, sizeof(pageText), "%d / %d", model.page, model.pages);
  const Word words[] = {{"Sections", ActionMenu, false},
                        {model.note, fui::NO_ACTION, false},
                        {model.pages > 1 ? pageText : nullptr, model.pages > 1 ? ActionNext : fui::NO_ACTION, false}};
  foot(screen, ink, words, 3);

  if (model.count <= 0) {
    emptyPage(screen, ink, y, model.emptyHeadline != nullptr ? model.emptyHeadline : "Nothing here yet",
              model.emptyMessage != nullptr ? model.emptyMessage : "", model.emptyCanFetch);
    return 0;
  }

  const int fits = frontFit(ink, device, model.items, model.count, model.top);
  for (int n = 0; n < fits; ++n) {
    const int i = model.top + n;
    const Headline& item = model.items[i];
    const bool lead = i == 0;
    const RowSize size = rowSize(ink, width, item, lead);
    hit(screen, rectOf(0, y + 1, device.width, size.height - 1), static_cast<fui::ActionId>(ActionOpen0 + i));
    ink.heads.text(rectOf(kSide, y + kRowPad, width, size.title), item.title, headlineStyle(lead));
    if (size.summary > 0) {
      ink.heads.text(rectOf(kSide, y + kRowPad + size.title + 8, width, size.summary), item.summary, summaryStyle());
    }
    y += size.height;
    if (y < bottom - 2) fill(ink.page, kSide, y, width, kHair);
  }
  return fits;
}

void buildMenu(toybox::Screen& screen, const Ink& ink, const MenuModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  int y = masthead(screen, ink, "News", model.edition, fui::NO_ACTION);

  const int creditH = ink.page.lineHeight(fui::FONT_SLOT_SMALL);
  const int bottom = device.height - creditH - 14;
  ink.page.text(rectOf(kSide, bottom + 4, width, creditH), "Stories by Kagi News. CC BY-NC 4.0. news.kagi.com",
                label(fui::TextAlign::Center));

  const int rows = model.sectionCount + 3;
  int rowH = (bottom - y - 10) / (rows > 0 ? rows : 1);
  if (rowH > 62) rowH = 62;
  const int lineH = ink.heads.lineHeight(fui::FONT_SLOT_BODY);
  const auto row = [&](const char* text, const char* right, const fui::ActionId action, const bool current,
                       const bool quiet) {
    hit(screen, rectOf(0, y + 1, device.width, rowH - 1), action);
    const int textY = y + (rowH - lineH) / 2;
    if (current) fill(ink.page, kSide, y + rowH / 2 - 4, 8, 8);
    const int indent = current ? 18 : 0;
    if (quiet) {
      ink.page.text(rectOf(kSide, textY, width, lineH), text, style(fui::FONT_SLOT_BODY));
    } else {
      ink.heads.text(rectOf(kSide + indent, textY, width - indent, lineH), text, style(fui::FONT_SLOT_BODY));
    }
    if (right != nullptr) {
      ink.heads.text(rectOf(kSide, textY + 3, width, lineH), right, style(fui::FONT_SLOT_SMALL, fui::TextAlign::Right));
    }
    y += rowH;
    fill(ink.page, kSide, y, width, kHair);
  };
  for (int i = 0; i < model.sectionCount; ++i) {
    row(model.sections[i], nullptr, static_cast<fui::ActionId>(ActionSection0 + i), i == model.current, false);
  }
  char count[16];
  std::snprintf(count, sizeof(count), "%d", model.favourites);
  row("Favourites", model.favourites > 0 ? count : nullptr, ActionFavourites, model.current < 0, false);
  row("Choose sections", nullptr, ActionChoose, false, true);
  row("Fetch today's edition", nullptr, ActionRefresh, false, true);
}

void buildChooser(toybox::Screen& screen, const Ink& ink, const ChooserModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  char chosen[24];
  std::snprintf(chosen, sizeof(chosen), "%d OF %d", model.chosenTotal, model.maxChosen);
  int y = masthead(screen, ink, "Sections", chosen, fui::NO_ACTION);

  char pageText[24];
  std::snprintf(pageText, sizeof(pageText), "%d / %d", model.page, model.pages);
  const Word words[] = {{"Done", ActionDone, false},
                        {model.pages > 1 ? "Previous" : nullptr, model.pages > 1 ? ActionPrev : fui::NO_ACTION, false},
                        {model.pages > 1 ? pageText : nullptr, model.pages > 1 ? ActionNext : fui::NO_ACTION, false}};
  foot(screen, ink, words, 3);

  const int bottom = device.height - kBar;
  const int rowH = (bottom - y) / kChooserRows;
  const int lineH = ink.page.lineHeight(fui::FONT_SLOT_BODY);
  const int boldH = ink.heads.lineHeight(fui::FONT_SLOT_BODY);
  for (int i = 0; i < model.count && i < kChooserRows; ++i) {
    hit(screen, rectOf(0, y + 1, device.width, rowH - 1), static_cast<fui::ActionId>(ActionToggle0 + i));
    // A chosen section is set bold with a filled square; the rest are plain.
    constexpr int kBox = 14;
    const int boxY = y + (rowH - kBox) / 2;
    fill(ink.page, kSide, boxY, kBox, kBox);
    if (!model.chosen[i]) {
      ink.page.fill(rectOf(kSide + 2, boxY + 2, kBox - 4, kBox - 4), fui::Paint::solid(fui::Color::White));
      ink.page.text(rectOf(kSide + kBox + 12, y + (rowH - lineH) / 2, width - kBox - 12, lineH), model.names[i],
                    style(fui::FONT_SLOT_BODY));
    } else {
      ink.heads.text(rectOf(kSide + kBox + 12, y + (rowH - boldH) / 2, width - kBox - 12, boldH), model.names[i],
                     style(fui::FONT_SLOT_BODY));
    }
    y += rowH;
    fill(ink.page, kSide, y, width, kHair);
  }
}

StoryMetrics storyMetrics(const Ink& ink, const fui::DeviceContext& device, const char* title) {
  StoryMetrics m;
  const int width = device.width - 2 * kReadSide;
  const int stripH = ink.page.lineHeight(fui::FONT_SLOT_SMALL) + 12;
  const int top = stripH + 8;
  const int bottom = device.height - kBar - 4;
  m.body = rectOf(kReadSide, top, width, bottom - top);
  const fui::TextStyle head = style(fui::FONT_SLOT_TITLE, fui::TextAlign::Left, 6);
  const int headH = wrappedHeight(ink.heads, title, head, width);
  // The headline, then a short rule, then the text.
  const int firstTop = top + 6 + headH + 14 + kRule + 14;
  m.firstBody = rectOf(kReadSide, firstTop, width, bottom - firstTop);
  const int lineH = ink.page.lineHeight(fui::FONT_SLOT_BODY);
  const int rest = lineH > 0 ? m.body.height / lineH : 1;
  const int first = lineH > 0 ? m.firstBody.height / lineH : 1;
  m.restLines = static_cast<uint16_t>(rest < 1 ? 1 : rest);
  m.firstLines = static_cast<uint16_t>(first < 1 ? 1 : first);
  // Whole lines only, so a page never ends on half of one.
  m.body.height = static_cast<int16_t>(m.restLines * lineH);
  m.firstBody.height = static_cast<int16_t>(m.firstLines * lineH);
  return m;
}

void buildStory(toybox::Screen& screen, const Ink& ink, const StoryModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kReadSide;

  const Word words[] = {{"Back", ActionBack, false},
                        {"Aa", ActionTextSize, false},
                        {model.favourite ? "Saved" : "Save", ActionFavourite, model.favourite}};
  foot(screen, ink, words, 3);

  // One quiet line at the head: where this is, and how far through.
  const int smallH = ink.page.lineHeight(fui::FONT_SLOT_SMALL);
  ink.page.text(rectOf(kReadSide, 6, width, smallH), model.section, label());
  ink.page.text(rectOf(kReadSide, 6, width, smallH), model.pageLabel, label(fui::TextAlign::Right));
  fill(ink.page, kReadSide, smallH + 11, width, kHair);

  const StoryMetrics m = storyMetrics(ink, device, model.title);
  if (model.firstPage) {
    const fui::TextStyle head = style(fui::FONT_SLOT_TITLE, fui::TextAlign::Left, 6);
    const int headH = wrappedHeight(ink.heads, model.title, head, width);
    ink.heads.text(rectOf(kReadSide, m.body.y + 6, width, headH), model.title, head);
    fill(ink.page, kReadSide, m.body.y + 6 + headH + 14, 56, kRule);
  }
  if (model.wrap == nullptr) return;
  model.wrap->draw(ink.page, model.firstPage ? m.firstBody : m.body, model.body,
                   style(fui::FONT_SLOT_BODY, fui::TextAlign::Left, 255), model.topLine);
}

void buildNotice(toybox::Screen& screen, const Ink& ink, const NoticeModel& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.device();
  const int width = device.width - 2 * kSide;
  int y = masthead(screen, ink, "News", "", fui::NO_ACTION);
  if (model.backButton) {
    const Word words[] = {{"Back", ActionBack, false}};
    foot(screen, ink, words, 1);
  }
  y += 36;
  const fui::TextStyle head = style(fui::FONT_SLOT_TITLE, fui::TextAlign::Left, 4);
  const int headH = wrappedHeight(ink.heads, model.headline, head, width);
  ink.heads.text(rectOf(kSide, y, width, headH), model.headline, head);
  y += headH + 14;
  fill(ink.page, kSide, y, 56, kRule);
  y += kRule + 16;
  if (model.message != nullptr && model.message[0] != '\0') {
    const fui::TextStyle body = style(fui::FONT_SLOT_BODY, fui::TextAlign::Left, 14);
    const int bodyH = wrappedHeight(ink.page, model.message, body, width);
    ink.page.text(rectOf(kSide, y, width, bodyH), model.message, body);
  }
}

}  // namespace newsui

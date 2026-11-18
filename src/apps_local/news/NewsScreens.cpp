#include "NewsScreens.h"

#include "../ui/ToyboxText.h"

namespace newsui {
namespace {

constexpr int kBodyTop = toybox::kBodyTop;
constexpr int kFooterHeight = toybox::kPillHeight;

void chrome(toybox::Screen& screen, const char* title, const char* rightLabel, const bool refresh) {
  fui::HeaderProps header;
  header.title = title;
  if (refresh) {
    header.trailingLabel = "REFRESH";
    header.trailingAction = ActionRefresh;
    header.trailingStyles = toybox::bandOutlineStyles();
    header.trailingRadius = toybox::kPillRadius / 2;
  } else if (rightLabel != nullptr) {
    header.rightLabel = rightLabel;
    header.subtitleText = screen.theme().smallText;
    header.subtitleText.color = fui::Color::White;
    header.subtitleText.align = fui::TextAlign::Right;
  }
  header.borderEdges = fui::EdgesNone;
  fui::TextStyle bandTitle = screen.theme().titleText;
  bandTitle.align = screen.theme().headerTitleAlign;
  bandTitle.maxLines = 1;
  header.titleText = bandTitle;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

fui::TextStyle kickerStyle(const fui::ThemeTokens& tokens) {
  fui::TextStyle kicker = tokens.smallText;
  kicker.font = toybox::kTileFont;
  kicker.maxLines = 1;
  return kicker;
}

int16_t leadHeight(const fui::DrawTarget& target, const fui::ThemeTokens& tokens) {
  return static_cast<int16_t>(target.lineHeight(kickerStyle(tokens).font) + 2 * target.lineHeight(tokens.titleText.font) +
                              2 * target.lineHeight(tokens.smallText.font) + 2 * toybox::kGutter);
}

}  // namespace

fui::TextStyle listTitleStyle(const fui::ThemeTokens& tokens) {
  fui::TextStyle title = tokens.smallText;
  title.maxLines = 2;
  return title;
}

int16_t listRowHeight(const fui::DrawTarget& target, const fui::ThemeTokens& tokens) {
  return static_cast<int16_t>(2 * target.lineHeight(tokens.smallText.font) +
                              target.lineHeight(kickerStyle(tokens).font) + toybox::kGutter);
}

fui::Rect listBand(const fui::DeviceContext& device, const bool withLead, const fui::DrawTarget& target,
                   const fui::ThemeTokens& tokens) {
  const int bottom = toybox::kMargin + kFooterHeight + toybox::kGutter;
  const int top = kBodyTop + (withLead ? leadHeight(target, tokens) + toybox::kGutter : 0);
  return fui::makeRect(toybox::kMargin, static_cast<int16_t>(top),
                       static_cast<int16_t>(device.width - 2 * toybox::kMargin),
                       static_cast<int16_t>(device.height - bottom - top));
}

int16_t listTitleWidth(const fui::DeviceContext& device, const fui::ThemeTokens& tokens) {
  const int16_t band = static_cast<int16_t>(device.width - 2 * toybox::kMargin);
  return static_cast<int16_t>(band - 2 * tokens.listSidePadding);
}

void buildList(toybox::Screen& screen, const ListModel& model) {
  chrome(screen, model.title, nullptr, true);

  // The footer: the sections, the one being read inverted.
  {
    const fui::Rect strip = screen.takeBottom(kFooterHeight, toybox::kGutter);
    const int count = model.tabCount > 0 ? model.tabCount : 1;
    const int16_t each = static_cast<int16_t>((strip.width - (count - 1) * toybox::kGutter) / count);
    for (int i = 0; i < model.tabCount; ++i) {
      const bool here = i == model.tab;
      fui::ButtonProps button;
      button.label = model.tabs[i];
      button.action = here ? fui::NO_ACTION : static_cast<fui::ActionId>(ActionTab0 + i);
      button.styles = here ? toybox::invertedStyles() : toybox::rowStyles();
      button.text = kickerStyle(screen.theme());
      button.text.align = fui::TextAlign::Center;
      button.text.color = here ? fui::Color::White : fui::Color::Black;
      screen.button(button, fui::makeRect(static_cast<int16_t>(strip.x + i * (each + toybox::kGutter)), strip.y, each,
                                          kFooterHeight));
    }
  }

  if (model.count <= 0 && model.leadTitle == nullptr) {
    const fui::Rect body = screen.body();
    fui::TextStyle headline = screen.theme().titleText;
    headline.color = fui::Color::Black;
    headline.align = fui::TextAlign::Center;
    headline.maxLines = 2;
    const char* head = model.emptyHeadline != nullptr ? model.emptyHeadline : "NOTHING HERE YET";
    const int16_t headH = fui::measureWrappedText(screen.target(), head, headline, body.width).height;
    fui::TextStyle message = screen.theme().smallText;
    message.align = fui::TextAlign::Center;
    message.maxLines = 4;
    const char* text = model.emptyMessage != nullptr ? model.emptyMessage : "";
    const int16_t textH = fui::measureWrappedText(screen.target(), text, message, body.width).height;
    const int16_t buttonH = static_cast<int16_t>(kFooterHeight + 2 * toybox::kGutter);
    int16_t y = static_cast<int16_t>(body.y + (body.height - headH - textH - buttonH - toybox::kGutter) / 2);
    screen.target().text(fui::makeRect(body.x, y, body.width, headH), head, headline);
    y = static_cast<int16_t>(y + headH + toybox::kGutter);
    screen.target().text(fui::makeRect(body.x, y, body.width, textH), text, message);
    y = static_cast<int16_t>(y + textH + 2 * toybox::kGutter);
    fui::ButtonProps button;
    button.label = "FETCH THE HEADLINES";
    button.action = ActionRefresh;
    button.styles = toybox::invertedStyles();
    button.radius = static_cast<uint8_t>(toybox::kPillRadius);
    const int16_t width = static_cast<int16_t>(body.width * 3 / 4);
    screen.button(button, fui::makeRect(static_cast<int16_t>(body.x + (body.width - width) / 2), y, width,
                                        static_cast<int16_t>(kFooterHeight)));
    return;
  }

  const bool withLead = model.leadTitle != nullptr;
  if (withLead) {
    const fui::DeviceContext& device = screen.device();
    const fui::ThemeTokens& tokens = screen.theme();
    const int16_t width = static_cast<int16_t>(device.width - 2 * toybox::kMargin);
    const int16_t height = leadHeight(screen.target(), tokens);
    int16_t y = kBodyTop;
    // The whole block opens the story.
    fui::ButtonProps hit;
    hit.label = "";
    hit.action = ActionOpenLead;
    hit.styles = toybox::rowStyles();
    screen.button(hit, fui::makeRect(toybox::kMargin, y, width, height));

    const int16_t x = static_cast<int16_t>(toybox::kMargin + tokens.listSidePadding);
    const int16_t inner = static_cast<int16_t>(width - 2 * tokens.listSidePadding);
    y = static_cast<int16_t>(y + toybox::kGutter / 2);
    const fui::TextStyle kicker = kickerStyle(tokens);
    const int16_t kickerH = screen.target().lineHeight(kicker.font);
    if (model.leadKicker != nullptr) {
      screen.target().text(fui::makeRect(x, y, inner, kickerH), model.leadKicker, kicker);
    }
    y = static_cast<int16_t>(y + kickerH + toybox::kGutter / 2);
    fui::TextStyle title = tokens.titleText;
    title.color = fui::Color::Black;
    title.align = fui::TextAlign::Left;
    title.maxLines = 2;
    const int16_t titleH = static_cast<int16_t>(2 * screen.target().lineHeight(title.font));
    screen.target().text(fui::makeRect(x, y, inner, titleH), model.leadTitle, title);
    y = static_cast<int16_t>(y + titleH + toybox::kGutter / 2);
    if (model.leadSummary != nullptr) {
      fui::TextStyle summary = tokens.smallText;
      summary.maxLines = 2;
      const int16_t summaryH = static_cast<int16_t>(2 * screen.target().lineHeight(summary.font));
      screen.target().text(fui::makeRect(x, y, inner, summaryH), model.leadSummary, summary);
    }
    screen.insetContent(fui::Insets{static_cast<int16_t>(height + toybox::kGutter), 0, 0, 0});
  }

  if (model.count <= 0) return;
  fui::ListProps list;
  list.items = model.items;
  list.count = static_cast<uint16_t>(model.count);
  list.topIndex = static_cast<uint16_t>(model.topIndex);
  list.selectedIndex = -1;
  list.action = ActionOpen;
  list.rowHeight = listRowHeight(screen.target(), screen.theme());
  list.labelText = listTitleStyle(screen.theme());
  list.subtitleText = kickerStyle(screen.theme());
  list.valueText = kickerStyle(screen.theme());
  list.valueText.align = fui::TextAlign::Right;
  list.balanceWrappedLabelWithValue = false;
  screen.list(list);
}

fui::Rect readerBody(const fui::DeviceContext& device) {
  const int bottom = toybox::kMargin + kFooterHeight + toybox::kGutter;
  return fui::makeRect(toybox::kMargin, kBodyTop, static_cast<int16_t>(device.width - 2 * toybox::kMargin),
                       static_cast<int16_t>(device.height - bottom - kBodyTop));
}

uint32_t buildReader(toybox::Screen& screen, const ReaderModel& model) {
  chrome(screen, model.title, model.pageLabel, false);
  const fui::DeviceContext& device = screen.device();
  const int16_t footerY = static_cast<int16_t>(device.height - toybox::kMargin - kFooterHeight);
  const int16_t usable = static_cast<int16_t>(device.width - 2 * toybox::kMargin);
  const int16_t arrow = static_cast<int16_t>((usable - 2 * toybox::kGutter) / 4);
  const int16_t middle = static_cast<int16_t>(usable - 2 * arrow - 2 * toybox::kGutter);
  const auto footerButton = [&screen, footerY](const char* label, const fui::ActionId action, const int16_t x,
                                               const int16_t width, const bool enabled) {
    fui::ButtonProps button;
    button.label = label;
    button.action = enabled ? action : fui::NO_ACTION;
    if (!enabled) button.styles = toybox::disabledButtonStyles();
    screen.button(button, fui::makeRect(x, footerY, width, kFooterHeight));
  };
  const int16_t left = toybox::kMargin;
  footerButton("<", ActionPagePrev, left, arrow, model.canPagePrev);
  footerButton("HEADLINES", ActionBack, static_cast<int16_t>(left + arrow + toybox::kGutter), middle, true);
  footerButton(">", ActionPageNext, static_cast<int16_t>(left + arrow + middle + 2 * toybox::kGutter), arrow,
               model.canPageNext);
  if (model.wrap == nullptr) return 0;
  const fui::TextStyle style = screen.theme().bodyText;
  model.wrap->draw(screen.target(), readerBody(device), model.text, style, model.topLine);
  return model.wrap->lineCount(screen.target(), readerBody(device).width, model.text, style);
}

void buildNotice(toybox::Screen& screen, const NoticeModel& model) {
  chrome(screen, "NEWS", nullptr, false);
  const fui::DeviceContext& device = screen.device();
  const int16_t width = static_cast<int16_t>(device.width - 2 * toybox::kMargin);
  if (model.backButton) {
    fui::ButtonProps action;
    action.label = "BACK TO THE HEADLINES";
    action.action = ActionBack;
    screen.button(action, fui::makeRect(toybox::kMargin,
                                        static_cast<int16_t>(device.height - toybox::kMargin - toybox::kPillHeight),
                                        width, toybox::kPillHeight));
  }
  int16_t y = kBodyTop;
  fui::TextStyle headline = screen.theme().titleText;
  headline.color = fui::Color::Black;
  headline.align = fui::TextAlign::Left;
  headline.maxLines = 2;
  const int16_t headlineHeight = static_cast<int16_t>(2 * screen.target().lineHeight(headline.font));
  screen.target().text(fui::makeRect(toybox::kMargin, y, width, headlineHeight), model.headline, headline);
  y = static_cast<int16_t>(y + headlineHeight + toybox::kGutter);
  screen.target().fill(fui::makeRect(toybox::kMargin, y, width, toybox::kRule), fui::Paint::solid(fui::Color::Black));
  y = static_cast<int16_t>(y + toybox::kRule + toybox::kGutter * 2);
  if (model.message != nullptr && model.message[0] != '\0') {
    const int16_t reserved = model.backButton ? static_cast<int16_t>(toybox::kPillHeight + toybox::kGutter) : 0;
    const int16_t bottom = static_cast<int16_t>(device.height - toybox::kMargin - reserved);
    fui::TextAreaProps message;
    message.text = model.message;
    message.showCaret = false;
    message.style = screen.theme().bodyText;
    fui::textArea(screen.frame(), fui::makeRect(toybox::kMargin, y, width, static_cast<int16_t>(bottom - y)), message);
  }
}

}  // namespace newsui

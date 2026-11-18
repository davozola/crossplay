#pragma once

// What NEWS draws: the headlines of one section, an article, and a notice.

#include "../ui/ToyboxScreen.h"
#include "../ui/ToyboxWrappedText.h"

namespace newsui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionOpen = 340,
  ActionPagePrev = 341,
  ActionPageNext = 342,
  ActionRefresh = 344,
  ActionBack = 345,
  ActionOpenLead = 346,
  ActionFavourite = 347,
  ActionTextSize = 348,
  ActionTab0 = 350,  // one per section, in order
};

constexpr int kMaxTabs = 6;

struct ListModel {
  const char* title = "NEWS";
  const char* updated = nullptr;  // "UPDATED 2h AGO", or nullptr
  const char* const* tabs = nullptr;
  int tabCount = 0;
  int tab = 0;
  const char* emptyHeadline = nullptr;
  const char* emptyMessage = nullptr;
  // Whether this list is one that can be fetched: the REFRESH chip, and the
  // button on its empty state.
  bool canRefresh = true;
  const fui::ListItem* items = nullptr;
  int count = 0;
  int topIndex = 0;
  // The lead: the newest item drawn large, on the first page only.
  const char* leadKicker = nullptr;
  const char* leadTitle = nullptr;
  const char* leadSummary = nullptr;
};

void buildList(toybox::Screen& screen, const ListModel& model);
fui::Rect listBand(const fui::DeviceContext& device, bool withLead, const fui::DrawTarget& target,
                   const fui::ThemeTokens& tokens);
fui::TextStyle listTitleStyle(const fui::ThemeTokens& tokens);
int16_t listRowHeight(const fui::DrawTarget& target, const fui::ThemeTokens& tokens);
int16_t listTitleWidth(const fui::DeviceContext& device, const fui::ThemeTokens& tokens);

struct ReaderModel {
  const char* title = "";
  const char* text = "";
  toybox::WrappedText* wrap = nullptr;
  uint32_t topLine = 0;
  const char* pageLabel = "";
  bool canPagePrev = false;
  bool canPageNext = false;
  bool favourite = false;
};
uint32_t buildReader(toybox::Screen& screen, const ReaderModel& model);
fui::Rect readerBody(const fui::DeviceContext& device);

struct NoticeModel {
  const char* headline = "";
  const char* message = "";
  bool backButton = false;
};
void buildNotice(toybox::Screen& screen, const NoticeModel& model);

}  // namespace newsui

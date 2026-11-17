#include "HistoryStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <string>

#include "../../network/HttpDownloader.h"

namespace hs = history;

namespace historystore {

namespace {

constexpr char kDir[] = "/.crosspoint/efemerides";
constexpr char kStagedPath[] = "/.crosspoint/efemerides/answer.tmp";
constexpr char kAgent[] = "crossplay-efemerides/1.0 (e-ink reader; https://github.com/davozola/crossplay)";

// One feed, fetched to the card and read from it a piece at a time: the
// answer for a full day's events runs to megabytes.
bool fetchInto(const char* type, const int month, const int day, std::vector<hs::Event>& events) {
  char url[120];
  hs::feedUrl(type, month, day, url, sizeof(url));
  const std::vector<HttpDownloader::Header> headers = {{"User-Agent", kAgent}, {"Accept", "application/json"}};
  if (HttpDownloader::downloadToFile(url, kStagedPath, nullptr, nullptr, "", "", headers) != HttpDownloader::OK) {
    LOG_ERR("EFEM", "fetch failed, status %d: %s", HttpDownloader::lastStatus(), url);
    Storage.remove(kStagedPath);
    return false;
  }
  const size_t before = events.size();
  {
    HalFile file;
    if (!Storage.openFileForRead("EFEM", kStagedPath, file)) return false;
    auto buffer = makeUniqueNoThrow<char[]>(2048);
    if (!buffer) {
      LOG_ERR("EFEM", "OOM: 2048 bytes");
      return false;
    }
    hs::Scanner scanner(events);
    while (true) {
      const int n = file.read(reinterpret_cast<uint8_t*>(buffer.get()), 2048);
      if (n <= 0) break;
      scanner.feed(buffer.get(), static_cast<size_t>(n));
    }
  }
  Storage.remove(kStagedPath);
  return events.size() > before;
}

}  // namespace

void dayPath(const int month, const int day, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%02d-%02d.txt", kDir, month % 100, day % 100);
}

bool fetchDay(const int month, const int day, std::vector<hs::Event>& out) {
  Storage.ensureDirectoryExists(kDir);
  out.clear();
  out.reserve(200);
  // The feed's own picks first, so they are the ones kept when both list an event.
  fetchInto("selected", month, day, out);
  fetchInto("events", month, day, out);
  if (out.empty()) return false;
  hs::dropRepeats(out);
  hs::sortEvents(out);
  char path[64];
  dayPath(month, day, path, sizeof(path));
  const std::string text = hs::pack(out);
  HalFile file;
  if (Storage.openFileForWrite("EFEM", path, file)) {
    file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    file.flush();
  }
  return true;
}

}  // namespace historystore

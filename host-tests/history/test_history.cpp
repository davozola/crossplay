#include <cstdio>
#include <cstring>
#include <string>

#include "HistoryCore.h"

using namespace history;

namespace {
int failures = 0;
int checks = 0;
void check(const bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL: %s\n", what);
  }
}

// The feed's shape: pages nested under each event, with texts and years of
// their own that must not be taken for the event's.
const char* kFeed =
    "{\"selected\":[{\"text\":\"Se lanza la sonda Mariner 9, la primera en orbitar otro planeta.\","
    "\"pages\":[{\"type\":\"standard\",\"title\":\"Mariner_9\",\"extract\":\"Una sonda \\\"espacial\\\" {de} la NASA\","
    "\"year\":1234,\"text\":\"no\",\"thumbnail\":{\"source\":\"https://x/y.jpg\",\"width\":320},"
    "\"coordinates\":[1.5,-2]}],\"year\":1971}],"
    "\"events\":[{\"text\":\"Se lanza la sonda Mariner 9, la primera en orbitar otro "
    "planeta.\",\"pages\":[],\"year\":1971},"
    "{\"year\":1558,\"text\":\"Isabel I sube al trono de Inglaterra tras la muerte de Mar\\u00eda "
    "I.\",\"pages\":[{\"a\":[[]]}]},"
    "{\"text\":\"Se estrena en Par\\u00eds la \\u00f3pera Mignon, de Ambroise Thomas. "
    "\\ud83c\\udfb5\",\"pages\":[],\"year\":1866},"
    "{\"text\":\"Tito Livio registra una batalla en la Galia.\",\"pages\":[],\"year\":-52},"
    "{\"text\":\"corto\",\"pages\":[],\"year\":1900},"
    "{\"text\":\"Un hecho sin a\\u00f1o que no deber\\u00eda entrar en la lista.\",\"pages\":[]}"
    "]}";
}  // namespace

int main() {
  {  // the feed, whole and in crumbs
    for (const size_t chunk : {static_cast<size_t>(100000), static_cast<size_t>(7), static_cast<size_t>(1)}) {
      std::vector<Event> events;
      Scanner scanner(events);
      const size_t len = std::strlen(kFeed);
      for (size_t at = 0; at < len; at += chunk) scanner.feed(kFeed + at, at + chunk <= len ? chunk : len - at);
      check(scanner.seen() == 7, "seven objects at event level");
      check(events.size() == 5, "five of them are events with a text and a year");
      if (events.size() != 5) continue;
      check(events[0].year == 1971 && events[0].selected && events[0].kind == Kind::Science &&
                events[0].text == "Se lanza la sonda Mariner 9, la primera en orbitar otro planeta.",
            "the pick, with its own year and not its page's");
      check(!events[1].selected && events[1].year == 1971, "the same event from the long list");
      check(events[2].year == 1558 && events[2].kind == Kind::History &&
                events[2].text ==
                    "Isabel I sube al trono de Inglaterra tras la muerte de Mar\xC3\xAD"
                    "a I.",
            "year before text, an escaped letter");
      check(events[3].year == 1866 && events[3].kind == Kind::Culture &&
                events[3].text == "Se estrena en Par\xC3\xADs la \xC3\xB3pera Mignon, de Ambroise Thomas.",
            events[3].text.c_str());
      check(events[4].year == -52, "before the common era");

      dropRepeats(events);
      check(events.size() == 4 && events[0].selected, "the repeat is dropped and the pick kept");
      sortEvents(events);
      check(events[0].year == 1971 && events[1].year == 1866 && events[3].year == -52, "newest first");

      const std::string packed = pack(events);
      std::vector<Event> back;
      unpack(packed, back);
      check(back.size() == 4 && back[0].selected && back[0].kind == Kind::Science && back[3].year == -52 &&
                back[1].text == events[1].text,
            "a day's file round trips");
      check(featured(back, Kind::Science, 11, 17) == 0 && featured(back, Kind::Culture, 11, 17) == 1, "what leads");
      check(countOf(back, Kind::History) == 2 && featured(back, Kind::History, 11, 17) >= 2, "history has two");
    }
    std::vector<Event> none;
    Scanner broken(none);
    const char* junk = "<html><body>503</body></html>";
    broken.feed(junk, std::strlen(junk));
    check(none.empty(), "an error page holds no events");
    Scanner capped(none, 2);
    capped.feed(kFeed, std::strlen(kFeed));
    check(none.size() == 2, "no more than the room given");
  }

  {  // sorting by words
    check(classify("Marie Curie recibe el Premio Nobel de Qu\xC3\xADmica.") == Kind::Science, "science before prizes");
    check(classify("Se publica la primera edici\xC3\xB3n de Don Quijote.") == Kind::Culture, "a book");
    check(classify("FIRMA DEL TRATADO DE PAZ ENTRE LOS DOS REINOS.") == Kind::History, "a treaty");
    check(classify("NACE UNA NUEVA TEOR\xC3\x8D"
                   "A sobre el origen.") == Kind::Science,
          "capitals with accents");
    check(std::strcmp(kindName(Kind::Culture), "CULTURA") == 0, "names");
  }

  {  // days
    char text[96];
    feedUrl("events", 11, 7, text, sizeof(text));
    check(std::strcmp(text, "https://es.wikipedia.org/api/rest_v1/feed/onthisday/events/11/07") == 0, text);
    dayLabel(11, 17, text, sizeof(text));
    check(std::strcmp(text, "17 DE NOVIEMBRE") == 0, text);
    int m = 1, d = 1;
    stepDay(m, d, -1);
    check(m == 12 && d == 31, "back over the new year");
    stepDay(m, d, 1);
    check(m == 1 && d == 1, "and forward");
    m = 2;
    d = 28;
    stepDay(m, d, 1);
    check(m == 2 && d == 29, "the 29th of February is a day here");
    stepDay(m, d, 1);
    check(m == 3 && d == 1, "then March");
    yearLabel(1969, text, sizeof(text));
    check(std::strcmp(text, "1969") == 0, text);
    yearLabel(-44, text, sizeof(text));
    check(std::strcmp(text, "44 A. C.") == 0, text);
  }

  if (failures == 0) {
    std::printf("history: all tests passed (%d checks)\n", checks);
    return 0;
  }
  return 1;
}

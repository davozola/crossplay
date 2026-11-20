// WIKIPEDIA's network side, off the device.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "WikiOnlineCore.h"

namespace {

int failures = 0;

void check(const bool ok, const char* what) {
  if (!ok) {
    ++failures;
    std::printf("FAIL %s\n", what);
  }
}

void same(const std::string& got, const std::string& want, const char* what) {
  if (got != want) {
    ++failures;
    std::printf("FAIL %s\n  got:  %s\n  want: %s\n", what, got.c_str(), want.c_str());
  }
}

struct StringSink final : wikies::Sink {
  std::string text;
  bool write(const char* data, const size_t len) override {
    text.append(data, len);
    return true;
  }
};

struct Result {
  std::string xhtml;
  std::string title;
  uint32_t id = 0;
  bool ok = false;
  bool missing = false;
  std::vector<std::string> headings;
};

Result convert(const std::string& json, const size_t step) {
  StringSink sink;
  wikies::ExtractReader reader(sink);
  for (size_t at = 0; at < json.size(); at += step) reader.feed(json.data() + at, std::min(step, json.size() - at));
  Result r;
  r.ok = reader.finish();
  r.xhtml = sink.text;
  r.title = reader.title();
  r.id = reader.pageId();
  r.missing = reader.missing();
  r.headings = reader.headings();
  return r;
}

// Every tag closed in order, nothing but the entities the writer makes.
bool wellFormed(const std::string& x) {
  std::vector<std::string> open;
  size_t i = 0;
  while (i < x.size()) {
    if (x[i] == '&') {
      const size_t semi = x.find(';', i);
      if (semi == std::string::npos) return false;
      const std::string e = x.substr(i, semi - i + 1);
      if (e != "&amp;" && e != "&lt;" && e != "&gt;") return false;
      i = semi + 1;
    } else if (x[i] == '<') {
      const size_t end = x.find('>', i);
      if (end == std::string::npos) return false;
      std::string tag = x.substr(i + 1, end - i - 1);
      i = end + 1;
      if (!tag.empty() && tag.back() == '/') continue;
      if (!tag.empty() && tag[0] == '/') {
        if (open.empty() || open.back() != tag.substr(1)) return false;
        open.pop_back();
      } else {
        open.push_back(tag.substr(0, tag.find(' ')));
      }
    } else {
      if (x[i] == '>') return false;
      ++i;
    }
  }
  return open.empty();
}

std::string answer(const std::string& extract) {
  return "{\"batchcomplete\":true,\"warnings\":{\"extracts\":{\"warnings\":\"HTML may be malformed\"}},"
         "\"query\":{\"redirects\":[{\"from\":\"Bcn\",\"to\":\"Barcelona\"}],\"pages\":[{\"pageid\":3175,"
         "\"ns\":0,\"title\":\"Barcelona\",\"extract\":\"" +
         extract + "\"}]}}";
}

void testArticle() {
  const std::string extract =
      "<link rel=\\\"mw-deduplicated-inline-style\\\" href=\\\"mw-data:TemplateStyles:r1\\\">"
      "<p class=\\\"mw-empty-elt\\\">\\n</p>\\n"
      "<p><b>Barcelona</b> es una ciudad espa\\u00f1ola, capital de <i>Catalu\\u00f1a</i> &amp; sede "
      "de 1&#160;620&nbsp;343 hab. a&lt;b R&D.</p>\\n\\n"
      "<h2><span id=\\\"Toponimia\\\">Toponimia</span></h2>\\n<p>El nombre\\nviene de <span>Barcino</span>.</p>"
      "<h2 data-mw-anchor=\\\"Historia\\\">Historia</h2>\\n"
      "<h3 data-mw-anchor=\\\"Prehistoria\\\">Prehistoria</h3>\\n<p>Restos.</p>"
      "<h3>Vac\\u00edo</h3>\\n"
      "<h3>Edad Media</h3><ul><li>Uno<ul><li>Dentro</li></ul></li>\\n<li>Dos <b>negrita sin cerrar</li></ul>"
      "<dl><dt>Dato</dt><dd>Valor</dd></dl>"
      "<h2>Sin nada</h2>\\n<p></p>"
      "<h2>V\\u00e9ase tambi\\u00e9n</h2><ul><li>Madrid</li></ul>"
      "<h2>Referencias</h2><p>No debe salir.</p><h3>Sub</h3><p>Tampoco.</p>"
      "<h2>Enlaces externos</h2><ul><li>Web oficial</li></ul>"
      "<h2>Emoji \\ud83d\\ude00</h2><p>fin<br>l\\u00ednea</p><!-- nota -->suelto";
  const std::string json = answer(extract);
  const Result whole = convert(json, json.size());
  check(whole.ok, "article converts");
  check(wellFormed(whole.xhtml), "article is well-formed");
  same(whole.title, "Barcelona", "title");
  check(whole.id == 3175, "page id");
  check(!whole.missing, "not missing");
  same(whole.xhtml,
       "<html><body><p><b>Barcelona</b> es una ciudad espa\xC3\xB1ola, capital de <i>Catalu\xC3\xB1"
       "a</i> &amp; sede de 1\xC2\xA0"
       "620\xC2\xA0"
       "343 hab. a&lt;b R&amp;D.</p>"
       "<h2 id=\"s1\">Toponimia</h2><p>El nombre viene de Barcino.</p>"
       "<h2 id=\"s2\">Historia</h2><h3>Prehistoria</h3><p>Restos.</p>"
       "<h3>Edad Media</h3><ul><li>Uno<ul><li>Dentro</li></ul></li><li>Dos <b>negrita sin cerrar</b></li></ul>"
       "<p><b>Dato</b></p><p>Valor</p>"
       "<h2 id=\"s3\">V\xC3\xA9"
       "ase tambi\xC3\xA9n</h2><ul><li>Madrid</li></ul>"
       "<h2 id=\"s4\">Emoji \xF0\x9F\x98\x80</h2><p>fin<br/>l\xC3\xADnea</p><p>suelto</p></body></html>",
       "article xhtml");
  check(whole.headings.size() == 4, "four headings");
  if (whole.headings.size() == 4) {
    same(whole.headings[0], "Toponimia", "heading 1");
    same(whole.headings[1], "Historia", "heading 2");
    same(whole.headings[3], "Emoji \xF0\x9F\x98\x80", "heading 4");
  }
  // The same bytes however the network cuts them.
  for (const size_t step : {size_t{1}, size_t{3}, size_t{7}, size_t{64}}) {
    const Result cut = convert(json, step);
    same(cut.xhtml, whole.xhtml, "chunked feed matches");
    check(cut.headings == whole.headings, "chunked headings match");
  }
}

void testEdges() {
  const Result missing =
      convert("{\"batchcomplete\":true,\"query\":{\"pages\":[{\"pageid\":99,\"missing\":true}]}}", 5);
  check(!missing.ok, "missing page writes nothing");
  check(missing.missing, "missing page is flagged");
  same(missing.xhtml, "", "missing page has no document");

  const Result empty = convert(answer(""), 4);
  check(!empty.ok, "empty extract is not an article");

  // Cut off mid-tag and mid-list: still a document.
  std::string torn = answer("<p>Hola <b>mundo</p><ul><li>uno<li>dos <i>tres");
  torn.resize(torn.size() - 5);
  const Result cut = convert(torn, 2);
  check(cut.ok, "torn answer still converts");
  check(wellFormed(cut.xhtml), "torn answer is well-formed");
  same(cut.xhtml, "<html><body><p>Hola <b>mundo</b></p><ul><li>uno</li><li>dos <i>tres</i></li></ul></body></html>",
       "torn xhtml");

  const Result odd = convert(answer("<table><tr><td>fuera</td></tr></table><style>.a{}</style>"
                                    "<p>a &copy; b &#x41; &#0; c & d &amp</p><li>hu\\u00e9rfano</li>"),
                             3);
  check(wellFormed(odd.xhtml), "odd input is well-formed");
  same(odd.xhtml,
       "<html><body><p>a &amp;copy; b A &amp;#0; c &amp; d &amp;amp</p><ul><li>hu\xC3\xA9rfano</li></ul></body></html>",
       "odd xhtml");

  {
    StringSink sink;
    wikies::ExtractReader reader(sink);
    reader.setCredit("De Wikipedia & CC");
    const std::string json = answer("<p>Hola</p><h2>Referencias</h2>");
    reader.feed(json.data(), json.size());
    check(reader.finish(), "credited article converts");
    same(sink.text, "<html><body><p>Hola</p><p><i>De Wikipedia &amp; CC</i></p></body></html>", "credit line");
  }

  // Deeper than the tree holds: nothing breaks.
  std::string deep;
  for (int i = 0; i < 40; ++i) deep += "<ul><li>x";
  const Result nested = convert(answer(deep), 9);
  check(nested.ok && wellFormed(nested.xhtml), "deep nesting stays well-formed");
}

void testHits() {
  const char* json =
      "{\"batchcomplete\":true,\"continue\":{\"sroffset\":8,\"continue\":\"-||\"},\"query\":{"
      "\"search\":[{\"ns\":0,\"title\":\"Barcelona\",\"pageid\":3175},{\"ns\":0,\"title\":\"F\\u00fatbol Club "
      "Barcelona\",\"pageid\":1054},{\"ns\":0,\"title\":\"Provincia de Barcelona\",\"pageid\":7}],"
      "\"prefixsearch\":[{\"ns\":0,\"title\":\"Barcelona\",\"pageid\":3175},{\"ns\":0,\"title\":\"Barcelona "
      "\\\"92\\\"\",\"pageid\":88},{\"ns\":4,\"title\":\"Wikipedia:Barcelona\",\"pageid\":5}]}}";
  std::vector<wikies::Hit> hits;
  const int n = wikies::parseHits(json, std::strlen(json), 8, hits);
  check(n == 4 && hits.size() == 4, "four hits without the repeat or the project page");
  if (hits.size() == 4) {
    same(hits[0].title, "Barcelona", "prefix match first");
    same(hits[1].title, "Barcelona \"92\"", "escaped quotes");
    same(hits[2].title, "F\xC3\xBAtbol Club Barcelona", "unicode escape");
    check(hits[3].id == 7, "last id");
  }
  hits.clear();
  check(wikies::parseHits(json, std::strlen(json), 2, hits) == 2, "max is kept");

  const char* random = "{\"batchcomplete\":true,\"query\":{\"random\":[{\"id\":4242,\"ns\":0,\"title\":\"Azar\"}]}}";
  hits.clear();
  check(wikies::parseHits(random, std::strlen(random), 1, hits) == 1 && hits[0].id == 4242, "random hit");

  hits.clear();
  check(wikies::parseHits("<html>502</html>", 16, 8, hits) == 0, "not json: no hits");
  check(wikies::parseHits("{\"error\":{\"code\":\"x\"}}", 22, 8, hits) == 0, "error answer: no hits");
}

void testLists() {
  const std::string top =
      "{\"items\":[{\"project\":\"es.wikipedia\",\"access\":\"all-access\",\"year\":\"2026\",\"month\":\"11\","
      "\"day\":\"18\",\"articles\":[{\"article\":\"Wikipedia:Portada\",\"views\":900,\"rank\":1},"
      "{\"article\":\"Especial:Buscar\",\"views\":800,\"rank\":2},"
      "{\"article\":\"Copa_Mundial_de_F\\u00fatbol\",\"views\":700,\"rank\":3},"
      "{\"article\":\"Archivo:Foto.jpg\",\"views\":650,\"rank\":4},"
      "{\"article\":\"-\",\"views\":640,\"rank\":5},"
      "{\"article\":\"Anexo:Pa\xC3\xADses\",\"views\":600,\"rank\":6},"
      "{\"article\":\"AC/DC\",\"views\":500,\"rank\":7},"
      "{\"article\":\"Cortado_a_med";
  std::vector<wikies::Hit> hits;
  check(wikies::parseTop(top.data(), top.size(), 24, hits) == 3,
        "three articles, the rest are not, the torn one is dropped");
  if (hits.size() == 3) {
    same(hits[0].title, "Copa Mundial de F\xC3\xBAtbol", "underscores become spaces");
    same(hits[1].title, "Anexo:Pa\xC3\xADses", "an annex is an article");
    same(hits[2].title, "AC/DC", "slash kept");
    check(hits[0].id == 0, "top hits carry no id");
  }
  hits.clear();
  check(wikies::parseTop(top.data(), top.size(), 1, hits) == 1, "top max is kept");
  hits.clear();
  check(wikies::parseTop("{\"type\":\"not_found\"}", 20, 24, hits) == 0, "404 body: nothing");

  const char* members =
      "{\"batchcomplete\":true,\"continue\":{\"cmcontinue\":\"page|x|1\",\"continue\":\"-||\"},\"query\":{"
      "\"categorymembers\":[{\"pageid\":11,\"ns\":0,\"title\":\"Marte (planeta)\"},{\"pageid\":12,\"ns\":0,"
      "\"title\":\"Mar\\u00eda Zambrano\"}]}}";
  hits.clear();
  check(wikies::parseHits(members, std::strlen(members), 24, hits) == 2 && hits[1].id == 12, "category members");

  same(wikies::topUrl(2026, 11, 8),
       "https://wikimedia.org/api/rest_v1/metrics/pageviews/top/es.wikipedia/all-access/2026/11/08", "day url");
  same(wikies::topUrl(2026, 10, 0),
       "https://wikimedia.org/api/rest_v1/metrics/pageviews/top/es.wikipedia/all-access/2026/10/all-days", "month url");
  const std::string cat = wikies::categoryUrl(
      "Wikipedia:Art\xC3\xAD"
      "culos destacados",
      "MA");
  check(cat.find("cmtitle=Categor%C3%ADa%3AWikipedia%3AArt%C3%ADculos%20destacados&cmstartsortkeyprefix=MA") !=
            std::string::npos,
        "category url");
  check(wikies::categoryUrl("X", "").find("cmstartsortkeyprefix") == std::string::npos, "category url from the start");
  check(wikies::articleUrlByTitle("AC/DC & m\xC3\xA1s").find("titles=AC%2FDC%20%26%20m%C3%A1s") != std::string::npos,
        "article by title url");
}

void testText() {
  check(wikies::titleMatches("Pe\xC3\xB1\xC3\xADscola", "penis"), "accents fold");
  check(wikies::titleMatches("F\xC3\xBAtbol Club Barcelona", "barcelona FUTBOL"), "words in any order");
  check(!wikies::titleMatches("Barcelona", "madrid"), "no match");
  check(!wikies::titleMatches("Barcelona", "  "), "blank query matches nothing");
  check(wikies::searchUrl("a\xC3\xB1o 1&2").find("pssearch=a%C3%B1o%201%262&") != std::string::npos, "query encoded");
  check(wikies::articleUrl(3175).find("pageids=3175") != std::string::npos, "article url");
  check(wikies::randomUrl().find("list=random") != std::string::npos, "random url");
}

}  // namespace

int main() {
  testArticle();
  testEdges();
  testHits();
  testLists();
  testText();
  if (failures == 0) std::printf("wikionline: all passed\n");
  return failures == 0 ? 0 : 1;
}

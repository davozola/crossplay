#include "WikiOnlineCore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../json/FlatJson.h"
#include "WikipediaFold.h"

namespace wikies {

namespace {

constexpr char kApi[] = "https://es.wikipedia.org/w/api.php?format=json&formatversion=2&utf8=1";

void percentEncode(std::string& out, const std::string& text) {
  static const char kHex[] = "0123456789ABCDEF";
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      out += ch;
    } else {
      out += '%';
      out += kHex[c >> 4];
      out += kHex[c & 15];
    }
  }
}

void appendUtf8(std::string& out, const uint32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

int hexValue(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// A JSON string value, unescaped.
std::string jsonString(const flatjson::Span& v) {
  std::string out;
  if (!v.ok() || *v.p != '"') return out;
  uint32_t high = 0;
  for (const char* p = v.p + 1; p < v.end && *p != '"'; ++p) {
    if (*p != '\\' || p + 1 >= v.end) {
      out += *p;
      continue;
    }
    ++p;
    switch (*p) {
      case 'n':
      case 'r':
      case 't':
      case 'b':
      case 'f':
        out += ' ';
        break;
      case 'u': {
        if (p + 4 >= v.end) return out;
        uint32_t cp = 0;
        for (int i = 1; i <= 4; ++i) {
          const int h = hexValue(p[i]);
          cp = (cp << 4) | static_cast<uint32_t>(h < 0 ? 0 : h);
        }
        p += 4;
        if (cp >= 0xD800 && cp < 0xDC00) {
          high = cp;
        } else if (cp >= 0xDC00 && cp < 0xE000) {
          if (high) appendUtf8(out, 0x10000 + ((high - 0xD800) << 10) + (cp - 0xDC00));
          high = 0;
        } else {
          appendUtf8(out, cp);
        }
        break;
      }
      default:
        out += *p;
    }
  }
  return out;
}

void hitsFrom(const flatjson::Span& array, const int max, std::vector<Hit>& out, int& added) {
  flatjson::forEach(array, 64, [&](int, const flatjson::Span& el) {
    if (added >= max) return;
    double ns = 0;
    if (flatjson::numberD(flatjson::member(el, "ns"), ns) && ns != 0) return;
    double id = 0;
    if (!flatjson::numberD(flatjson::member(el, "pageid"), id) && !flatjson::numberD(flatjson::member(el, "id"), id)) {
      return;
    }
    if (id < 1) return;
    Hit hit;
    hit.id = static_cast<uint32_t>(id);
    hit.title = jsonString(flatjson::member(el, "title"));
    if (hit.title.empty()) return;
    for (const Hit& have : out) {
      if (have.id == hit.id) return;
    }
    out.push_back(std::move(hit));
    ++added;
  });
}

bool isSpace(const char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

// Sections that are nothing once the references and the links are gone.
bool leftOut(const std::string& heading) {
  static const char* const kNames[] = {"referencias", "enlaces externos", "notas y referencias",
                                       "referencias y notas"};
  std::string lower;
  for (const char c : heading) lower += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
  for (const char* name : kNames) {
    if (lower == name) return true;
  }
  return false;
}

}  // namespace

// ------------------------------------------------------------------ urls

std::string searchUrl(const std::string& query) {
  std::string q;
  percentEncode(q, query);
  std::string url = kApi;
  url += "&action=query&list=prefixsearch%7Csearch&psnamespace=0&pslimit=8&pssearch=";
  url += q;
  url += "&srnamespace=0&srlimit=8&srprop=&srinfo=&srsearch=";
  url += q;
  return url;
}

std::string randomUrl() { return std::string(kApi) + "&action=query&list=random&rnnamespace=0&rnlimit=1"; }

std::string articleUrl(const uint32_t pageId) {
  return std::string(kApi) + "&action=query&prop=extracts&redirects=1&pageids=" + std::to_string(pageId);
}

// ---------------------------------------------------------------- answers

int parseHits(const char* json, const size_t len, const int max, std::vector<Hit>& out) {
  const flatjson::Span query = flatjson::member(flatjson::root(json, len), "query");
  int added = 0;
  hitsFrom(flatjson::member(query, "prefixsearch"), max, out, added);
  hitsFrom(flatjson::member(query, "search"), max, out, added);
  hitsFrom(flatjson::member(query, "random"), max, out, added);
  return added;
}

std::string foldTitle(const std::string& title) {
  std::string out(title.size(), '\0');
  size_t outLen = 0;
  wikipedia::foldTitle(title.data(), title.size(), out.data(), out.size(), &outLen);
  out.resize(outLen);
  return out;
}

bool titleMatches(const std::string& title, const std::string& query) {
  const std::string hay = foldTitle(title);
  const std::string needle = foldTitle(query);
  bool any = false;
  size_t at = 0;
  while (at < needle.size()) {
    while (at < needle.size() && needle[at] == ' ') ++at;
    size_t end = at;
    while (end < needle.size() && needle[end] != ' ') ++end;
    if (end > at) {
      if (hay.find(needle.substr(at, end - at)) == std::string::npos) return false;
      any = true;
    }
    at = end;
  }
  return any;
}

// ---------------------------------------------------------------- article

ExtractReader::ExtractReader(Sink& sink) : sink_(sink) {}

void ExtractReader::feed(const char* data, const size_t len) {
  for (size_t i = 0; i < len; ++i) jsonChar(data[i]);
}

bool ExtractReader::finish() {
  if (html_ == Html::Entity) entityDone(false);
  if (inHeading_) endHeading();
  closeAll();
  if (started_ && anyText_ && !credit_.empty()) {
    out("<p><i>");
    for (const char c : credit_) outEscaped(c);
    out("</i></p>");
  }
  if (started_) out("</body></html>");
  flushOut();
  return started_ && anyText_ && !writeFailed_;
}

// ------------------------------------------------------------------- json

void ExtractReader::jsonChar(const char c) {
  if (inString_) {
    if (unicodeLeft_ > 0) {
      const int h = hexValue(c);
      unicode_ = (unicode_ << 4) | static_cast<uint32_t>(h < 0 ? 0 : h);
      if (--unicodeLeft_ == 0) {
        if (unicode_ >= 0xD800 && unicode_ < 0xDC00) {
          highSurrogate_ = unicode_;
        } else if (unicode_ >= 0xDC00 && unicode_ < 0xE000) {
          if (highSurrogate_) emitCodepoint(0x10000 + ((highSurrogate_ - 0xD800) << 10) + (unicode_ - 0xDC00));
          highSurrogate_ = 0;
        } else {
          emitCodepoint(unicode_);
        }
      }
      return;
    }
    if (escape_) {
      escape_ = false;
      switch (c) {
        case 'n':
          stringChar('\n');
          break;
        case 't':
        case 'r':
        case 'b':
        case 'f':
          stringChar(' ');
          break;
        case 'u':
          unicodeLeft_ = 4;
          unicode_ = 0;
          break;
        default:
          stringChar(c);
      }
      return;
    }
    if (c == '\\') {
      escape_ = true;
      return;
    }
    if (c == '"') {
      inString_ = false;
      stringEnd();
      return;
    }
    stringChar(c);
    return;
  }

  if (c == '"') {
    inString_ = true;
    stringIsValue_ = expectValue_;
    expectValue_ = false;
    target_ = Target::None;
    if (stringIsValue_) {
      if (lastKey_ == "extract") target_ = Target::Extract;
      if (lastKey_ == "title") {
        target_ = Target::Title;
        title_.clear();
      }
    } else {
      key_.clear();
    }
    return;
  }
  if (c == ':') {
    lastKey_ = key_;
    expectValue_ = true;
    number_.clear();
    return;
  }
  if (expectValue_ && !isSpace(c)) {
    // A bare value: a number, true, false or null.
    if (c == ',' || c == '}' || c == ']') {
      if (lastKey_ == "pageid" && !number_.empty()) pageId_ = static_cast<uint32_t>(strtoul(number_.c_str(), nullptr, 10));
      if ((lastKey_ == "missing" || lastKey_ == "invalid") && number_ == "true") missing_ = true;
      expectValue_ = false;
      return;
    }
    if (c == '{' || c == '[') {
      expectValue_ = false;
      return;
    }
    if (number_.size() < 16) number_ += c;
  }
}

void ExtractReader::emitCodepoint(const uint32_t cp) {
  std::string bytes;
  appendUtf8(bytes, cp);
  for (const char b : bytes) stringChar(b);
}

void ExtractReader::stringChar(const char c) {
  if (!stringIsValue_) {
    if (key_.size() < 32) key_ += c;
    return;
  }
  valueByte(c);
}

void ExtractReader::valueByte(const char c) {
  if (target_ == Target::Extract) {
    htmlByte(c);
  } else if (target_ == Target::Title) {
    if (title_.size() < 300) title_ += c;
  }
}

void ExtractReader::stringEnd() {
  if (stringIsValue_ && target_ == Target::Extract) {
    if (html_ == Html::Entity) entityDone(false);
    html_ = Html::Text;
  }
  target_ = Target::None;
}

// ------------------------------------------------------------------- html

void ExtractReader::htmlByte(const char c) {
  switch (html_) {
    case Html::Text:
      if (c == '<') {
        html_ = Html::Tag;
        tag_.clear();
      } else if (c == '&') {
        html_ = Html::Entity;
        entity_.clear();
      } else {
        textByte(c);
      }
      return;
    case Html::Tag:
      if (tag_.size() == 2 && tag_ == "!-" && c == '-') {
        html_ = Html::Comment;
        commentDashes_ = 0;
        return;
      }
      if (c == '>') {
        html_ = Html::Text;
        handleTag();
        return;
      }
      if (tag_.size() < 200) tag_ += c;
      return;
    case Html::Comment:
      if (c == '-') {
        ++commentDashes_;
      } else if (c == '>' && commentDashes_ >= 2) {
        html_ = Html::Text;
      } else {
        commentDashes_ = 0;
      }
      return;
    case Html::Entity:
      if (c == ';') {
        entityDone(true);
        html_ = Html::Text;
      } else if (entity_.size() >= 10 || c == '<' || c == '&' || isSpace(c)) {
        // Not an entity after all: a bare ampersand in the text.
        entityDone(false);
        html_ = Html::Text;
        htmlByte(c);
      } else {
        entity_ += c;
      }
      return;
  }
}

void ExtractReader::entityDone(const bool terminated) {
  const std::string name = entity_;
  entity_.clear();
  if (terminated) {
    uint32_t cp = 0;
    if (name == "amp") cp = '&';
    else if (name == "lt") cp = '<';
    else if (name == "gt") cp = '>';
    else if (name == "quot") cp = '"';
    else if (name == "apos") cp = '\'';
    else if (name == "nbsp") cp = 0xA0;
    else if (name.size() > 1 && name[0] == '#') {
      const bool hex = name[1] == 'x' || name[1] == 'X';
      cp = static_cast<uint32_t>(strtoul(name.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10));
      if (cp >= 0x110000 || (cp >= 0xD800 && cp < 0xE000)) cp = 0;
    }
    if (cp != 0) {
      textCodepoint(cp);
      return;
    }
  }
  textByte('&');
  for (const char ch : name) textByte(ch);
  if (terminated) textByte(';');
}

void ExtractReader::textCodepoint(const uint32_t cp) {
  std::string bytes;
  appendUtf8(bytes, cp);
  for (const char b : bytes) textByte(b);
}

void ExtractReader::handleTag() {
  const char* p = tag_.c_str();
  bool closing = false;
  if (*p == '/') {
    closing = true;
    ++p;
  }
  char name[12];
  size_t n = 0;
  while (*p && n + 1 < sizeof(name)) {
    char ch = *p;
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + 32);
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))) break;
    name[n++] = ch;
    ++p;
  }
  name[n] = 0;
  if (n == 0) return;  // "<!doctype", "<?", a stray "<"
  const bool selfClosed = !tag_.empty() && tag_.back() == '/';

  if (skipDepth_ > 0) {
    if (strcmp(name, skipName_) == 0) {
      if (closing) {
        --skipDepth_;
      } else if (!selfClosed) {
        ++skipDepth_;
      }
    }
    return;
  }
  if (!closing && !selfClosed && (strcmp(name, "style") == 0 || strcmp(name, "script") == 0 ||
                                  strcmp(name, "table") == 0 || strcmp(name, "figure") == 0)) {
    snprintf(skipName_, sizeof(skipName_), "%s", name);
    skipDepth_ = 1;
    return;
  }
  if (closing) {
    closeTag(name);
  } else {
    openTag(name);
  }
}

void ExtractReader::openTag(const char* name) {
  const bool heading = name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == 0;
  if (inHeading_) {
    if (!heading) return;
    endHeading();
  }
  if (heading) {
    closeAll();
    int level = name[1] - '0';
    if (level < 2) level = 2;
    if (level > 4) level = 4;
    headLevel_ = level;
    head_.clear();
    inHeading_ = true;
    return;
  }
  if (strcmp(name, "p") == 0 || strcmp(name, "div") == 0 || strcmp(name, "blockquote") == 0 ||
      strcmp(name, "dd") == 0 || strcmp(name, "dt") == 0) {
    closeInline();
    closeParagraph();
    push(El::P);
    if (name[1] == 't') push(El::B);
    return;
  }
  if (strcmp(name, "ul") == 0 || strcmp(name, "ol") == 0) {
    closeInline();
    closeParagraph();
    push(name[0] == 'u' ? El::Ul : El::Ol);
    return;
  }
  if (strcmp(name, "li") == 0) {
    closeInline();
    // Back to the list this item belongs to, closing the item before it.
    while (depth_ > 0 && stack_[depth_ - 1].el != El::Ul && stack_[depth_ - 1].el != El::Ol) popThrough(depth_ - 1);
    if (depth_ == 0) push(El::Ul);
    push(El::Li);
    return;
  }
  if (strcmp(name, "b") == 0 || strcmp(name, "strong") == 0) {
    push(El::B);
  } else if (strcmp(name, "i") == 0 || strcmp(name, "em") == 0) {
    push(El::I);
  } else if (strcmp(name, "sub") == 0) {
    push(El::Sub);
  } else if (strcmp(name, "sup") == 0) {
    push(El::Sup);
  } else if (strcmp(name, "br") == 0) {
    if (inContainer() && !skipSection_) {
      bool emitted = false;
      for (int i = 0; i < depth_; ++i) emitted = emitted || stack_[i].emitted;
      if (emitted) {
        out("<br/>");
        lastWasSpace_ = true;
      }
    }
  }
  // Anything else (span, a, abbr, cite, link...) is transparent: its text stays.
}

void ExtractReader::closeTag(const char* name) {
  const bool heading = name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == 0;
  if (inHeading_) {
    if (heading) endHeading();
    return;
  }
  El el;
  if (strcmp(name, "p") == 0 || strcmp(name, "div") == 0 || strcmp(name, "blockquote") == 0 ||
      strcmp(name, "dd") == 0 || strcmp(name, "dt") == 0) {
    el = El::P;
  } else if (strcmp(name, "ul") == 0) {
    el = El::Ul;
  } else if (strcmp(name, "ol") == 0) {
    el = El::Ol;
  } else if (strcmp(name, "li") == 0) {
    el = El::Li;
  } else if (strcmp(name, "b") == 0 || strcmp(name, "strong") == 0) {
    el = El::B;
  } else if (strcmp(name, "i") == 0 || strcmp(name, "em") == 0) {
    el = El::I;
  } else if (strcmp(name, "sub") == 0) {
    el = El::Sub;
  } else if (strcmp(name, "sup") == 0) {
    el = El::Sup;
  } else {
    return;
  }
  const int at = find(el);
  if (at >= 0) popThrough(at);
}

void ExtractReader::endHeading() {
  inHeading_ = false;
  while (!head_.empty() && head_.back() == ' ') head_.pop_back();
  if (head_.empty()) return;
  if (headLevel_ == 2) {
    // A new section: whatever headings were still waiting had nothing under them.
    pendingHeads_.clear();
    skipSection_ = leftOut(head_);
    if (!skipSection_) pendingHeads_.push_back({2, head_});
  } else if (!skipSection_) {
    while (!pendingHeads_.empty() && pendingHeads_.back().level >= headLevel_) pendingHeads_.pop_back();
    pendingHeads_.push_back({headLevel_, head_});
  }
  head_.clear();
}

void ExtractReader::textByte(const char c) {
  if (skipDepth_ > 0) return;
  const auto u = static_cast<unsigned char>(c);
  const bool space = isSpace(c);
  if (u < 0x20 && !space) return;
  if (inHeading_) {
    if (space) {
      if (!head_.empty() && head_.back() != ' ') head_ += ' ';
    } else if (head_.size() < 160) {
      head_ += c;
    }
    return;
  }
  if (skipSection_) return;
  if (space) {
    // Between blocks, and at the head of one, a space is nothing.
    if (lastWasSpace_ || !inContainer()) return;
    bool emitted = false;
    for (int i = 0; i < depth_; ++i) emitted = emitted || stack_[i].emitted;
    if (!emitted) return;
    out(" ");
    lastWasSpace_ = true;
    return;
  }
  if (!inContainer()) push(El::P);
  flushHeads();
  emitOpens();
  outEscaped(c);
  lastWasSpace_ = false;
  anyText_ = true;
}

// ------------------------------------------------------------------- tree

namespace {
const char* elName(const uint8_t el) {
  static const char* const kNames[] = {"p", "ul", "ol", "li", "b", "i", "sub", "sup"};
  return kNames[el];
}
}  // namespace

void ExtractReader::push(const El el) {
  if (depth_ >= kMaxDepth) return;
  stack_[depth_++] = Open{el, false};
  if (el == El::P || el == El::Li) lastWasSpace_ = true;
}

void ExtractReader::popThrough(const int index) {
  while (depth_ > index) {
    const Open top = stack_[--depth_];
    if (top.emitted) {
      out("</");
      out(elName(static_cast<uint8_t>(top.el)));
      out(">");
    }
    // A block ended: the next words start clean. After bold or italic the
    // space that follows is a real one.
    if (top.el == El::P || top.el == El::Li || top.el == El::Ul || top.el == El::Ol) lastWasSpace_ = true;
  }
}

void ExtractReader::closeAll() {
  if (depth_ > 0) popThrough(0);
}

void ExtractReader::closeInline() {
  while (depth_ > 0) {
    const El top = stack_[depth_ - 1].el;
    if (top == El::B || top == El::I || top == El::Sub || top == El::Sup) {
      popThrough(depth_ - 1);
    } else {
      break;
    }
  }
}

// Closes the paragraph on top, if there is one above the nearest list.
void ExtractReader::closeParagraph() {
  if (depth_ > 0 && stack_[depth_ - 1].el == El::P) popThrough(depth_ - 1);
}

int ExtractReader::find(const El el) const {
  for (int i = depth_ - 1; i >= 0; --i) {
    if (stack_[i].el == el) return i;
  }
  return -1;
}

bool ExtractReader::inContainer() const {
  for (int i = depth_ - 1; i >= 0; --i) {
    if (stack_[i].el == El::P || stack_[i].el == El::Li) return true;
    if (stack_[i].el == El::Ul || stack_[i].el == El::Ol) return false;
  }
  return false;
}

void ExtractReader::emitOpens() {
  for (int i = 0; i < depth_; ++i) {
    if (stack_[i].emitted) continue;
    out("<");
    out(elName(static_cast<uint8_t>(stack_[i].el)));
    out(">");
    stack_[i].emitted = true;
  }
}

// The headings that were waiting for the first words under them. They go out
// ahead of the block those words are in, which has not been opened yet.
void ExtractReader::flushHeads() {
  if (pendingHeads_.empty()) return;
  for (const PendingHead& head : pendingHeads_) {
    char open[32];
    if (head.level == 2) {
      headings_.push_back(head.text);
      snprintf(open, sizeof(open), "<h2 id=\"s%u\">", static_cast<unsigned>(headings_.size()));
    } else {
      snprintf(open, sizeof(open), "<h%d>", head.level);
    }
    out(open);
    for (const char c : head.text) outEscaped(c);
    snprintf(open, sizeof(open), "</h%d>", head.level);
    out(open);
  }
  pendingHeads_.clear();
}

// -------------------------------------------------------------------- out

void ExtractReader::out(const char* s) { out(s, strlen(s)); }

void ExtractReader::out(const char* s, const size_t n) {
  if (!started_) {
    started_ = true;
    out("<html><body>");
  }
  for (size_t i = 0; i < n; ++i) {
    if (bufLen_ == sizeof(buf_)) flushOut();
    buf_[bufLen_++] = s[i];
  }
}

void ExtractReader::outEscaped(const char c) {
  if (c == '&') {
    out("&amp;");
  } else if (c == '<') {
    out("&lt;");
  } else if (c == '>') {
    out("&gt;");
  } else {
    out(&c, 1);
  }
}

void ExtractReader::flushOut() {
  if (bufLen_ == 0) return;
  if (!writeFailed_ && !sink_.write(buf_, bufLen_)) writeFailed_ = true;
  bytesOut_ += bufLen_;
  bufLen_ = 0;
}

}  // namespace wikies

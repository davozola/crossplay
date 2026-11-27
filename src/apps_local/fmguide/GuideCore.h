#pragma once

// FM-1 GUIAS: user guides to the community firmwares of the M-VAVE FM-1, to be
// read beside the synth. Freestanding: what a guide is made of, and the checks
// host-tests/fmguide runs over every one. The guides themselves are in
// Guide*.cpp, in Spanish.
//
// A guide is chapters; a chapter is a run of blocks that the reader lays out
// into pages. Text is UTF-8 inside Latin-1, which is what the reading serif
// carries. Anything set in the pixel face (labels on the drawn panel, the
// flow boxes, the step grid) is ASCII capitals.

#include <cstdint>

namespace fmguide {

// The FM-1's controls, as bits, for the panel drawing to light.
enum Control : uint32_t {
  cMaster = 1u << 0,
  cSelect = 1u << 1,
  cPresets = 1u << 2,
  cAlgorithm = 1u << 3,
  cOctDown = 1u << 4,
  cOctUp = 1u << 5,
  cKnob1 = 1u << 6,
  cKnob2 = 1u << 7,
  cKnob3 = 1u << 8,
  cKnob4 = 1u << 9,
  cFx = 1u << 10,
  cSel = 1u << 11,  // printed SEL on the unit; Felucca's manual calls it SCL
  cEnv = 1u << 12,
  cLfo = 1u << 13,
  cEdit = 1u << 14,
  cGlo = 1u << 15,
  cHome = 1u << 16,
  cSave = 1u << 17,
  cArp = 1u << 18,
  cSeq = 1u << 19,
  cPlay = 1u << 20,
  cRec = 1u << 21,
  cScreen = 1u << 22,
  cKeys = 1u << 23,
  cKnobs = cKnob1 | cKnob2 | cKnob3 | cKnob4,
  cOct = cOctDown | cOctUp,
  cAll = (1u << 24) - 1,
};
constexpr int kControls = 24;

// The 27 keys: 16 white (F3 to G5) in bits 0-15, 11 black in bits 16-26.
constexpr int kWhiteKeys = 16;
constexpr int kBlackKeys = 11;
constexpr uint32_t white(const int n) { return 1u << (n - 1); }   // 1-based
constexpr uint32_t black(const int n) { return 1u << (15 + n); }  // 1-based
constexpr uint32_t kAllWhite = 0xFFFFu;
constexpr uint32_t kAllBlack = 0x7FFu << 16;
// The white key (0-based) each black key sits after.
constexpr int8_t kBlackAfter[kBlackKeys] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};

enum class Kind : uint8_t {
  Head,    // a: a heading
  Para,    // a: a paragraph
  Step,    // a: its number, b: what to do
  Bullet,  // a: one point
  Note,    // a: a label in capitals, b: the note
  Row,     // a: a control or a name, b: what it does
  Panel,   // mask: the controls lit, a: a caption
  Keys,    // mask: the keys lit, a: a caption
  Flow,    // a: boxes joined by '>' with '|' between rows, b: a caption
  Grid,    // a: rows of "NAME:" and 16 of x X or -, '|' between rows, b: a caption
};

struct Block {
  Kind kind;
  const char* a;
  const char* b;
  uint32_t mask;
};

constexpr Block H(const char* text) { return {Kind::Head, text, nullptr, 0}; }
constexpr Block P(const char* text) { return {Kind::Para, text, nullptr, 0}; }
constexpr Block S(const char* number, const char* text) { return {Kind::Step, number, text, 0}; }
constexpr Block B(const char* text) { return {Kind::Bullet, text, nullptr, 0}; }
constexpr Block N(const char* label, const char* text) { return {Kind::Note, label, text, 0}; }
constexpr Block R(const char* control, const char* text) { return {Kind::Row, control, text, 0}; }
constexpr Block PANEL(const uint32_t mask, const char* caption) { return {Kind::Panel, caption, nullptr, mask}; }
constexpr Block KEYS(const uint32_t mask, const char* caption) { return {Kind::Keys, caption, nullptr, mask}; }
constexpr Block FLOW(const char* boxes, const char* caption) { return {Kind::Flow, boxes, caption, 0}; }
constexpr Block GRID(const char* rows, const char* caption) { return {Kind::Grid, rows, caption, 0}; }

struct Chapter {
  const char* title;  // Latin-1
  const Block* blocks;
  int count;
};

struct Guide {
  const char* name;     // ASCII capitals: it goes in the band
  const char* tagline;  // Latin-1, one line
  const char* version;  // what the guide was written against
  const Chapter* chapters;
  int count;
};

constexpr int kGuides = 7;
// How many guides the first save file held. unpack() still reads one that short.
constexpr int kGuidesFirstSave = 5;
const Guide& guideAt(int index);

// The short names the drawn panel prints on each control, in bit order.
const char* controlLabel(int bit);

// Where a reader is in each guide, as bytes for the card and back.
struct Place {
  uint8_t chapter = 0;
  uint8_t page = 0;
};
constexpr int kSaveBytes = 2 + kGuides * 2;
void pack(const Place* places, int last, uint8_t* bytes);
bool unpack(const uint8_t* bytes, int size, Place* places, int& last);

// --- Checks, for the tests. Each returns nullptr when the text is good and a
// word on what is wrong when it is not. ---

// UTF-8 that stays inside Latin-1, and no longer than `maxChars` characters.
const char* checkProse(const char* text, int maxChars);
// ASCII capitals, digits and the few signs the pixel face has.
const char* checkLabel(const char* text, int maxChars);
const char* checkFlow(const char* boxes);
const char* checkGrid(const char* rows);
// Every rule a block has to keep.
const char* checkBlock(const Block& block);

// How many rows a grid or a flow has, and one row of either copied out.
int rowsOf(const char* spec);
bool rowAt(const char* spec, int row, char* out, int cap);

}  // namespace fmguide

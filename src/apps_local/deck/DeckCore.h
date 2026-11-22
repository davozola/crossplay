#pragma once

// DECK, the part with no hardware in it: the templates. A template is fifteen
// buttons for one program, and a button is a name and the keys that do it on a
// Mac and on Windows.
//
// Every chord here is letters, digits, arrows, Tab, Enter, Escape, Space and
// function keys, held with modifiers. That is deliberate. A keyboard sends
// where a key is, not what is printed on it, and punctuation moves between a
// Spanish and an English keyboard while those do not: "Ctrl+[" from this
// device would arrive as a different key on half the desks it is used at.
// host-tests/deck holds every chord to that.

#include <cstdint>

namespace deck {

enum class Os : uint8_t { Mac, Windows, Count };

// Modifier bits, as the keyboard report carries them.
constexpr uint8_t kCtrl = 0x01;
constexpr uint8_t kShift = 0x02;
constexpr uint8_t kAlt = 0x04;  // Option on a Mac
constexpr uint8_t kGui = 0x08;  // Command on a Mac, the Windows key elsewhere

// USB HID keyboard usages.
constexpr uint8_t letter(const char c) { return static_cast<uint8_t>(0x04 + (c - 'A')); }
constexpr uint8_t digit(const int d) { return static_cast<uint8_t>(d == 0 ? 0x27 : 0x1E + d - 1); }
constexpr uint8_t fkey(const int n) { return static_cast<uint8_t>(0x3A + n - 1); }
constexpr uint8_t kEnter = 0x28;
constexpr uint8_t kEscape = 0x29;
constexpr uint8_t kTab = 0x2B;
constexpr uint8_t kSpace = 0x2C;
constexpr uint8_t kRight = 0x4F;
constexpr uint8_t kLeft = 0x50;
constexpr uint8_t kDown = 0x51;
constexpr uint8_t kUp = 0x52;
constexpr uint8_t kPageUp = 0x4B;
constexpr uint8_t kPageDown = 0x4E;

struct Chord {
  uint8_t mods;
  uint8_t usage;
};

struct Button {
  const char* label;  // capitals, at most two words, each at most nine letters
  Chord mac;
  Chord win;
  constexpr Chord on(const Os os) const { return os == Os::Mac ? mac : win; }
};

constexpr int kButtons = 15;  // five by three on its side, three by five upright

struct Template {
  const char* name;  // capitals, for lists
  Button buttons[kButtons];
};

// How each template is titled on its own page, in the order of kTemplates.
inline constexpr const char* kTitles[] = {"Sistema",    "Chrome",        "Excel",    "Word",
                                          "PowerPoint", "ON1 Photo RAW", "Reuniones"};

// The same letter under Command on a Mac and under Control on Windows, which
// is most of what the two have in common.
constexpr Button both(const char* label, const char c, const uint8_t extra = 0) {
  return {label, {static_cast<uint8_t>(kGui | extra), letter(c)}, {static_cast<uint8_t>(kCtrl | extra), letter(c)}};
}
// The same chord on both.
constexpr Button same(const char* label, const uint8_t mods, const uint8_t usage) {
  return {label, {mods, usage}, {mods, usage}};
}

// What earns a button: a chord that is awkward to hold or hard to remember.
// Copy and paste are under the left hand already and are not here. Each
// template is three rows of five that belong together, and a style may set
// them as rows or as columns.
inline constexpr Template kTemplates[] = {
    {"SISTEMA",
     {// Getting about the desktop.
      {"VENTANAS", {kCtrl, kUp}, {kGui, kTab}},
      {"ESCRITORIO ANTERIOR", {kCtrl, kLeft}, {kCtrl | kGui, kLeft}},
      {"ESCRITORIO SIGUIENTE", {kCtrl, kRight}, {kCtrl | kGui, kRight}},
      {"VER ESCRITORIO", {0, fkey(11)}, {kGui, letter('D')}},
      {"CAMBIAR APP", {kGui, kTab}, {kAlt, kTab}},
      // Taking and finding things.
      {"CAPTURA ZONA", {kGui | kShift, digit(4)}, {kGui | kShift, letter('S')}},
      {"GRABAR PANTALLA", {kGui | kShift, digit(5)}, {kGui | kShift, letter('R')}},
      {"PEGAR LIMPIO", {kGui | kAlt | kShift, letter('V')}, {kCtrl | kShift, letter('V')}},
      {"BUSCAR", {kGui, kSpace}, {kGui, letter('S')}},
      {"ARCHIVOS", {kGui | kAlt, kSpace}, {kGui, letter('E')}},
      // The window in front.
      {"MINIMIZAR", {kGui, letter('M')}, {kGui, kDown}},
      {"PANTALLA COMPLETA", {kGui | kCtrl, letter('F')}, {0, fkey(11)}},
      both("CERRAR VENTANA", 'W'),
      {"FORZAR CIERRE", {kGui | kAlt, kEscape}, {kCtrl | kShift, kEscape}},
      {"BLOQUEAR", {kGui | kCtrl, letter('Q')}, {kGui, letter('L')}}}},
    {"CHROME",
     {// Tabs.
      both("NUEVA PESTAÑA", 'T'),
      both("CERRAR PESTAÑA", 'W'),
      both("REABRIR", 'T', kShift),
      same("PESTAÑA ANTERIOR", kCtrl | kShift, kTab),
      same("PESTAÑA SIGUIENTE", kCtrl, kTab),
      // Finding.
      both("BUSCAR PESTAÑAS", 'A', kShift),
      both("DIRECCIÓN", 'L'),
      both("BUSCAR", 'F'),
      both("RECARGA TOTAL", 'R', kShift),
      both("INCÓGNITO", 'N', kShift),
      // The browser's own pages.
      {"HISTORIAL", {kGui, letter('Y')}, {kCtrl, letter('H')}},
      {"DESCARGAS", {kGui | kShift, letter('J')}, {kCtrl, letter('J')}},
      both("MARCADOR", 'D'),
      {"INSPECTOR", {kGui | kAlt, letter('I')}, {kCtrl | kShift, letter('I')}},
      {"ÚLTIMA PESTAÑA", {kGui, digit(9)}, {kCtrl, digit(9)}}}},
    {"EXCEL",
     {// A cell and what goes into it.
      {"EDITAR CELDA", {kCtrl, letter('U')}, {0, fkey(2)}},
      {"FIJAR REF", {kGui, letter('T')}, {0, fkey(4)}},
      both("PEGAR VALORES", 'V', kShift),
      {"PEGADO ESPECIAL", {kGui | kCtrl, letter('V')}, {kCtrl | kAlt, letter('V')}},
      {"FORMATO CELDAS", {kGui, digit(1)}, {kCtrl, digit(1)}},
      // A range.
      same("FILTRO", kCtrl | kShift, letter('L')),
      same("TABLA", kCtrl, letter('T')),
      both("RELLENAR ABAJO", 'D'),
      // Shift and a digit is the same sign on a Spanish and an English keyboard
      // for 1 and 5, which is what these two are written as.
      same("FORMATO MILES", kCtrl | kShift, digit(1)),
      same("PORCENTAJE", kCtrl | kShift, digit(5)),
      // A model: its outline, its sheets, its numbers.
      {"AGRUPAR", {kGui | kShift, letter('K')}, {kAlt | kShift, kRight}},
      {"DESAGRUPAR", {kGui | kShift, letter('J')}, {kAlt | kShift, kLeft}},
      {"HOJA ANTERIOR", {kAlt, kLeft}, {kCtrl, kPageUp}},
      {"HOJA SIGUIENTE", {kAlt, kRight}, {kCtrl, kPageDown}},
      same("RECALCULAR", 0, fkey(9))}},
    {"WORD",
     {// Styles.
      {"TÍTULO 1", {kGui | kAlt, digit(1)}, {kCtrl | kAlt, digit(1)}},
      {"TÍTULO 2", {kGui | kAlt, digit(2)}, {kCtrl | kAlt, digit(2)}},
      {"TÍTULO 3", {kGui | kAlt, digit(3)}, {kCtrl | kAlt, digit(3)}},
      both("NORMAL", 'N', kShift),
      both("VIÑETAS", 'L', kShift),
      // Formatting carried, dropped and changed. Word moved the format
      // painter's chords in 2024 to give Shift+V to pasting text alone.
      both("PEGAR LIMPIO", 'V', kShift),
      {"COPIAR FORMATO", {kGui | kAlt, letter('C')}, {kCtrl | kAlt, letter('C')}},
      {"PEGAR FORMATO", {kGui | kAlt, letter('V')}, {kCtrl | kAlt, letter('V')}},
      same("QUITAR FORMATO", kCtrl, kSpace),
      same("MAYÚSCULAS", kShift, fkey(3)),
      // Reviewing.
      {"COMENTAR", {kGui | kAlt, letter('A')}, {kCtrl | kAlt, letter('M')}},
      both("CONTROL CAMBIOS", 'E', kShift),
      {"NOTA PIE", {kGui | kAlt, letter('F')}, {kCtrl | kAlt, letter('F')}},
      both("CONTAR PALABRAS", 'G', kShift),
      {"SALTO PÁGINA", {kGui, kEnter}, {kCtrl, kEnter}}}},
    {"POWERPOINT",
     {// Building a slide.
      same("NUEVA DIAPOSITIVA", kCtrl, letter('M')),
      both("DUPLICAR", 'D'),
      {"AGRUPAR", {kGui | kAlt, letter('G')}, {kCtrl, letter('G')}},
      {"DESAGRUPAR", {kGui | kAlt | kShift, letter('G')}, {kCtrl | kShift, letter('G')}},
      both("CENTRAR", 'E'),
      // Making it tidy.
      both("COPIAR FORMATO", 'C', kShift),
      both("PEGAR FORMATO", 'V', kShift),
      {"GUÍAS", {kGui | kAlt | kCtrl, letter('G')}, {kAlt, fkey(9)}},
      same("CUADRÍCULA", kShift, fkey(9)),
      both("NEGRITA", 'B'),
      // Showing it.
      {"DESDE INICIO", {kGui | kShift, kEnter}, {0, fkey(5)}},
      {"DESDE AQUÍ", {kGui, kEnter}, {kShift, fkey(5)}},
      {"MODERADOR", {kAlt, kEnter}, {kAlt, fkey(5)}},
      same("PANTALLA NEGRA", 0, letter('B')),
      same("TERMINAR", 0, kEscape)}},
    // ON1 Photo RAW answers to single letters, the same on both systems; only
    // its two zooms take the system's modifier.
    {"ON1 PHOTO RAW",
     {same("EXPLORAR", 0, letter('G')),
      same("REVELAR", 0, letter('D')),
      same("EFECTOS", 0, letter('S')),
      same("AJUSTE LOCAL", 0, letter('A')),
      same("VER FOTO", 0, letter('E')),
      same("TIRA", 0, letter('F')),
      same("COMPARAR", 0, letter('C')),
      same("VER MÁSCARA", 0, letter('O')),
      same("OCULTAR PANELES", 0, kTab),
      same("ME GUSTA", 0, letter('P')),
      same("DESCARTAR", 0, letter('X')),
      same("TRES ESTRELLAS", 0, digit(3)),
      same("CINCO ESTRELLAS", 0, digit(5)),
      // Fit and 100% are the zooms with keys of their own; the stepped zoom is
      // on + and -, which move between a Spanish and an English keyboard.
      {"AJUSTAR", {kGui, digit(0)}, {kCtrl, digit(0)}},
      {"ZOOM 100%", {kGui | kAlt, digit(0)}, {kCtrl | kAlt, digit(0)}}}},
    // A call, in whichever of the three it is: the microphone, the camera, the
    // hand, what is shared or said beside it, and the way out. Each program
    // answers only while its own window is in front.
    {"REUNIONES",
     {both("TEAMS MICRO", 'M', kShift),
      both("TEAMS CÁMARA", 'O', kShift),
      both("TEAMS MANO", 'K', kShift),
      both("TEAMS COMPARTIR", 'E', kShift),
      both("TEAMS SALIR", 'H', kShift),
      {"ZOOM MICRO", {kGui | kShift, letter('A')}, {kAlt, letter('A')}},
      {"ZOOM CÁMARA", {kGui | kShift, letter('V')}, {kAlt, letter('V')}},
      same("ZOOM MANO", kAlt, letter('Y')),
      {"ZOOM COMPARTIR", {kGui | kShift, letter('S')}, {kAlt, letter('S')}},
      {"ZOOM SALIR", {kGui, letter('W')}, {kAlt, letter('Q')}},
      both("MEET MICRO", 'D'),
      both("MEET CÁMARA", 'E'),
      {"MEET MANO", {kGui | kCtrl, letter('H')}, {kCtrl | kAlt, letter('H')}},
      {"MEET CHAT", {kGui | kCtrl, letter('C')}, {kCtrl | kAlt, letter('C')}},
      {"MEET PERSONAS", {kGui | kCtrl, letter('P')}, {kCtrl | kAlt, letter('P')}}}},
};
constexpr int kTemplateCount = sizeof(kTemplates) / sizeof(kTemplates[0]);
static_assert(sizeof(kTitles) / sizeof(kTitles[0]) == kTemplateCount, "every template has a title");

// The keyboard report for a chord: modifiers, reserved, six keys.
constexpr int kReportBytes = 8;
inline void fillReport(uint8_t report[kReportBytes], const Chord chord) {
  for (int i = 0; i < kReportBytes; ++i) report[i] = 0;
  report[0] = chord.mods;
  report[2] = chord.usage;
}

// How a chord is written on a key: "CMD SHIFT T", "ALT F4", "G". The names are
// the ones printed on each system's own keyboards.
inline int describe(const Chord chord, const Os os, char* out, const int cap) {
  static const char* const kMac[4] = {"CTRL", "SHIFT", "OPT", "CMD"};
  static const char* const kWin[4] = {"CTRL", "SHIFT", "ALT", "WIN"};
  int n = 0;
  const auto put = [&](const char* s) {
    if (n > 0 && n < cap - 1) out[n++] = ' ';
    while (*s && n < cap - 1) out[n++] = *s++;
  };
  // Mac keyboards read Control, Option, Shift, Command; Windows ones Ctrl, Win, Alt, Shift.
  static const int kMacOrder[4] = {0, 2, 1, 3};
  static const int kWinOrder[4] = {0, 3, 2, 1};
  for (int i = 0; i < 4; ++i) {
    const int bit = os == Os::Mac ? kMacOrder[i] : kWinOrder[i];
    if (chord.mods & (1 << bit)) put(os == Os::Mac ? kMac[bit] : kWin[bit]);
  }
  char key[8] = "";
  const uint8_t u = chord.usage;
  if (u >= 0x04 && u <= 0x1D) {
    key[0] = static_cast<char>('A' + u - 0x04);
  } else if (u >= 0x1E && u <= 0x26) {
    key[0] = static_cast<char>('1' + u - 0x1E);
  } else if (u == 0x27) {
    key[0] = '0';
  } else if (u >= 0x3A && u <= 0x45) {
    const int f = u - 0x3A + 1;
    key[0] = 'F';
    if (f >= 10) {
      key[1] = '1';
      key[2] = static_cast<char>('0' + f - 10);
    } else {
      key[1] = static_cast<char>('0' + f);
    }
  }
  const char* name = key;
  switch (u) {
    case kEnter:
      name = "INTRO";
      break;
    case kEscape:
      name = "ESC";
      break;
    case kTab:
      name = "TAB";
      break;
    case kSpace:
      name = "ESPACIO";
      break;
    case kRight:
      name = "DCHA";
      break;
    case kLeft:
      name = "IZDA";
      break;
    case kUp:
      name = "ARRIBA";
      break;
    case kDown:
      name = "ABAJO";
      break;
    case kPageUp:
      name = "RE PAG";
      break;
    case kPageDown:
      name = "AV PAG";
      break;
    default:
      break;
  }
  put(name);
  out[n < cap ? n : cap - 1] = 0;
  return n;
}

// A usage this app may send: see the note at the top of the file.
constexpr bool layoutProof(const uint8_t u) {
  return (u >= 0x04 && u <= 0x27) || u == kEnter || u == kEscape || u == kTab || u == kSpace ||
         (u >= 0x3A && u <= 0x45) || (u >= 0x4B && u <= 0x52);
}

// How a template is set on the panel. The buttons and their chords are the
// same in all three; a swipe up or down goes from one to the next.
//   Editorial  a printed table in a serif, with the template named in a column
//   Teclado    a black page of key caps, in the device's own capitals
//   Indice     three columns of five under a black band, a row to each button
enum class Style : uint8_t { Editorial, Teclado, Indice, Count };
constexpr int kStyleCount = static_cast<int>(Style::Count);
constexpr Style stepStyle(const Style style, const int by) {
  return static_cast<Style>(((static_cast<int>(style) + by) % kStyleCount + kStyleCount) % kStyleCount);
}

// Where a button sits. Editorial and Teclado set a template as three rows of
// five; Indice turns each row into a column, so what belongs together still
// sits together.
struct Place {
  int col;
  int row;
};
constexpr int kColumns(const Style style) { return style == Style::Indice ? 3 : 5; }
constexpr int kRows(const Style style) { return style == Style::Indice ? 5 : 3; }
constexpr Place placeOf(const Style style, const int button) {
  return style == Style::Indice ? Place{button / 5, button % 5} : Place{button % 5, button / 5};
}
constexpr int buttonIn(const Style style, const int col, const int row) {
  return style == Style::Indice ? col * 5 + row : row * 5 + col;
}

// What the app remembers between visits. Version 1 had no style and reads as
// Editorial.
constexpr int kStateBytes = 5;
inline void packState(const Os os, const int chosen, const bool asked, const Style style, uint8_t* out) {
  out[0] = 2;  // version
  out[1] = static_cast<uint8_t>(os);
  out[2] = static_cast<uint8_t>(chosen);
  out[3] = asked ? 1 : 0;
  out[4] = static_cast<uint8_t>(style);
}
inline bool unpackState(const uint8_t* in, const int size, Os& os, int& chosen, bool& asked, Style& style) {
  os = Os::Mac;
  chosen = 0;
  asked = false;
  style = Style::Editorial;
  if (size < 4 || (in[0] != 1 && in[0] != 2) || in[1] >= static_cast<uint8_t>(Os::Count) || in[2] >= kTemplateCount) {
    return false;
  }
  if (in[0] == 2 && (size < kStateBytes || in[4] >= kStyleCount)) return false;
  os = static_cast<Os>(in[1]);
  chosen = in[2];
  asked = in[3] != 0;
  if (in[0] == 2) style = static_cast<Style>(in[4]);
  return true;
}

}  // namespace deck

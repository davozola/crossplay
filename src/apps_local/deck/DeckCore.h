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
inline constexpr const char* kTitles[] = {"Sistema", "Chrome", "Excel", "Word", "PowerPoint", "ON1 Photo RAW"};

// The same letter under Command on a Mac and under Control on Windows, which
// is most of what the two have in common.
constexpr Button both(const char* label, const char c, const uint8_t extra = 0) {
  return {label, {static_cast<uint8_t>(kGui | extra), letter(c)}, {static_cast<uint8_t>(kCtrl | extra), letter(c)}};
}
// The same chord on both.
constexpr Button same(const char* label, const uint8_t mods, const uint8_t usage) {
  return {label, {mods, usage}, {mods, usage}};
}

inline constexpr Template kTemplates[] = {
    {"SISTEMA",
     {both("COPIAR", 'C'),
      both("PEGAR", 'V'),
      both("CORTAR", 'X'),
      both("DESHACER", 'Z'),
      both("TODO", 'A'),
      {"CAMBIAR APP", {kGui, kTab}, {kAlt, kTab}},
      {"VENTANAS", {kCtrl, kUp}, {kGui, kTab}},
      {"ESCRITORIO", {0, fkey(11)}, {kGui, letter('D')}},
      {"BUSCAR", {kGui, kSpace}, {kGui, letter('S')}},
      {"CAPTURA", {kGui | kShift, digit(4)}, {kGui | kShift, letter('S')}},
      both("GUARDAR", 'S'),
      both("NUEVO", 'N'),
      {"CERRAR", {kGui, letter('W')}, {kAlt, fkey(4)}},
      {"SALIR", {kGui, letter('Q')}, {kAlt, fkey(4)}},
      {"BLOQUEAR", {kGui | kCtrl, letter('Q')}, {kGui, letter('L')}}}},
    {"CHROME",
     {both("NUEVA PESTAÑA", 'T'),
      both("CERRAR PESTAÑA", 'W'),
      both("REABRIR", 'T', kShift),
      same("PESTAÑA ANTERIOR", kCtrl | kShift, kTab),
      same("PESTAÑA SIGUIENTE", kCtrl, kTab),
      {"ATRÁS", {kGui, kLeft}, {kAlt, kLeft}},
      {"ADELANTE", {kGui, kRight}, {kAlt, kRight}},
      both("RECARGAR", 'R'),
      both("DIRECCIÓN", 'L'),
      both("BUSCAR", 'F'),
      both("INCÓGNITO", 'N', kShift),
      both("MARCADOR", 'D'),
      {"HISTORIAL", {kGui, letter('Y')}, {kCtrl, letter('H')}},
      {"DESCARGAS", {kGui | kShift, letter('J')}, {kCtrl, letter('J')}},
      {"PANTALLA COMPLETA", {kGui | kCtrl, letter('F')}, {0, fkey(11)}}}},
    {"EXCEL",
     {both("GUARDAR", 'S'),
      both("DESHACER", 'Z'),
      both("REHACER", 'Y'),
      both("COPIAR", 'C'),
      both("PEGAR", 'V'),
      {"PEGADO ESPECIAL", {kGui | kCtrl, letter('V')}, {kCtrl | kAlt, letter('V')}},
      {"EDITAR CELDA", {kCtrl, letter('U')}, {0, fkey(2)}},
      same("FUNCIÓN", kShift, fkey(3)),
      {"FORMATO CELDAS", {kGui, digit(1)}, {kCtrl, digit(1)}},
      same("FILTRO", kCtrl | kShift, letter('L')),
      both("RELLENAR ABAJO", 'D'),
      both("TABLA", 'T'),
      same("IR A", kCtrl, letter('G')),
      {"HOJA ANTERIOR", {kAlt, kLeft}, {kCtrl, kPageUp}},
      {"HOJA SIGUIENTE", {kAlt, kRight}, {kCtrl, kPageDown}}}},
    {"WORD",
     {both("GUARDAR", 'S'),
      both("DESHACER", 'Z'),
      both("REHACER", 'Y'),
      both("NEGRITA", 'B'),
      both("CURSIVA", 'I'),
      both("SUBRAYADO", 'U'),
      {"TÍTULO 1", {kGui | kAlt, digit(1)}, {kCtrl | kAlt, digit(1)}},
      {"TÍTULO 2", {kGui | kAlt, digit(2)}, {kCtrl | kAlt, digit(2)}},
      both("NORMAL", 'N', kShift),
      both("VIÑETAS", 'L', kShift),
      both("CENTRAR", 'E'),
      both("BUSCAR", 'F'),
      {"COMENTAR", {kGui | kAlt, letter('A')}, {kCtrl | kAlt, letter('M')}},
      both("CONTROL CAMBIOS", 'E', kShift),
      {"SALTO PÁGINA", {kGui, kEnter}, {kCtrl, kEnter}}}},
    {"POWERPOINT",
     {same("NUEVA DIAPOSITIVA", kCtrl, letter('M')),
      both("DUPLICAR", 'D'),
      both("GUARDAR", 'S'),
      both("DESHACER", 'Z'),
      both("REHACER", 'Y'),
      {"AGRUPAR", {kGui | kAlt, letter('G')}, {kCtrl, letter('G')}},
      {"QUITAR GRUPO", {kGui | kAlt | kShift, letter('G')}, {kCtrl | kShift, letter('G')}},
      both("COPIAR FORMATO", 'C', kShift),
      both("PEGAR FORMATO", 'V', kShift),
      both("CENTRAR", 'E'),
      both("NEGRITA", 'B'),
      {"DESDE INICIO", {kGui | kShift, kEnter}, {0, fkey(5)}},
      {"DESDE AQUÍ", {kGui, kEnter}, {kShift, fkey(5)}},
      same("PANTALLA NEGRA", 0, letter('B')),
      same("TERMINAR", 0, kEscape)}},
    // ON1 Photo RAW answers to single letters, the same on both systems.
    {"ON1 PHOTO RAW",
     {same("EXPLORAR", 0, letter('G')), same("REVELAR", 0, letter('D')), same("EFECTOS", 0, letter('S')),
      same("AJUSTE LOCAL", 0, letter('A')), same("VER FOTO", 0, letter('E')), same("TIRA", 0, letter('F')),
      same("COMPARAR", 0, letter('C')), same("VER MÁSCARA", 0, letter('O')), same("OCULTAR PANELES", 0, kTab),
      same("ME GUSTA", 0, letter('P')), same("DESCARTAR", 0, letter('X')), same("TRES ESTRELLAS", 0, digit(3)),
      same("CINCO ESTRELLAS", 0, digit(5)), same("ANTERIOR", 0, kLeft), same("SIGUIENTE", 0, kRight)}},
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

// What the app remembers between visits.
constexpr int kStateBytes = 4;
inline void packState(const Os os, const int chosen, const bool asked, uint8_t* out) {
  out[0] = 1;  // version
  out[1] = static_cast<uint8_t>(os);
  out[2] = static_cast<uint8_t>(chosen);
  out[3] = asked ? 1 : 0;
}
inline bool unpackState(const uint8_t* in, const int size, Os& os, int& chosen, bool& asked) {
  os = Os::Mac;
  chosen = 0;
  asked = false;
  if (size < kStateBytes || in[0] != 1 || in[1] >= static_cast<uint8_t>(Os::Count) || in[2] >= kTemplateCount) {
    return false;
  }
  os = static_cast<Os>(in[1]);
  chosen = in[2];
  asked = in[3] != 0;
  return true;
}

}  // namespace deck

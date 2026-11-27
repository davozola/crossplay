#include "GuideData.h"
#include "GuideShrubChapters.h"

// SHRUB V1.39, by Steve Duda / Xfer Records (shrub.audio). Checked on 27
// November 2026 against its Operation Guide 1.0, the editor's own engine
// (build 2026-11-25, firmware 1.39: every page, knob, value and name used
// here) and the emulator at shrub.audio/play (every page of the panel).
// The chapters are in GuideShrub*.cpp; this file puts them in order.

namespace fmguide {

namespace {

using namespace shrub;

// The copies are made at start-up from objects that are themselves constant,
// so the order the files are initialised in does not matter.
const Chapter kChapters[kCount] = {
    kWhat,       kCanCant,   kFirstTen,                                                                   //
    kMove,       kPresets,   kOsc,         kFilter,     kMod,          kFx,           kPlay,       kSeq,  //
    kLabIntro,   kLabSub,    kLabAcid,     kLabReese,   kLabSynthwave, kLabLead,      kLabDigital, kLabSupersaw,
    kLabCinema,  kLab80s,    kLabStrings,  kLabPluck,   kLabBerlin,    kLabBells,     kLabWobble,              //
    kLabDrums,   kLabKick,   kLabSnare,    kLabHat,     kLabPerc,      kLabAmbient,   kLabDrone,   kLabRiser,  //
    kHowSound,   kHowPlay,   kHowFiles,    kHowTrouble,                                                        //
    kRefButtons, kRefCombos, kRefMessages, kParOsc,     kParFilterEnv, kParLfoMatrix, kParFx,      kParPlay,
    kParGlobal,  kEditor,    kMidi,  //
    kInstall,    kRescue,    kSources,
};

constexpr Part kParts[] = {
    {"Empezar", "EMPEZAR", "Qué es, qué puede y tus primeros 10 minutos", What, 3},
    {"Manual", "MANUAL", "Cada sección del panel, a fondo", Move, 8},
    {"Laboratorio: bajos y leads", "LAB BAJOS", "Síntesis haciendo sonidos: bajos y solistas", LabIntro, 8},
    {"Laboratorio: pads y teclas", "LAB PADS", "Pads, cuerdas, plucks, arpegios y campanas", LabCinema, 7},
    {"Laboratorio: ritmo y textura", "LAB RITMO", "Batería, texturas, drones y transiciones", LabDrums, 8},
    {"¿Cómo hago...?", "COMO HAGO", "Respuestas directas, por lo que quieres hacer", HowSound, 4},
    {"Atajos y parámetros", "REFERENCIA", "Para consultar mientras tocas", RefButtons, 9},
    {"Editor y MIDI", "EDITOR Y MIDI", "El editor web, el MIDI, USB y Bluetooth", Editor, 2},
    {"Instalar y rescatar", "INSTALAR", "Instalar, volver, rescate, fuentes y versión", Install, 3},
};

}  // namespace

const Guide kGuideShrub = {
    "SHRUB", "Serum en la FM-1: tablas de ondas, filtros, matriz y efectos.",
    "V1.39", kChapters,
    kCount,  "27-11-2026",
    kParts,  static_cast<int>(sizeof(kParts) / sizeof(kParts[0])),
};

}  // namespace fmguide

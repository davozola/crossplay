#pragma once

// SHRUB's guide is long enough to be several files: what to read first, the
// manual, the sound lab, the questions, the reference, the editor and
// installing. Each file defines its chapters; GuideShrub.cpp puts them in
// order and into parts. The numbers here are what a LINK points at.

#include "GuideCore.h"

namespace fmguide {
namespace shrub {

enum Ch : uint8_t {
  // EMPEZAR
  What,
  CanCant,
  FirstTen,
  // MANUAL
  Move,
  Presets,
  Osc,
  Filter,
  Mod,
  Fx,
  Play,
  Seq,
  // LABORATORIO: BAJOS Y LEADS
  LabIntro,
  LabSub,
  LabAcid,
  LabReese,
  LabSynthwave,
  LabLead,
  LabDigital,
  LabSupersaw,
  // LABORATORIO: PADS Y TECLAS
  LabCinema,
  Lab80s,
  LabStrings,
  LabPluck,
  LabBerlin,
  LabBells,
  LabWobble,
  // LABORATORIO: RITMO Y TEXTURA
  LabDrums,
  LabKick,
  LabSnare,
  LabHat,
  LabPerc,
  LabAmbient,
  LabDrone,
  LabRiser,
  // COMO HAGO
  HowSound,
  HowPlay,
  HowFiles,
  HowTrouble,
  // ATAJOS Y PARAMETROS
  RefButtons,
  RefCombos,
  RefMessages,
  ParOsc,
  ParFilterEnv,
  ParLfoMatrix,
  ParFx,
  ParPlay,
  ParGlobal,
  // EDITOR Y MIDI
  Editor,
  Midi,
  // INSTALAR Y RESCATAR
  Install,
  Rescue,
  Sources,
  kCount,
};

extern const Chapter kWhat, kCanCant, kFirstTen;
extern const Chapter kMove, kPresets, kOsc, kFilter, kMod, kFx, kPlay, kSeq;
extern const Chapter kLabIntro, kLabSub, kLabAcid, kLabReese, kLabSynthwave, kLabLead, kLabDigital, kLabSupersaw;
extern const Chapter kLabCinema, kLab80s, kLabStrings, kLabPluck, kLabBerlin, kLabBells, kLabWobble;
extern const Chapter kLabDrums, kLabKick, kLabSnare, kLabHat, kLabPerc, kLabAmbient, kLabDrone, kLabRiser;
extern const Chapter kHowSound, kHowPlay, kHowFiles, kHowTrouble;
extern const Chapter kRefButtons, kRefCombos, kRefMessages;
extern const Chapter kParOsc, kParFilterEnv, kParLfoMatrix, kParFx, kParPlay, kParGlobal;
extern const Chapter kEditor, kMidi, kInstall, kRescue, kSources;

}  // namespace shrub
}  // namespace fmguide

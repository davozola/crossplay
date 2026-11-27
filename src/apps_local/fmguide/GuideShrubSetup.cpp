#include "GuideData.h"
#include "GuideShrubChapters.h"
#include "SynthFigures.h"

// SHRUB, EDITOR Y MIDI, INSTALAR, FUENTES. The editor's menus are as built on
// 25 November 2026 (df37609, firmware 1.39); installing and recovery follow
// the Operation Guide and the install page, worded without promises the
// sources do not make.

namespace fmguide {
namespace shrub {

namespace {

using namespace synthfig;

constexpr Block kEditorBlocks[] = {
    P("shrub.audio/editor, en Chrome o Edge en el ordenador. Sin la FM-1 hace de sinte en el navegador; con Connect "
      "FM-1 edita la tuya por USB y puede enseñar su pantalla en grande. Pestañas: OSC, FX, MATRIX, GLOBAL, SEQ y "
      "LIBRARY. Hay también un emulador completo de la FM-1 en shrub.audio/play."),
    H("El menú File"),
    R("Undo, Redo", "Deshacer y rehacer cambios del sonido."),
    R("Init Preset", "Pone el sonido en INIT."),
    R("Open Patch or Serum 1 Preset", "Abre un .shrub o uno o varios .fxp de Serum 1."),
    R("Open Serum 1 Folder as Bank", "Una carpeta de .fxp como banco entero."),
    R("Browse Serum 1 Factory", "Los presets de fábrica de Serum 1, sin tener Serum."),
    R("Save to FM-1 User Presets", "Manda el sonido a un hueco de la FM-1."),
    R("Save Patch to Disk", "Un .shrub: el sonido con sus pistas."),
    R("Export to Serum2", "Un .SerumPreset. Arpegio, escala y azar por nota no pasan."),
    R("Share This Preset", "Un enlace con el sonido. No lleva tus tablas propias."),
    R("Render a Note to Audio", "Un .wav de 24 bits estéreo, con hasta 10 s de cola tras soltar."),
    R("User Wavetables", "Tus tablas: WAV de Serum o de 2048 muestras por fotograma, hasta 64. Tres bancos de 16."),
    R("MIDI map", "Cargar, guardar, por defecto o vaciar el mapa de MIDI Learn."),
    R("Update Firmware", "Lleva al instalador."),
    H("Bancos"),
    R("De fábrica",
      "Default Shrub, FjordLead 1 y 2, General MIDI, Mental Modulations, Juneau, Saif Sameer Sigil, Vintage Tones y "
      "Virtual Virtual Riot."),
    R("Load Bank, Save Bank", "Abrir o guardar un .shrubbank: 128 huecos, tablas y mapa MIDI."),
    R("Wipe to INIT", "Todos los huecos del banco abierto en INIT."),
    R("Tidy Wavetables", "Quita del banco las tablas que nadie usa."),
    R("Reorder by Category", "Ordena los huecos por categoría."),
    R("Apply Editor's Macros", "Aplica las macros del editor a todos los presets."),
    DANGER("BANCOS",
           "Cargar un banco y mandarlo a la FM-1, o Wipe to INIT, sustituye tus huecos. Antes: LIBRARY, Get from "
           "FM-1, y Save Bank."),
    H("LIBRARY"),
    P("Los 128 huecos de la FM-1 en 8 filas de 16, por colores de categoría. Get from FM-1 lo trae todo; Send to FM-1 "
      "manda solo los cambios. Arrastra para ordenar. Desde aquí se restaura el preset de fábrica de un hueco."),
    H("Otros"),
    R("MIDI Learn", "Clic derecho en cualquier mando. El mapa se queda en la FM-1."),
    R("Macros", "Arrastra una macro sobre un mando para asignarla."),
    R("Secuencias", "En SEQ, clic derecho en la tecla de una secuencia: CLEAR la vacía."),
    N("IMPORTAR DE SERUM 1",
      "Un .fxp es un punto de partida: SHRUB tiene un filtro, 8 notas y siete efectos. Lee los avisos al importar y "
      "escucha antes de guardar."),
    H("Qué archivo usar"),
    R(".shrub", "Un sonido con sus 27 secuencias, fuera de la FM-1."),
    R(".shrubbank", "La copia de seguridad entera."),
    R(".fxp", "Serum 1, para importar."),
    R(".SerumPreset", "Serum 2, solo exportar."),
    R(".fwsc", "Un paquete de firmware, para instalar una versión concreta."),
};

constexpr Block kMidiBlocks[] = {
    P("SHRUB recibe MIDI por tres sitios y manda por dos. GLOBAL, pág. 3: MIDI CH en OMNI escucha todos los canales."),
    H("Entrada"),
    R("USB", "Notas, controladores, reloj, cambio de programa y NRPN desde el ordenador."),
    R("MIDI IN (minijack)", "Lo mismo desde un teclado o una caja con salida MIDI."),
    R("Bluetooth", "Notas y reloj, con BLUETOOTH en ON o LEAD."),
    H("Salida"),
    R("USB", "Las notas de las teclas, el latch como CC 64, y reloj con arranque y parada."),
    R("Bluetooth", "Las notas que tocas. En LEAD, además reloj, arranque, parada y posición."),
    R("Minijack", "No hay salida MIDI por minijack."),
    H("Qué entiende"),
    B("Notas con velocity, pitch bend (BEND RANGE en GLOBAL), rueda, presión y sustain."),
    B("Cambio de programa: elige el hueco."),
    B("369 NRPN: cada parámetro automatizable tiene el suyo, para llegar a todo desde un DAW."),
    B("MPE: MIDI CH en MPE y el controlador en la zona baja. Cada nota lleva su bend (±48 st); su presión mueve su "
      "volumen y su CC 74 su posición de tabla. Filtro y efectos son comunes a todas."),
    H("Los CC de fábrica más útiles"),
    R("CC 74 y 71", "CUTOFF y RES."),
    R("CC 16 a 19", "MACRO 1 a 4."),
    R("CC 20 y 21", "WT POS de OSC A y OSC B."),
    R("CC 73, 75, 78, 72", "ENV 1: ATTACK, DECAY, SUSTAIN, RELEASE."),
    R("CC 76, 77, 86, 87", "RATE de los LFO 1 a 4."),
    R("CC 91, 94, 93", "Mezcla de reverb, delay y chorus."),
    R("CC 5 y 7", "GLIDE y LEVEL del preset."),
    R("CC 120, 123, 121", "Apagar todo, todas las notas, reiniciar controladores."),
    H("Audio por USB"),
    P("La FM-1 es también una tarjeta de sonido USB. USB LEVEL (GLOBAL, pág. 3): MASTER sigue el volumen; FIXED va a "
      "tope; SPK OFF calla el altavoz y la salida."),
    H("Dos FM-1 a la vez"),
    P("BLUETOOTH en ON en una y LEAD en la que manda. La primera vez, la FM-1 se reinicia una vez. LINK dice VISIBLE, "
      "SEARCHING, CONNECTED o LEADS. Si no se encuentran, LINK a la derecha prueba una calibración genérica al "
      "reiniciar; a la izquierda, vuelve a la suya."),
};

constexpr Block kInstallBlocks[] = {
    P("SHRUB se instala desde el navegador, sin programas. Sustituye el firmware de M-VAVE, y se puede volver a él."),
    N("TU UNIDAD",
      "Las fuentes no hablan de revisiones del hardware ni de cargadores de arranque alternativos. Si tu FM-1 es "
      "reciente o lleva un bootloader de rescate instalado, sigue también lo que diga quien lo hizo, y no des por "
      "hecho que todo lo de aquí se comporta igual."),
    H("Antes de empezar"),
    B("Copia de seguridad: en el editor, LIBRARY, Get from FM-1, y Bank, Save Bank."),
    B("Batería cargada o la FM-1 enchufada, y un cable USB de datos que funcione."),
    B("Cierra los demás programas que usen MIDI: un DAW, otro editor."),
    H("Instalar"),
    PANEL(cScreen, "La pantalla dice SHRUB UPDATE mientras instala."),
    S("1", "Conecta la FM-1 por USB y enciéndela."),
    S("2", "Abre shrub.audio/editor/install en Chrome o Edge."),
    S("3", "«Use the SHRUB on this site» para la última versión, o «Choose package (.fwsc)» para una concreta."),
    S("4", "Pulsa Install y permite el acceso MIDI cuando lo pida el navegador."),
    S("5", "Unos dos minutos sin tocar el cable. Se reinicia sola al acabar."),
    DANGER("NO DESCONECTES",
           "Cortar el cable o la batería durante SHRUB UPDATE deja la instalación a medias. Según el manual, se "
           "retoma lanzando el instalador otra vez; un desarrollador ha avisado de que un fallo puede dejar la "
           "unidad sin arrancar."),
    H("Si sale mal"),
    R("Se corta a mitad", "Se queda en modo actualización aunque la apagues. Lanza el instalador y continúa."),
    R("No llegó a escribir", "Vuelve sola a su firmware anterior en un minuto, más o menos."),
    R("No la encuentra", "Cierra otros programas MIDI; prueba otro cable o puerto."),
    R("Editor y firmware no casan", "Actualiza el firmware o recarga el editor."),
    H("Desde otro firmware"),
    P("Si la FM-1 lleva un firmware que el instalador no reconoce, como uno de la comunidad, vuelve antes a M-VAVE "
      "V15 o V16 con su programa M-Upgrade, y luego instala SHRUB."),
    H("Volver a M-VAVE"),
    P("Descarga el paquete oficial de la web de M-VAVE y pásalo con M-Upgrade. Guarda antes tu banco de SHRUB "
      "desde el editor."),
};

constexpr Block kRescueBlocks[] = {
    H("Rescate por USB"),
    PANEL(cOct, "OCT- y OCT+ mantenidos al encender: rescate por USB."),
    S("1", "Conecta el cable USB."),
    S("2", "Mantén OCT- y OCT+ mientras enciendes, y un segundo más."),
    S("3", "KEEP HOLDING OCT- AND OCT+ FOR USB RECOVERY, y luego USB RECOVERY."),
    S("4", "Lanza el instalador de SHRUB otra vez, o M-Upgrade para volver a M-VAVE."),
    N("SIN GARANTIAS",
      "El manual presenta este camino como la vuelta atrás, pero no promete nada para todas las unidades. Si tu FM-1 "
      "tiene un cargador de rescate propio, ese es otro camino distinto: el de quien lo instaló."),
    H("Si se cuelga"),
    R("Se congela", "El vigilante la reinicia a los 8 s; GLOBAL dirá WATCHDOG RESET."),
    R("Pantalla roja", "Un informe del fallo, se reinicia a los 10 s. Haz una foto y mándala al autor."),
    R("CORE 2 FROZE", "Pon CPU MHZ en 320 (GLOBAL, pág. 5)."),
    R("GLO al encender", "Apaga o enciende el segundo núcleo: otra salida si el fallo viene de ahí."),
    H("Si falla al arrancar"),
    S("1", "Primer fallo seguido: arranca sin tus presets."),
    S("2", "Segundo: SAFE START, sin leer nada guardado."),
    S("3", "Tercero: entra sola en el rescate por USB."),
    P("Si vuelve a arrancar bien, guarda tu banco desde el editor cuanto antes."),
    H("Bloqueo infantil"),
    P("Si solo responden las teclas, las octavas y PRESETS, está puesto el bloqueo infantil. Mantén HOME al encender "
      "para quitarlo."),
};

constexpr Block kSourcesBlocks[] = {
    P("Esta guía es para SHRUB V1.39. Revisada el 27 de noviembre de 2026."),
    H("Fuentes"),
    R("Manual", "shrub.audio/guide: Operation Guide 1.0, para el firmware V1.39."),
    R("Editor", "shrub.audio/editor, versión del 25-11-2026 (df37609): menús, nombres y rangos."),
    R("Emulador", "shrub.audio/play: las páginas de la pantalla, vistas una a una."),
    R("Instalador", "shrub.audio/editor/install: los pasos y los avisos."),
    H("Cómo se comprobó"),
    B("Nombres de páginas, mandos y valores: leídos en el motor del editor y en el emulador."),
    B("Valores de INIT: los del motor del editor."),
    B("Combinaciones de botones: solo las que da el manual."),
    B("Recetas del laboratorio: montadas con nombres y rangos reales, pero no escuchadas. Por eso dicen "
      "«orientativa»."),
    H("Lo que no se ha podido comprobar"),
    B("Cómo suena cada receta en una FM-1 de verdad."),
    B("Cuántas notas caben con cada sonido: el navegador no limita la CPU como la FM-1."),
    B("Si SEQ + OCT- + OCT+ borra una secuencia o las 27."),
    B("La página del DAMP de la reverb."),
    B("Nada sobre revisiones de hardware o cargadores de arranque alternativos."),
    B("No había lista de cambios oficial ni acceso al foro de la comunidad."),
    N("SI NO CASA",
      "Si algo de aquí no coincide con tu FM-1, manda la pantalla de tu unidad y su versión: puede haber cambiado en "
      "una versión nueva."),
};

}  // namespace

const Chapter kEditor = FMGUIDE_CHAPTER("El editor", kEditorBlocks);
const Chapter kMidi = FMGUIDE_CHAPTER("MIDI, USB y Bluetooth", kMidiBlocks);
const Chapter kInstall = FMGUIDE_CHAPTER("Instalar y volver", kInstallBlocks);
const Chapter kRescue = FMGUIDE_CHAPTER("Rescate y fallos", kRescueBlocks);
const Chapter kSources = FMGUIDE_CHAPTER("Versión y fuentes", kSourcesBlocks);

}  // namespace shrub
}  // namespace fmguide

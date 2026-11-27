#include "GuideData.h"
#include "GuideShrubChapters.h"
#include "SynthFigures.h"

// SHRUB, COMO HAGO: answers by what the reader wants to do, each enough to do
// it, with a link to the chapter that explains it. Checked on 27 November 2026
// (see GuideShrub.cpp).

namespace fmguide {
namespace shrub {

namespace {

using namespace synthfig;

constexpr Block kHowSoundBlocks[] = {
    H("Que el sonido evolucione solo"),
    P("Mueve algo con un LFO lento o con el caos. LFO, LFO 1: pág. 2 BPM OFF y MODE FREE; pág. 1 RATE muy bajo (0.10 "
      "Hz), DEST A WT POS y AMOUNT +30 %. Para algo que nunca se repite: LFO, CHAOS, y en la matriz SOURCE CHAOS 1X "
      "con el destino que quieras."),
    LINK(Mod, "Envolventes, LFO y matriz"),
    H("Un sonido más grueso"),
    P("Unison: EDIT, OSC A, pág. 2, VOICES 3 a 7 y DETUNE 15 a 30 %. Suma OSC B una octava abajo (pág. 3, OCT -1) y el "
      "SUB. Un poco de DISTORTION o el COMPRESSOR con OTT terminan de engordarlo. Más unison, menos notas a la vez."),
    LINK(LabSupersaw, "Receta: supersaw"),
    H("Más ancho en estéreo"),
    P("WIDTH de cada oscilador (pág. 2) a +100 % con VOICES de 2 o más. Además: el FILTER con STEREO alto (pág. 2), el "
      "CHORUS, el HYPER/DIM, el DELAY en PING-PONG y la REVERB con WIDTH alto. Los graves, mejor estrechos: déjalos "
      "en el SUB con DIRECT."),
    H("Que las notas se deslicen"),
    P("GLO, VOICING: MONO en MONO, LEGATO en ON y GLIDE a tu gusto (unos 50 a 150 ms). Liga las notas: pisa la nueva "
      "antes de soltar la anterior. Con ALWAYS en ON se deslizan también sin ligar."),
    FIG(Glide, "GLIDE es el tiempo del deslizamiento; con LEGATO solo pasa al ligar."),
    H("Asignar una macro"),
    S("1", "ENV, MATRIX. KNOB 1: una conexión libre."),
    S("2",
      "KNOB 2, SOURCE: MACRO 1. KNOB 3, DEST: lo que quieras mover (CUTOFF, por ejemplo). KNOB 4, AMOUNT: cuánto."),
    S("3", "HOME y otra vez HOME: los KNOB son ahora MACRO 1 a 4. Gira el KNOB 1."),
    P("Una macro puede tener varios destinos: repite con otra conexión y la misma MACRO 1. En el editor también puedes "
      "arrastrar una macro sobre un mando."),
    H("Dibujar mi propio LFO"),
    S("1", "LFO, LFO 1, y con SELECT hasta la pág. 5: POINT, X, Y, CURVE."),
    S("2", "Mantén LFO y pulsa OCT+ para añadir un punto (hasta 17) u OCT- para quitar el elegido."),
    S("3", "KNOB 1 elige el punto; KNOB 2 y 3 lo mueven; KNOB 4 curva el tramo."),
    P("En el editor se dibuja con el ratón: doble clic añade o quita puntos y Shift arrastrando pinta escalones."),
    LINK(LabWobble, "Receta: wobble a tempo"),
};

constexpr Block kHowPlayBlocks[] = {
    H("Que una tecla toque un acorde"),
    S("1", "SEL (SCL), pág. 1: KEY con tu tónica (no --) y SCALE con tu escala."),
    S("2", "CHORD: el tipo, por ejemplo TRIAD. Cada tecla blanca toca el acorde de ese grado."),
    S("3", "Con CHORD encendido, las teclas negras cambian el tipo de acorde al vuelo."),
    KEYS(kAllBlack, "Las once negras eligen ROOT, 3RD, TRIAD, 7TH, 9TH, SUS2, SUS4, 6TH, POWER, OPEN u OCTAVE."),
    H("Mantener un arpegio sin las teclas"),
    P("Dos formas. ARP, pág. 2: LATCH ON, y el arpegio sigue al soltar hasta que toques otro acorde. O el latch "
      "general: OCT- y OCT+ a la vez dejan sonando todo lo que sueltes, como un pedal; otra vez, se para."),
    H("Grabar una melodía en bucle"),
    S("1", "Ajusta el TEMPO en GLOBAL, pág. 1. Si quieres, CLICK en REC: metrónomo al grabar."),
    S("2", "Pulsa REC, espera la cuenta (IN 4 a IN 1) y toca, una nota a la vez."),
    S("3", "REC o PLAY para terminar. PLAY la repite en bucle. El largo de la pista se cambia en SEQ con ALGORITHM."),
    P("Para escribirla nota a nota sin prisa: parado, SEQ + REC, y cada tecla un paso."),
    LINK(Seq, "El secuenciador"),
    H("Grabar el movimiento de un mando"),
    P("Pulsa REC y, tras la cuenta, gira el mando mientras suena. La toma de ese mando acaba cuando dejas de girarlo. "
      "Queda en una pista y se repite en bucle; en SEQ la ves como una curva."),
    FIG(Automation, "El movimiento grabado, como puntos unidos en una pista."),
    H("Varias secuencias en un preset"),
    P("Hay 27, una por tecla. Mantén SEQ y pulsa una tecla para elegir la secuencia; graba en ella. Para empezar desde "
      "otra que ya tengas, mantén SEQ, mantén la tecla de esa secuencia y pulsa la de una vacía: la copia."),
    H("Cambiar de secuencia tocando en directo"),
    P("Pulsa SEQ dos veces seguidas: cada tecla elige su secuencia. Sonando, cambia en el compás siguiente; con SEQ "
      "mantenido, KNOB 4 SWITCH lo cambia a BEAT o 1/16. Pulsa SEQ dos veces, o una tecla de octava, para volver a "
      "tocar notas."),
    H("Una textura generativa"),
    P("Escala en SCALE, arpegiador en RANDOM o RAND DRIFT con CHANCE por debajo del 100 % y LATCH, azar por nota en "
      "VOICING (pág. 2) y el caos moviendo la tabla y el filtro. Añade delay y reverb largos."),
    LINK(LabAmbient, "Receta: textura ambiental generativa"),
};

constexpr Block kHowFilesBlocks[] = {
    H("Importar presets de Serum"),
    P("En el editor (shrub.audio/editor): File, Open Patch or Serum 1 Preset, y elige uno o varios .fxp de Serum 1. O "
      "File, Browse Serum 1 Factory Presets: los de fábrica de Serum 1, sin tener Serum. Luego File, Save to FM-1 User "
      "Presets, con la FM-1 conectada. Lee los avisos que da al importar: un solo filtro, 8 notas y siete efectos "
      "cambian algunos sonidos."),
    N("SERUM 2",
      "Los presets de Serum 2 no se pueden importar. Al revés sí: File, Export to Serum2 crea un .SerumPreset."),
    H("Añadir mis wavetables"),
    P("En el editor: File, User Wavetables. Acepta WAV de Serum o con 2048 muestras por fotograma, hasta 64 "
      "fotogramas. Caben tres bancos de 16 tablas. Envíalas a la FM-1 desde ese diálogo o desde LIBRARY, y salen al "
      "final de la lista de TABLE."),
    H("Recuperar un preset de fábrica"),
    P("Desde el editor, con la FM-1 conectada: LIBRARY, Get from FM-1, elige el hueco y restaura su preset de fábrica. "
      "Luego Send to FM-1. Para dejarlo todo como venía: el banco Default Shrub."),
    DANGER("DEFAULT SHRUB",
           "Cargar el banco Default Shrub y enviarlo pone cada hueco en su preset de fábrica y quita tus tablas: "
           "guarda antes tu banco con Save Bank."),
    H("Sincronizar dos FM-1 con SHRUB"),
    S("1", "En las dos: GLO, GLOBAL, pág. 4, BLUETOOTH. Una en ON y la que manda el tempo en LEAD."),
    S("2", "La primera vez que enciendes el Bluetooth, la FM-1 se reinicia una vez."),
    S("3", "LINK enseña el estado: VISIBLE, SEARCHING, CONNECTED o LEADS, con el final del nombre de la otra."),
    P("La que está en LEAD manda reloj, arranque, parada y posición. Si no se encuentran, LINK girado a la derecha "
      "prueba una calibración genérica al reiniciar."),
    H("Conectar un teclado MIDI"),
    P("Por USB (si el teclado hace de anfitrión), por el minijack MIDI IN o por Bluetooth. GLO, GLOBAL, pág. 3: MIDI "
      "CH en OMNI escucha todos los canales. El teclado trae velocity, rueda, presión y pitch bend; BEND RANGE fija su "
      "alcance. Para MPE, MIDI CH en MPE."),
    LINK(Midi, "MIDI, USB y Bluetooth"),
    H("Grabar el audio en el ordenador"),
    P("La FM-1 manda su audio por USB. Grábalo como una entrada más en tu programa. GLOBAL, pág. 3, USB LEVEL: MASTER "
      "sigue el volumen; FIXED lo deja siempre a tope; SPK OFF calla el altavoz para que no lo oigas doble. Para una "
      "sola nota, el editor tiene File, Render a Note to Audio (.wav)."),
    H("Una copia de seguridad completa"),
    S("1", "Abre el editor, Connect FM-1."),
    S("2", "LIBRARY, Get from FM-1: trae los 128 huecos."),
    S("3", "Bank, Save Bank: un .shrubbank con los 128 huecos, sus tablas y el mapa MIDI."),
    P("Un sonido suelto, con sus pistas: File, Save Patch to Disk (.shrub). Un enlace de File, Share This Preset no "
      "lleva tus tablas propias."),
};

constexpr Block kHowTroubleBlocks[] = {
    H("Desaparecen notas en los acordes"),
    P("Caben hasta 8 notas, y menos cuanto más unison tenga el sonido. Mira GLO, VOICING: POLY dice cuántas caben. "
      "Baja "
      "VOICES en los osciladores, quita el unison de OSC B, o pon QUALITY en DRAFT (GLOBAL, pág. 5). Con CPU MHZ en "
      "320 caben un 10 % menos."),
    FIG(VoicePool, "Las voces son compartidas: un unison grande gasta muchas por nota."),
    H("No se guardan todas mis secuencias"),
    P("Cada hueco tiene alrededor de 1 KB para el sonido y sus 27 secuencias. Si no cabe, SAVE dice SAVED, THINNED "
      "(suaviza las pistas) o SAVED, NO LANES (las deja fuera). Usa menos pistas o más cortas, o graba con menos "
      "detalle. Fuera de la FM-1, el editor guarda el sonido con todas sus pistas en un .shrub."),
    H("No suena nada"),
    B("MASTER, y LEVEL del preset (GLOBAL, pág. 1)."),
    B("USB LEVEL en SPK OFF calla el altavoz y la salida a propósito."),
    B("En INIT, OSC B y el SUB están a 0 %; ajusta LEVEL de OSC A."),
    B("Child lock: si solo responden teclas, OCT y PRESETS, mantén HOME al encender."),
    H("CUTOFF no hace nada"),
    P("EDIT, FILTER, pág. 3: lo que esté en OFF no pasa por el filtro. En INIT, todo está en OFF."),
    H("Notas colgadas"),
    P("Manda CC 120 o CC 123 desde el ordenador, o pulsa el botón de pánico del editor. Comprueba también que no "
      "tienes el latch (OCT- y OCT+) o el LATCH del arpegio encendidos."),
    H("Un filtro comb o flanger se queda mudo"),
    P("La reverb HALL comparte memoria con esos filtros. Pon la reverb en PLATE o apágala."),
    H("Mensajes de error"),
    LINK(RefMessages, "Qué significa cada mensaje"),
    LINK(Rescue, "Rescate y fallos al arrancar"),
};

}  // namespace

const Chapter kHowSound = FMGUIDE_CHAPTER("El sonido", kHowSoundBlocks);
const Chapter kHowPlay = FMGUIDE_CHAPTER("Tocar y secuenciar", kHowPlayBlocks);
const Chapter kHowFiles = FMGUIDE_CHAPTER("Archivos y conexiones", kHowFilesBlocks);
const Chapter kHowTrouble = FMGUIDE_CHAPTER("Cuando algo no va", kHowTroubleBlocks);

}  // namespace shrub
}  // namespace fmguide

#include "GuideData.h"
#include "GuideShrubChapters.h"
#include "SynthFigures.h"

// SHRUB, LABORATORIO: pads, strings, plucks, an arpeggio, bells and a
// wobble. Every page, knob and value named here exists on V1.39 with that
// name and range (checked in the editor's engine on 27 November 2026); the
// results were not heard, and each recipe says so.

namespace fmguide {
namespace shrub {

namespace {

using namespace synthfig;

constexpr Block kCinemaBlocks[] = {
    P("Una alfombra lenta que entra despacio, cambia sin parar y deja una cola larga: el fondo de una banda sonora. "
      "Aquí todo se mueve a la vez, pero cada cosa a su ritmo."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Ataque y release largos, LFO lento a la tabla, caos al filtro y reverb grande."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    S("1",
      "EDIT, OSC A, pág. 1: ALGORITHM hasta SAW>SQUARE, una tabla que pasa de sierra a cuadrada. Pág. 2: VOICES 4, "
      "DETUNE 20 %, WIDTH +100 %."),
    S("2", "EDIT, OSC B: WT POS 1 (seno), LEVEL 40 %. Pág. 3: OCT +1. Un brillo suave por encima."),
    S("3", "EDIT, FILTER, pág. 3: OSC A y OSC B en ON. Pág. 1: MG LOW 12, CUTOFF unos 1500 Hz, RES 10 %."),
    S("4",
      "ENV, ENV 1: ATTACK unos 1.50 s, SUSTAIN 0.0 dB, RELEASE unos 3.00 s. Escucha: entra despacio y se queda al "
      "soltar."),
    S("5",
      "LFO, LFO 1: pág. 2, BPM OFF, MODE FREE. Pág. 1: RATE unos 0.10 Hz, DEST A WT POS, AMOUNT +30 %. La onda "
      "cambia cada diez segundos."),
    S("6",
      "LFO, CHAOS: CHAOS 1 unos 0.20 Hz. ENV, MATRIX: una conexión libre, SOURCE CHAOS 1X, DEST CUTOFF, AMOUNT +10 "
      "%. El brillo sube y baja sin repetirse."),
    S("7", "FX, rack: hueco 2 CHORUS en ON (MIX 30 %), hueco 3 DELAY en NORMAL (1/4, FEEDBACK 35 %, MIX 15 %)."),
    S("8", "Hueco 5 REVERB en HALL. En sus mandos: SIZE 80 %, DECAY 70 %, MIX 35 %."),
    FIG(Adsr, "Un pad: Attack largo, Sustain alto y Release largo. La nota tarda en llegar y en irse."),
    N("HALL Y FILTRO",
      "Esta receta usa MG LOW 12. Si cambias a un filtro comb, FLANGE o REVERB, la HALL puede silenciarlo: usa PLATE."),
    H("Variaciones"),
    B("Mantén el acorde con OCT- y OCT+ a la vez y toca por encima."),
    B("RAND CUT y RAND PAN en VOICING, pág. 2: cada nota algo distinta."),
    B("Compara con los presets 45 ETHEREAL COMPASS y 56 SLOW PAD."),
    H("Guardar"),
    S("1", "SAVE. Nombre CINEMA PAD, categoría PAD. SAVE."),
};

constexpr Block k80sBlocks[] = {
    P("El pad de los polifónicos de principios de los 80, del tipo Juno: una onda de pulso que cambia de ancho poco a "
      "poco, un filtro suave y un chorus que lo ensancha todo."),
    R("Nivel", "Básico."),
    R("Aprendes", "Modulación de ancho de pulso (PWM) y el chorus como parte del sonido."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(Warp, "WARP deforma el ciclo. PWM estrecha una parte; moverlo con un LFO da el vaivén clásico."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 1: WT POS 4 (cuadrada). WARP PWM, AMOUNT 30 %."),
    S("2",
      "LFO, LFO 1: pág. 2, BPM OFF, MODE FREE. Pág. 1: RATE unos 0.40 Hz, DEST A WARP, AMOUNT +40 %. Escucha: el "
      "sonido se mueve como si respirara."),
    S("3", "EDIT, FILTER, pág. 3: OSC A en ON. Pág. 1: MG LOW 24, CUTOFF unos 1800 Hz, RES 15 %. Pág. 2: ENV 2 +15 %."),
    S("4", "ENV, ENV 2: ATTACK unos 400 ms, DECAY unos 2.00 s, SUSTAIN -12.0 dB."),
    S("5", "ENV, ENV 1: ATTACK unos 300 ms, RELEASE unos 800 ms."),
    S("6", "FX, rack, hueco 2 (CHORUS): MODE ON. En sus mandos: RATE unos 0.5 Hz, DEPTH unos 6.0 ms, MIX 50 %."),
    S("7", "Hueco 5 (REVERB): MODE PLATE, MIX 20 %."),
    H("Variaciones"),
    B("Más Juno: la tabla PWM JUNO en lugar de BASIC SHAPES."),
    B("Un sub sin filtrar: SUB/NOISE, SHAPE SQUARE, OCT -1, SUB 30 %."),
    B("El editor trae el banco Juneau: 80 sonidos en este estilo. Y está el preset 98 SOLINA."),
    H("Guardar"),
    S("1", "SAVE. Nombre JUNO PAD, categoría PAD. SAVE."),
};

constexpr Block kStringsBlocks[] = {
    P("Las cuerdas de string machine de los 70 y 80: sierras desafinadas, ataque suave, ensemble y un vibrato que "
      "llega tarde, cuando la nota ya lleva un rato sonando."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Modulación auxiliar: una envolvente que decide cuánto actúa un LFO."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(Matrix, "El LFO hace el vibrato; AUX (ENV 3) lo deja pasar poco a poco al mantener la nota."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 2: VOICES 3, DETUNE 25 %."),
    S("2", "EDIT, OSC B: WT POS 2 (sierra), LEVEL 35 %. Pág. 3: OCT +1."),
    S("3", "EDIT, FILTER, pág. 3: OSC A y OSC B en ON. Pág. 1: MG LOW 12, CUTOFF unos 3000 Hz, RES 0 %."),
    S("4", "ENV, ENV 1: ATTACK unos 250 ms, SUSTAIN 0.0 dB, RELEASE unos 600 ms."),
    S("5", "LFO, LFO 2: pág. 2, BPM OFF, MODE FREE. Pág. 1: RATE unos 5.0 Hz."),
    S("6", "ENV, ENV 3: ATTACK unos 800 ms, SUSTAIN 0.0 dB."),
    S("7",
      "ENV, MATRIX, pág. 1: una conexión libre, SOURCE LFO 2, DEST MAST TUNE, AMOUNT +3 %. Pág. 3: AUX ENV 3, MODE x "
      "AUX. Escucha: la nota empieza recta y el vibrato aparece al mantenerla."),
    S("8", "FX, rack: CHORUS en ON (MIX 35 %) y REVERB en HALL (MIX 25 %)."),
    N("POR QUE AUX",
      "SHRUB no tiene el rise ni el delay de los LFO de Serum. AUX lo sustituye: la cantidad de LFO se multiplica por "
      "lo que valga ENV 3 en cada momento."),
    H("Variaciones"),
    B("Más orquestal: la tabla SOLINA VIOLA en OSC A."),
    B("Toca acordes en una sola tecla: SCALE, CHORD TRIAD, con tu tónica y escala."),
    B("Compara con los presets 38 REALISTIC STRING y 48 MENTHOL STRINGS."),
    H("Guardar"),
    S("1", "SAVE. Nombre STRINGS, categoría PAD. SAVE."),
};

constexpr Block kPluckBlocks[] = {
    P("Un golpe corto y brillante que se apaga solo, como una cuerda pulsada sintética. Aparece en el pop, el house y "
      "cualquier melodía rápida."),
    R("Nivel", "Básico."),
    R("Aprendes", "Envolventes cortas sin sustain, el filtro que se cierra con la nota y el delay como eco rítmico."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    S("1", "EDIT, OSC B: WT POS 4 (cuadrada), LEVEL 30 %. Pág. 3: OCT +1."),
    S("2",
      "EDIT, FILTER, pág. 3: OSC A y OSC B en ON. Pág. 1: MG LOW 24, CUTOFF unos 400 Hz, RES 25 %. Pág. 2: ENV 2 +55 "
      "%."),
    S("3", "ENV, ENV 2: ATTACK 0.0 ms, DECAY unos 180 ms, SUSTAIN -inf dB, RELEASE unos 120 ms."),
    S("4",
      "ENV, ENV 1: ATTACK 0.5 ms, DECAY unos 400 ms, SUSTAIN -inf dB, RELEASE unos 300 ms. Escucha: un «plic» que se "
      "apaga aunque mantengas la tecla."),
    S("5", "FX, rack, hueco 3 (DELAY): MODE PING-PONG. En sus mandos: TIME L y R 1/8., FEEDBACK 35 %, MIX 25 %."),
    S("6", "Hueco 5 (REVERB): MODE PLATE, MIX 20 %."),
    FIG(Adsr, "Un pluck: Attack nulo, Sustain a cero. Solo cuenta el Decay."),
    H("Variaciones"),
    B("Más cuerda: el filtro comb (COMB +) con CUTOFF hacia los 1000 Hz y RES alta."),
    B("Más digital: OSC A con la tabla NICE PLUGGER."),
    B("Compara con los presets 58 a 60 (PLUCK LO-FI, STR y BLUR HAWK)."),
    H("Guardar"),
    S("1", "SAVE. Nombre PLUCK, categoría PLUCK. SAVE."),
};

constexpr Block kBerlinBlocks[] = {
    P("La secuencia hipnótica de la escuela de Berlín, la de Tangerine Dream: un arpegio rápido y repetitivo sobre un "
      "acorde, con el filtro abriéndose y cerrándose a mano durante minutos. Aquí además se graba ese movimiento."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Arpegiador con LATCH, delay a tempo y grabar el movimiento de un mando en una pista."),
    R("Parte de", "Tu PLUCK, o un hueco INIT con los pasos 1 a 4 de esa receta."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    LINK(LabPluck, "Receta del pluck"),
    FIG(Arp, "El arpegiador toca el acorde nota a nota. Con LATCH sigue solo."),
    H("Paso a paso"),
    PANEL(cArp | cAlgorithm | cKnobs | cRec | cPlay, "ARP y sus KNOB; REC para grabar el movimiento, PLAY para parar."),
    S("1", "GLO, GLOBAL, pág. 1: TEMPO 110 BPM."),
    S("2",
      "ARP, ARP otra vez (encendido). ALGORITHM: UP/DOWN. Pág. 1: RATE 1/16, GATE 50 %, OCTAVES 2. Pág. 2: LATCH "
      "ON."),
    S("3", "Toca un acorde menor de tres notas y suelta: sigue sonando."),
    S("4", "FX, DELAY: TIME L 1/8., TIME R 1/4, FEEDBACK 40 %, MIX 25 %. Los ecos rellenan los huecos."),
    S("5",
      "Pulsa REC. Tras la cuenta, ve a EDIT, FILTER y gira CUTOFF despacio, arriba y abajo, durante unos compases. "
      "Pulsa REC para terminar."),
    S("6", "SEQ: verás la pista con la curva del CUTOFF. Se repetirá sola."),
    FIG(Automation, "Lo que hiciste con CUTOFF queda como una curva en una pista, y se repite en bucle."),
    H("Variaciones"),
    B("RES más alta mientras grabas: el barrido canta más."),
    B("Graba un segundo mando, por ejemplo DECAY de ENV 2, con un largo de pista distinto: se irán desfasando."),
    B("Patrones PLAYED o CONVERGE para variar el orden de las notas."),
    B("Compara con los presets 71 CLASSIC ARP y 77 DOWNUNDER."),
    H("Guardar"),
    S("1", "SAVE. Nombre BERLIN SEQ, categoría SEQ. Se guarda con su pista. SAVE."),
};

constexpr Block kBellsBlocks[] = {
    P("Campanas, vibráfonos y sonidos metálicos salen de modular un oscilador con otro a una relación de afinación que "
      "no es exacta. Es lo que hacía el DX7, y SHRUB lo tiene como warp: FM."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Modulación de frecuencia con un oscilador mudo, relaciones inarmónicas y una envolvente al warp."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 1: WT POS 1 (seno)."),
    S("2",
      "EDIT, OSC B, pág. 1: WT POS 1 (seno), LEVEL 0 %. Pág. 3: OCT +1, SEMI +10: unas 3,5 veces la frecuencia de A, "
      "una relación que no es exacta. OSC B no suena: solo modula."),
    S("3",
      "EDIT, OSC A, pág. 1: WARP FM (B), AMOUNT 40 %. Escucha: el seno se vuelve metálico. Prueba otros SEMI en OSC "
      "B y quédate con el que más te guste."),
    S("4", "ENV, ENV 1: ATTACK 0.0 ms, DECAY unos 2.00 s, SUSTAIN -inf dB, RELEASE unos 2.00 s."),
    S("5",
      "ENV, MATRIX: una conexión libre, SOURCE ENV 2, DEST A WARP, AMOUNT +30 %. ENV, ENV 2: DECAY unos 600 ms, "
      "SUSTAIN -inf dB. El golpe es brillante y la cola se dulcifica, como una campana real."),
    S("6", "FX, rack, hueco 5 (REVERB): MODE HALL, MIX 30 %."),
    H("Variaciones"),
    B("Más metal y menos campana: WARP RM (B) en vez de FM (B)."),
    B("Gong: OSC A en OCT -1, DECAY de ENV 1 a 6.00 s y SEMI de OSC B en +6."),
    B("Compara con los presets 61 BELL CASCADE, 64 BIG BELLS y 99 VINTAGE BELLS."),
    H("Guardar"),
    S("1", "SAVE. Nombre FM BELL, categoría PLUCK. SAVE."),
};

constexpr Block kWobbleBlocks[] = {
    P("El bajo que «habla» del dubstep: un LFO sincronizado al tempo abre y cierra el filtro en figuras rítmicas. "
      "Cambiar la figura del LFO cambia el ritmo del bajo."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "LFO sincronizado con BPM, RETRIG y una macro para controlar la velocidad en directo."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(LfoShapes, "La forma del LFO es la forma del wobble: TRIANGLE es suave; SAW DOWN golpea y cae."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 3: OCT -1. EDIT, SUB/NOISE: SUB 40 %, OCT -1."),
    S("2", "EDIT, FILTER, pág. 3: OSC A en ON. Pág. 1: MG LOW 24, CUTOFF unos 200 Hz, RES 40 %."),
    S("3",
      "LFO, LFO 1: pág. 2, BPM ON, MODE RETRIG. Pág. 1: RATE 1/8, DEST CUTOFF, AMOUNT +60 %. Escucha: «uau uau» a "
      "corcheas."),
    S("4", "GLO, VOICING: MONO."),
    S("5", "FX, rack, hueco 1 (DISTORTION): MODE TUBE, DRIVE 40 %, MIX 50 %."),
    S("6",
      "Una macro para la velocidad: ENV, MATRIX, una conexión libre, SOURCE MACRO 1, DEST LFO1 RATE, AMOUNT +50 %. "
      "HOME, HOME otra vez: KNOB 1 es MACRO 1."),
    H("Variaciones"),
    B("RATE 1/8 t o 1/16 para otros ritmos; cámbialo mientras grabas con REC."),
    B("SKEW en SAW DOWN: cada golpe abre de golpe y cierra despacio."),
    B("Mira el preset 19 SCREAM WOBBLE 01."),
    H("Guardar"),
    S("1", "SAVE. Nombre WOBBLE, categoría BASS. SAVE."),
};

}  // namespace

const Chapter kLabCinema = FMGUIDE_CHAPTER("Pad cinematográfico", kCinemaBlocks);
const Chapter kLab80s = FMGUIDE_CHAPTER("Pad de los 80", k80sBlocks);
const Chapter kLabStrings = FMGUIDE_CHAPTER("Cuerdas sintéticas", kStringsBlocks);
const Chapter kLabPluck = FMGUIDE_CHAPTER("Pluck melódico", kPluckBlocks);
const Chapter kLabBerlin = FMGUIDE_CHAPTER("Arpegio Tangerine Dream", kBerlinBlocks);
const Chapter kLabBells = FMGUIDE_CHAPTER("Campanas y metales", kBellsBlocks);
const Chapter kLabWobble = FMGUIDE_CHAPTER("Wobble a tempo", kWobbleBlocks);

}  // namespace shrub
}  // namespace fmguide

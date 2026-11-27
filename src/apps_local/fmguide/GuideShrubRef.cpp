#include "GuideData.h"
#include "GuideShrubChapters.h"
#include "SynthFigures.h"

// SHRUB, REFERENCIA: every button, combination and message, then every page
// of every part with its knobs and ranges. Names and ranges are the engine's
// own on V1.39 (read in the editor on 27 November 2026); combinations are the
// Operation Guide's, and nothing here is guessed.

namespace fmguide {
namespace shrub {

namespace {

using namespace synthfig;

constexpr Block kButtonsBlocks[] = {
    P("Lo que hace cada control con una pulsación corta. Pulsar un botón de sección otra vez pasa a su parte "
      "siguiente."),
    PANEL(cAll, "Todo el panel de la FM-1 con SHRUB."),
    H("Mandos"),
    R("MASTER", "Volumen general."),
    R("PRESETS", "Recorre los 128 huecos; carga al dejar de girar."),
    R("SELECT", "Pasa de página dentro de la parte."),
    R("ALGORITHM", "Elige el tipo: tabla, warp, filtro, patrón de arpegio, largo de pista..."),
    R("KNOB 1 a 4", "Los cuatro valores de la página."),
    H("Botones de sección"),
    R("EDIT", "OSC A, OSC B, SUB/NOISE, FILTER."),
    R("ENV", "ENV 1, ENV 2, ENV 3, MATRIX."),
    R("LFO", "LFO 1 a 4, CHAOS, MATRIX."),
    R("FX", "FX RACK y cada efecto del rack."),
    R("GLO", "VOICING y GLOBAL."),
    R("SEL (SCL)", "SCALE."),
    R("HOME", "Pantalla del preset; otra vez, las macros."),
    H("Botones de acción"),
    R("SAVE", "Abre el guardado; otra vez, guarda."),
    R("ARP", "Página del arpegio; otra vez, lo enciende o apaga."),
    R("SEQ", "Las pistas de la secuencia en curso."),
    R("PLAY", "Arranca o para el transporte."),
    R("REC", "Graba notas y movimientos."),
    R("OCT- y OCT+", "Una octava abajo o arriba."),
    R("Teclas", "27 teclas, de Fa a Sol. Tocan notas, o eligen secuencia con SEQ."),
};

constexpr Block kCombosBlocks[] = {
    P("Todas las combinaciones que documenta el manual de SHRUB. Las que borran algo van en recuadro negro. Si una "
      "combinación no está aquí, no está documentada: no la busques probando."),
    H("Pulsaciones dobles y largas"),
    R("HOME mantenido", "Medio segundo: marca el preset como favorito, o lo desmarca."),
    R("GLO mantenido", "Medio segundo: el arbusto. SELECT da un sonido al azar o una mezcla."),
    R("SEQ dos veces", "Las teclas eligen secuencia. Dos veces más, o una octava, vuelve a las notas."),
    R("Botón otra vez", "La parte siguiente de esa sección."),
    H("Combinaciones"),
    R("OCT- + OCT+", "Latch: las notas siguen al soltarlas. Otra vez, se paran."),
    R("EDIT, ENV o LFO + SELECT", "Salta entre las partes de la sección, en los dos sentidos."),
    R("LFO + OCT+", "En la pág. 5, añade un punto a la forma dibujada."),
    R("LFO + OCT-", "En la pág. 5, quita el punto elegido. Los extremos no se quitan."),
    R("PRESETS con cambios", "EDITED / TURN AGAIN: gira otra vez para irte sin guardar."),
    H("Con SEQ mantenido"),
    R("SEQ + tecla", "Elige la secuencia de esa tecla."),
    R("SEQ + tecla + tecla vacía", "Copia la primera secuencia en la segunda."),
    R("SEQ + REC, parado", "Escribir notas paso a paso (STEP)."),
    R("SEQ + SEL (SCL)", "TRANSPOSE SEQ: las teclas transponen la secuencia."),
    R("SEQ + KNOB 1 a 4", "SWING, OVERLAP, PARAMS y SWITCH."),
    DANGER("SEQ + OCT-", "Borra el punto seleccionado de la pista elegida."),
    DANGER("SEQ + OCT+", "Vacía la pista elegida."),
    DANGER("SEQ + OCT- + OCT+",
           "Mantenidos, la pantalla pide CLEAR ALL? KEEP HOLDING; si sigues, vacía todas las pistas. Si sueltas "
           "antes, no borra nada. El manual no dice si son las de una secuencia o las de las 27."),
    H("Al encender"),
    R("OCT- + OCT+", "Mantenidos con el USB conectado: rescate por USB."),
    R("GLO", "Apaga el segundo núcleo del chip, o lo vuelve a encender."),
    R("HOME", "Bloqueo infantil: solo teclas, octavas y PRESETS. Otra vez para quitarlo."),
    N("ANTES DEL BLOQUEO",
      "El bloqueo infantil deja el volumen donde esté: fíjalo antes, y guarda lo que estés editando."),
    H("Lo que no hay"),
    B("Borrar una secuencia entera o las 27 desde la FM-1: no documentado. En el editor, clic derecho en la tecla de "
      "la "
      "secuencia y CLEAR."),
    B("Borrar un preset: no hay. Se sobrescribe con otro, por ejemplo con un INIT."),
};

constexpr Block kMessagesBlocks[] = {
    P("Lo que puede escribir la pantalla, por orden de lo más corriente a lo más raro."),
    H("Al tocar y guardar"),
    R("EDITED / TURN AGAIN", "Has cambiado el sonido: girar PRESETS otra vez lo descarta."),
    R("SAVED", "Guardado."),
    R("SAVED, THINNED", "Ha suavizado las pistas para que quepan."),
    R("SAVED, NO LANES", "Guardado sin las pistas: no cabían."),
    R("AUX CUT / LFOS CUT / NO LFO 4", "Ha quitado algo de un sonido enorme para que quepa."),
    R("SAVE AGAIN: SOUND + LANES", "Pulsa SAVE otra vez para guardar el sonido con sus pistas."),
    R("NOT SAVED: STORAGE FULL", "La memoria está llena: nada guardado."),
    R("ARP OFF", "El arpegio está apagado: ARP otra vez lo enciende."),
    R("IN 4 ... IN 1", "La cuenta de un compás antes de grabar."),
    R("STEP", "Escritura de notas paso a paso."),
    R("CLEAR ALL? KEEP HOLDING", "Sigue manteniendo para borrar todas las pistas, o suelta para no borrar."),
    R("BAT LOW", "Batería por debajo del 30 %. Se quita por encima del 33 %."),
    H("Al instalar y rescatar"),
    R("SHRUB UPDATE", "Instalando. No toques el cable: unos dos minutos."),
    R("KEEP HOLDING OCT- AND OCT+", "Sigue manteniendo: va a entrar en el rescate por USB."),
    R("USB RECOVERY", "Lista para recibir el firmware desde el instalador."),
    H("Cuando algo falla"),
    R("WATCHDOG RESET", "Se había congelado y el vigilante la reinició a los 8 s. Sale en GLOBAL."),
    R("CORE 2 FROZE", "El segundo núcleo se atascó. Prueba CPU MHZ en 320 (GLOBAL, pág. 5)."),
    R("SAFE START", "Arranque sin leer nada guardado, tras dos fallos seguidos al encender."),
    R("Pantalla roja", "Un informe del fallo; se reinicia a los 10 s. Hazle una foto para el autor."),
    LINK(Rescue, "Rescate y fallos al arrancar"),
};

constexpr Block kParOscBlocks[] = {
    P("Las páginas de EDIT para OSC A y OSC B (iguales) y SUB/NOISE, con sus rangos. ALGORITHM elige la tabla en OSC y "
      "la forma en SUB."),
    H("OSC A y OSC B"),
    R("1: WT POS", "El fotograma de la tabla, de 1 al último."),
    R("1: WARP, AMOUNT", "KNOB 2 elige el warp; KNOB 3, su cantidad, de 0 a 100 %."),
    R("1: LEVEL", "Volumen del oscilador, 0 a 100 %."),
    R("2: VOICES", "Unison: 1 a 8 voces."),
    R("2: DETUNE, BLEND", "Desafinado entre voces, 0 a 100 %; mezcla de la central y las demás."),
    R("2: WIDTH", "Apertura estéreo del unison, -100 a +100 %."),
    R("3: OCT, SEMI, FINE", "Octavas ±4, semitonos ±12, centésimas ±100."),
    R("3: RANGE", "Hasta dónde llega el DETUNE: 0 a 48 semitonos."),
    R("4: PAN", "Panorama del oscilador."),
    R("4: PHASE, RAND", "Fase de inicio (0 a 360 deg, o MEM) y cuánto azar en ella."),
    FIG(Wavetable, "WT POS recorre los fotogramas de la tabla."),
    H("SUB/NOISE"),
    R("1: SHAPE", "SINE, TRIANGLE, SQUARE, SAW, ROUND, RECT o PULSE."),
    R("1: OCT", "Octava del sub."),
    R("1: DIRECT", "ON: el sub se salta el filtro y los efectos."),
    R("1: SUB", "Volumen del sub, 0 a 100 %."),
    R("2: COLOR", "WHITE, PINK, BROWN, BLUE, CRUSH o RADIO."),
    R("2: PITCH", "Afinación del ruido."),
    R("2: NOISE", "Volumen del ruido, en el KNOB 4. El KNOB 3 no hace nada."),
    LINK(Osc, "Osciladores, sub y ruido explicados"),
};

constexpr Block kParFilterEnvBlocks[] = {
    H("FILTER"),
    R("ALGORITHM", "El tipo: 76, de LP 6 a los combinados."),
    R("1: CUTOFF", "Frecuencia de corte, 8 Hz a 22094 Hz."),
    R("1: RES, DRIVE, MIX", "Resonancia, saturación y mezcla con el sonido sin filtrar, en %."),
    R("2: FAT", "Engorda el filtro, en %."),
    R("2: KEY TRK", "Cuánto sigue el corte a la nota tocada."),
    R("2: ENV 2", "Cuánto abre ENV 2 el corte."),
    R("2: STEREO", "Separa el corte de los dos canales."),
    R("3: OSC A, OSC B, NOISE, SUB", "ON u OFF: qué pasa por el filtro. En INIT, todo OFF."),
    FIG(CutoffRes, "CUTOFF y RES: dónde corta y cuánto realza ese punto."),
    H("ENV 1, ENV 2 y ENV 3"),
    R("1: ATTACK", "0.0 ms a 32.0 s."),
    R("1: DECAY", "0.0 ms a 32.0 s."),
    R("1: SUSTAIN", "-inf a 0.0 dB."),
    R("1: RELEASE", "0.0 ms a 32.0 s."),
    R("2: HOLD", "Cuánto se queda arriba tras el ataque."),
    R("2: A, D y R CURVE", "La curva de cada tramo, -100 a +100 %."),
    N("A MITAD DE MANDO", "Los tiempos no son lineales: con el mando a la mitad, 1.00 s; a un cuarto, unos 31 ms."),
    LINK(Filter, "El filtro explicado"),
};

constexpr Block kParLfoMatrixBlocks[] = {
    H("LFO 1 a 4"),
    R("1: RATE", "0 a 100 Hz, o con BPM 43 figuras, de 32 bar a 1/256 t."),
    R("1: SKEW", "La forma: de SAW DOWN a TRIANGLE a SAW UP."),
    R("1: DEST, AMOUNT", "Un destino rápido y cuánto, sin pasar por la matriz."),
    R("2: BPM", "ON: RATE en figuras a tempo."),
    R("2: MODE", "FREE, RETRIG (reinicia con cada nota) o ENVELOPE (una sola pasada)."),
    R("2: PHASE, BIPOLAR", "Punto de inicio; si va a los dos lados del valor."),
    R("3: START, PEAK, END", "Los tres puntos de la forma básica."),
    R("3: RANDOM", "OFF, S&H o SMOOTH."),
    R("4: POINT X, POINT Y", "Un punto más en la forma."),
    R("4: SMOOTH, DEPTH", "Suavizado y profundidad."),
    R("5: POINT, X, Y, CURVE", "Dibujo punto a punto, hasta 17 puntos."),
    H("CHAOS"),
    R("CHAOS 1 y 2", "Velocidad de cada atractor, LORENZ y ROSSLER, y su BPM."),
    R("Salidas", "1X, 1Y, 2X y 2Y, como fuentes de la matriz."),
    H("MATRIX"),
    R("1: SLOT", "La conexión, 1 a 16."),
    R("1: SOURCE, DEST", "De dónde sale y a dónde va: 111 destinos."),
    R("1: AMOUNT", "-100 a +100 %."),
    R("2: BIPOLAR, CURVE", "Si la fuente va a los dos lados; la curva de respuesta."),
    R("3: AUX, MODE", "Otra fuente que escala la conexión: x AUX o x 1-AUX."),
    R("3: AUX CURVE", "La curva de la fuente auxiliar."),
    R("Fuentes",
      "LFO 1-4, ENV 1-3, CHAOS 1X, 1Y, 2X, 2Y, NOTE, RAND, VELOCITY, MOD WHEEL, AFTERTOUCH, BEND, MACRO 1-4, CC2 "
      "BREATH, CC5 GLIDE y FIXED."),
    FIG(Matrix, "Una conexión: fuente, cantidad y destino, con una auxiliar opcional."),
    LINK(Mod, "Envolventes, LFO y matriz explicados"),
};

constexpr Block kParFxBlocks[] = {
    H("FX RACK"),
    R("KNOB 1, SLOT", "El hueco, 1 a 7."),
    R("KNOB 2, SLOT n", "El módulo de ese hueco, o EMPTY."),
    R("KNOB 3, MOVE", "UP o DOWN: cambia el orden."),
    R("KNOB 4, MODE", "El tipo, u OFF sin quitarlo."),
    FIG(FxChain, "Siete huecos en fila: el sonido pasa de uno al siguiente."),
    H("Los módulos"),
    R("HYPER/DIM", "UNISON 0 a 7, RATE, DETUNE, MIX; DIM SIZE, DIM MIX."),
    R("DISTORTION", "DRIVE, MIX, LEVEL, FILTER (OFF, PRE, POST); FREQ, Q, LP/HP."),
    R("FLANGER", "RATE, DEPTH, FEEDBACK, MIX; BPM, WIDTH 0 a 360 deg."),
    R("PHASER", "RATE, FREQ, DEPTH, FEEDBACK; BPM, WIDTH, POLES 1 a 18, MIX."),
    R("CHORUS", "RATE 0 a 20 Hz, DEPTH 0 a 26 ms, FEEDBACK, MIX; DELAY 1, DELAY 2 (0 a 20 ms), LPF 50 Hz a 15 kHz."),
    R("DELAY", "TIME L y R (1/64 t a 1/2), FEEDBACK, MIX; BPM, LINK, FREQ 40 Hz a 18 kHz, BW."),
    R("COMPRESSOR", "RATIO de 1:1 a LIMIT; modo OTT de varias bandas."),
    R("REVERB", "SIZE, DECAY, WIDTH, MIX; LOW CUT, HIGH CUT, SPIN."),
    R("EQ", "Dos bandas: FREQ, Q, GAIN (-24 a +24 dB) y TYPE."),
    R("FILTER", "Segundo filtro, todos los tipos: CUTOFF, RES, DRIVE, MIX; FAT, STEREO."),
    R("PUMP", "RATE: bar, 1/2, 1/4, 1/8, 1/16, 1/4., 1/8., 1/4 t, 1/8 t."),
    N("SIN VERIFICAR",
      "El motor de la reverb tiene también un DAMP, pero no se ha visto en qué página sale en la FM-1."),
    LINK(Fx, "Los efectos explicados"),
};

constexpr Block kParPlayBlocks[] = {
    H("VOICING"),
    R("1: MONO", "POLY o MONO."),
    R("1: LEGATO", "ON: las notas ligadas no reinician las envolventes."),
    R("1: GLIDE", "Portamento: OFF a 8.00 s."),
    R("1: ALWAYS", "ON: desliza también entre notas sueltas."),
    R("2: RAND CUT, TUN, PAN, VOL", "Azar por nota en corte, afinación, panorama y volumen."),
    H("SCALE"),
    R("1: KEY", "-- (apagada) o de C a B."),
    R("1: SCALE", "Una de 51 escalas."),
    R("1: CHORD", "El acorde que toca cada tecla, o apagado."),
    R("2: OUT OF KEY", "CORRECT, MUTE o THRU."),
    R("2: KEYS LIT", "SCALE PAGE o ALWAYS."),
    R("2: ARP+SCALE", "Si arpegio y escala van con el preset o son globales."),
    H("ARP"),
    R("ALGORITHM", "El patrón: UP, DOWN, UP/DOWN, PLAYED, RANDOM, RAND DRIFT, CONVERGE..."),
    R("1: RATE",
      "bar, 3/4, bar t, 1/2, 3/8, 1/3, 1/4, 3/16, 1/6, 1/8, 3/32, 1/12, 1/16, 3/64, 1/24, 1/32, 3/128, "
      "1/48 o 1/64."),
    R("1: GATE", "Duración de cada nota, 1 a 200 %."),
    R("1: OCTAVES, CHANCE", "1 a 4 octavas; probabilidad de cada nota."),
    R("2: OFFSET", "-8 a +8: desplaza el patrón."),
    R("2: REPEATS", "INF o 1 a 16."),
    R("2: RETRIG", "OFF, NOTE, 1/4, 1/2, BAR, 2 BARS o 4 BARS."),
    R("2: LATCH", "ON: sigue sonando al soltar."),
    R("3: SWING", "50 a 75 %."),
    H("SEQ"),
    R("SELECT", "La pista, 1 a 8."),
    R("ALGORITHM", "El largo de la pista."),
    R("KNOB 1 a 4", "POINT, VALUE, TIME, TEMPO."),
    LINK(Play, "Voces, escala y arpegio explicados"),
};

constexpr Block kParGlobalBlocks[] = {
    P("GLOBAL son los ajustes de la FM-1, salvo LEVEL y TEMPO, que van con cada preset. Se llega con GLO, GLO."),
    R("1: TEMPO", "20 a 300 BPM; 120 en INIT."),
    R("1: LEVEL", "Volumen del preset, -inf a +12 dB; -6.0 dB en INIT."),
    R("1: CLICK", "Metrónomo: OFF, ON o REC (solo al grabar)."),
    R("1: ARP+SCALE", "Si arpegio y escala van con el preset."),
    R("2: VOICES, STEP", "Control de voces: OFF a 8 pasos, y el paso elegido."),
    R("2: PAN n, TUNE n", "Panorama y afinación de ese paso."),
    R("3: MIDI CH", "OMNI, 1 a 16, o MPE. OMNI de fábrica."),
    R("3: USB LEVEL", "MASTER, FIXED o SPK OFF."),
    R("3: KEY LIGHT", "OFF, PNO1 a 5, ALL1 a 5."),
    R("3: BEND RANGE", "OFF, 2, 7, 12 o 24 semitonos; 2 de fábrica."),
    R("4: REC NOTES", "MONO, POLY u OFF. MONO de fábrica."),
    R("4: MIDI OCT", "-4 a +4."),
    R("4: BLUETOOTH", "OFF, ON o LEAD."),
    R("4: LINK", "Estado del enlace con otra FM-1."),
    R("5: A4 TUNING", "415 a 466 Hz; 440 de fábrica."),
    R("5: QUALITY", "NORMAL o DRAFT (más notas, menos calidad)."),
    R("5: CPU MHZ", "360 o 320 (más estable, un 10 % menos de notas)."),
    LINK(Midi, "MIDI, USB y Bluetooth"),
};

}  // namespace

const Chapter kRefButtons = FMGUIDE_CHAPTER("Botones y mandos", kButtonsBlocks);
const Chapter kRefCombos = FMGUIDE_CHAPTER("Combinaciones y atajos", kCombosBlocks);
const Chapter kRefMessages = FMGUIDE_CHAPTER("Mensajes de la pantalla", kMessagesBlocks);
const Chapter kParOsc = FMGUIDE_CHAPTER("Parámetros: osciladores", kParOscBlocks);
const Chapter kParFilterEnv = FMGUIDE_CHAPTER("Parámetros: filtro y ENV", kParFilterEnvBlocks);
const Chapter kParLfoMatrix = FMGUIDE_CHAPTER("Parámetros: LFO y matriz", kParLfoMatrixBlocks);
const Chapter kParFx = FMGUIDE_CHAPTER("Parámetros: efectos", kParFxBlocks);
const Chapter kParPlay = FMGUIDE_CHAPTER("Parámetros: tocar", kParPlayBlocks);
const Chapter kParGlobal = FMGUIDE_CHAPTER("Parámetros: GLOBAL", kParGlobalBlocks);

}  // namespace shrub
}  // namespace fmguide

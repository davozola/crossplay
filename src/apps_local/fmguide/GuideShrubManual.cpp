#include "GuideData.h"
#include "GuideShrubChapters.h"
#include "SynthFigures.h"

// SHRUB, MANUAL: every part of the panel. Page and knob names as the FM-1
// shows them, read off the emulator; ranges as the editor's engine prints
// them. Checked on 27 November 2026 (see GuideShrub.cpp).

namespace fmguide {
namespace shrub {

namespace {

using namespace synthfig;

constexpr Block kMoveBlocks[] = {
    P("SHRUB ordena el sonido en secciones, cada sección en partes, y cada parte en páginas de cuatro mandos. Un botón "
      "abre su sección; pulsarlo otra vez pasa a la parte siguiente. SELECT pasa las páginas y los cuatro KNOB cambian "
      "lo que la pantalla enseña encima de ellos."),
    PANEL(cMaster | cPresets | cSelect | cAlgorithm | cKnobs,
          "MASTER, SELECT, PRESETS y ALGORITHM a la izquierda; los cuatro KNOB, bajo la pantalla."),
    H("Los mandos"),
    R("MASTER", "El volumen general."),
    R("PRESETS", "Recorre los presets. El sonido se carga cuando dejas de girar."),
    R("SELECT", "Pasa las páginas de la parte. En SAVE mueve el cursor del nombre; en SEQ elige la pista."),
    R("ALGORITHM",
      "El tipo de la parte: la tabla de un oscilador, el tipo de filtro, el patrón del arpegio. En HOME salta de "
      "categoría."),
    R("KNOB 1 a 4", "Los cuatro valores de la página que se ve."),
    H("Los botones y sus partes"),
    PANEL(cFx | cSel | cEnv | cLfo | cEdit | cGlo | cHome | cSave | cArp | cSeq | cPlay | cRec,
          "Las dos filas de botones. El segundo, impreso SEL, es el que SHRUB llama SCL: las escalas."),
    R("EDIT", "OSC A, OSC B, SUB/NOISE y FILTER, una por pulsación."),
    R("ENV", "ENV 1, ENV 2, ENV 3 y MATRIX."),
    R("LFO", "LFO 1 a LFO 4, CHAOS y MATRIX. La MATRIX es la misma que en ENV."),
    R("FX", "FX RACK, y después cada efecto del rack, en su orden."),
    R("GLO", "VOICING (cómo se tocan las notas) y GLOBAL (los ajustes de la FM-1)."),
    R("SEL (SCL)", "SCALE: tonalidad, escala y acordes."),
    R("HOME", "La pantalla del preset. Otra vez: sus KNOB pasan a ser las cuatro macros."),
    R("SAVE", "Guardar. Otra vez: confirma."),
    R("ARP", "La página del arpegiador. Otra vez: lo enciende o lo apaga."),
    R("SEQ", "El secuenciador: las pistas de la secuencia en curso."),
    R("PLAY", "Arranca y para el transporte: arpegio, pistas y metrónomo."),
    R("REC", "Graba. Si suena, desde el compás siguiente; si está parado, tras un compás de cuenta."),
    R("OCT- y OCT+", "Una octava abajo y arriba. Juntos: latch, las notas siguen al soltarlas."),
    N("ATAJO",
      "Mantén EDIT, ENV o LFO y gira SELECT: saltas entre sus partes en los dos sentidos, sin pulsar el botón varias "
      "veces."),
    H("La pantalla"),
    R("Arriba", "El nombre de la parte, en su color; una raya por página, la actual resaltada."),
    R("En medio", "El dibujo de la parte: la tabla en 3D, la curva del filtro, la envolvente, el efecto."),
    R("Abajo", "Los cuatro mandos, con el nombre y el valor que tienen."),
    R("Anillo azul", "Un mando modulado lleva un anillo azul que marca hasta dónde lo mueve la modulación."),
    H("El teclado"),
    KEYS(kAllWhite | kAllBlack, "27 teclas, de Fa a Sol: 16 blancas y 11 negras. Tocan siempre con velocity 100."),
    P("Se iluminan las teclas que suenan, las toques tú, lleguen por MIDI o las toque el arpegio. En la página SCALE "
      "enseñan la escala. KEY LIGHT, en GLOBAL, hace que las apagadas brillen un poco para tocar a oscuras."),
};

constexpr Block kPresetsBlocks[] = {
    P("HOME es la pantalla de inicio: el preset que suena, su número, su nombre, su categoría y cuatro mandos a mano. "
      "De ahí sales a todo lo demás y ahí vuelves."),
    PANEL(cHome | cPresets | cAlgorithm | cKnobs,
          "En HOME: PRESETS elige, ALGORITHM salta de categoría y los KNOB tocan lo básico."),
    H("Los 128 huecos"),
    R("1 a 100", "Los de fábrica, en siete categorías: BASS, LEAD, MISC, PAD, PLUCK, SEQ y SYNTH."),
    R("101 a 128", "INIT: sonidos de partida donde guardar los tuyos."),
    R("Con una U", "Un hueco de fábrica que ya has sobrescrito lleva una U delante del número."),
    H("El preset INIT"),
    P("Es el init de Serum: la sierra de BASIC SHAPES en OSC A (WT POS 2), OSC B a 0 %, ataque de 0.5 ms y release "
      "de 10 ms, sin efectos encendidos. El filtro es LP 24 a unos 2 kHz, pero no recibe nada: en su página 3, OSC "
      "A, OSC B, NOISE y SUB están en OFF."),
    H("Los mandos de HOME"),
    R("KNOB 1 y 2", "CUTOFF y RES del filtro."),
    R("KNOB 3 y 4", "A POS y B POS: la posición en la tabla de cada oscilador."),
    R("HOME otra vez",
      "Los KNOB pasan a ser MACRO 1 a 4. Se queda así al cambiar de preset y al apagar, hasta que vuelvas a pulsar."),
    P("Al girar una macro, la pantalla dice un momento a qué va, por ejemplo «M1: CUTOFF» o «M2: A WT POS, RES». Una "
      "macro sin destino dice «M3: -» y no hace nada: las macros son fuentes de la matriz."),
    LINK(Mod, "Cómo conectar una macro en la matriz"),
    H("Favoritos"),
    S("1", "Mantén HOME medio segundo: el preset queda marcado como favorito."),
    S("2", "Con algún favorito, ALGORITHM en HOME pasa también por el grupo FAVORITES."),
    S("3", "Para quitarlo, mantén HOME otra vez."),
    H("No perder lo editado"),
    P("Si has cambiado algo y giras PRESETS, la pantalla dice EDITED / TURN AGAIN y el sonido no cambia. Gira otra vez "
      "para irte, perdiendo los cambios, o pulsa SAVE para quedártelos."),
    N("AL APAGAR",
      "SHRUB guarda solo el sonido en curso y sus pistas un par de segundos después de que dejes de tocar mandos, y lo "
      "recupera al encender. No es lo mismo que guardarlo en un hueco."),
    H("Guardar"),
    PANEL(cSave | cSelect | cAlgorithm | cKnobs, "SAVE: los KNOB eligen hueco, cursor, letra y categoría."),
    S("1", "Pulsa SAVE. La pantalla enseña el hueco, el nombre y lo que vas a sustituir."),
    S("2",
      "KNOB 1 elige el hueco, de 1 a 128. Si el sonido venía de fábrica, empieza en el primer INIT libre para que no "
      "pises un original sin querer."),
    S("3", "KNOB 2 (o SELECT) mueve el cursor; KNOB 3 (o ALGORITHM) cambia la letra. Hasta 16 caracteres."),
    S("4", "KNOB 4 elige la categoría."),
    S("5", "SAVE otra vez guarda. Cualquier otro botón de sección sale sin guardar."),
    DANGER("SOBRESCRIBE",
           "Guardar en un hueco ocupado sustituye lo que había. Un preset de fábrica se recupera desde el editor "
           "(LIBRARY, "
           "restaurar el preset de fábrica del hueco); uno tuyo, solo si tienes copia."),
    H("Lo que puede contestar"),
    R("SAVED", "Guardado."),
    R("SAVED, THINNED", "Las pistas tenían demasiado detalle: las ha suavizado un poco para que quepan."),
    R("SAVED, NO LANES", "Las pistas no cabían: el sonido se ha guardado sin ellas."),
    R("AUX CUT, LFOS CUT, NO LFO 4",
      "Un sonido enorme ha perdido algo para caber: una modulación auxiliar, la forma dibujada de un LFO o el LFO 4."),
    R("NOT SAVED: STORAGE FULL", "La memoria está llena. Libera huecos desde LIBRARY o sobrescribe uno viejo."),
    P("Cada hueco tiene alrededor de 1 KB para el sonido y sus secuencias. Si también hay pistas, SAVE puede pedir "
      "«SAVE AGAIN: SOUND + LANES»."),
    N("EL ARBUSTO",
      "Mantén GLO medio segundo y aparece el arbusto de SHRUB. SELECT a la izquierda da un sonido al azar; a la "
      "derecha, una mezcla de seis presets. Cualquier botón vuelve."),
};

constexpr Block kOscBlocks[] = {
    P("EDIT abre los osciladores. OSC A y OSC B funcionan igual: leen una wavetable, una fila de formas de onda que "
      "se recorre con WT POS. Mover la posición cambia el timbre poco a poco: es la gracia de la síntesis por tablas."),
    FIG(Waveforms, "Las formas básicas. Cuanto más abrupta la forma, más armónicos y más brillo."),
    PANEL(cEdit | cAlgorithm | cSelect | cKnobs,
          "EDIT pasa de OSC A a OSC B, SUB/NOISE y FILTER. ALGORITHM elige la tabla; SELECT, la página."),
    H("OSC A y OSC B, página a página"),
    R("1: WT POS, WARP, AMOUNT, LEVEL",
      "La posición en la tabla, el tipo de warp, cuánto deforma (0-100 %) y el volumen del oscilador."),
    R("2: VOICES, DETUNE, BLEND, WIDTH",
      "Unison: de 1 a 8 voces, cuánto se desafinan, cuánto pesan las de fuera frente a la central y cuánto se abren en "
      "estéreo (-100 a +100 %)."),
    R("3: OCT, SEMI, FINE, RANGE",
      "Afinación: octavas (-4 a +4), semitonos (-12 a +12), centésimas (-100 a +100 ct) y RANGE, el alcance del "
      "detune del unison (0 a 48 st)."),
    R("4: PAN, PHASE, RAND",
      "Panorama (50 L a 50 R), fase de inicio de la onda (0 a 360 deg, o MEM) y cuánto varía esa fase en cada "
      "nota."),
    FIG(Unison, "Unison: varias copias de la nota, desafinadas (DETUNE) y abiertas en estéreo (WIDTH)."),
    H("Las tablas"),
    P("ALGORITHM elige la tabla. La lista empieza en DEFAULT y BASIC SHAPES y sigue con tablas de Serum 1 y 2 (MODEL "
      "D, "
      "REESE, PWM JUNO, SOLINA VIOLA, 808 SUB...), unas que crecen como un autómata celular (RULE 30, RULE 90...), "
      "SHRUBBY, leída del dibujo del arbusto, y ADDITIVE. Las tuyas aparecen después de cargarlas."),
    R("BASIC SHAPES", "WT POS 1 seno, 2 sierra (la de INIT), 3 triángulo, 4 cuadrada."),
    R("ADDITIVE",
      "Una tabla que construyes con armónicos: HARMONICS elige uno y le das nivel. MODE (SINGLE, AUTO WARP, SWEEP), "
      "WARP, START, END, CURVE, FRAMES (8 a 64) y GAIN completan sus páginas."),
    FIG(Harmonics, "Un sonido es la suma de armónicos. ADDITIVE te deja dibujar esa suma."),
    H("WARP: deformar la onda"),
    P("WARP elige cómo se retuerce la onda y AMOUNT, cuánto. Son los de Serum. Los que llevan (B) en OSC A, o (A) en "
      "OSC B, usan el otro oscilador como modulador."),
    FIG(Warp, "El mismo ciclo, deformado. SYNC mete más ciclos; BEND+ los aprieta al principio; MIRROR los refleja."),
    R("SYNC, SYNC WIN, SYNC 1/2 WIN", "Sincronía dura: la onda vuelve a empezar antes de acabar. El clásico chillido."),
    R("BEND+, BEND-, BEND+/-", "Aprieta la onda hacia el principio, el final o los dos."),
    R("PWM", "Ancho de pulso: estrecha una parte del ciclo."),
    R("ASYM+, ASYM-, ASYM+/-", "Deforma sin simetría: cambia el brillo de una mitad del ciclo."),
    R("FLIP y MIRROR", "Da la vuelta a una parte del ciclo o lo refleja."),
    R("QUANTIZE", "Escalonado: baja la resolución de la onda, como un sonido de 8 bits."),
    R("PD (B)", "Distorsión de fase, al estilo de los Casio CZ, guiada por el otro oscilador."),
    R("FM, AM y RM", "Frecuencia, amplitud o anillo modulados desde el otro oscilador (B o A), el ruido o el sub."),
    R("REMAP", "26 curvas, de REMAP 8SQR a REMAP RAND, que leen la onda en otro orden."),
    H("SUB/NOISE"),
    P("Tercera pulsación de EDIT. El sub refuerza los graves por debajo de los osciladores; el ruido da aire, golpe o "
      "textura."),
    R("1: SHAPE, OCT, DIRECT, SUB",
      "La forma del sub (SINE, TRIANGLE, SQUARE, SAW, ROUND RECT, PULSE), su octava (-4 a +4), DIRECT y su nivel."),
    R("2: COLOR, PITCH, -, NOISE",
      "El color del ruido (WHITE, PINK, BROWN, BLUE, CRUSH, RADIO), su afinación (50 % es neutro) y, en el KNOB 4, su "
      "nivel. El KNOB 3 no hace nada."),
    N("DIRECT",
      "Con DIRECT en ON, el sub se salta el filtro y los efectos y se une al final: graves firmes aunque cierres el "
      "filtro o distorsiones."),
};

constexpr Block kFilterBlocks[] = {
    P("Cuarta pulsación de EDIT. Un único filtro para todo lo que le mandes. Es la pieza que más se nota en un acorde, "
      "porque todas las notas se suman antes de filtrarse."),
    PANEL(cEdit | cAlgorithm | cSelect | cKnob1 | cKnob2,
          "ALGORITHM elige el tipo; KNOB 1 y 2, CUTOFF y RES. SELECT pasa las tres páginas."),
    H("Las tres páginas"),
    R("1: CUTOFF, RES, DRIVE, MIX",
      "El corte (8 Hz a 22 kHz), la resonancia, la saturación a la entrada y la mezcla entre seco y filtrado."),
    R("2: FAT, KEY TRK, ENV 2, STEREO",
      "El segundo mando de cada tipo, si el corte sigue al teclado (OFF u ON), cuánto lo mueve ENV 2 (-100 a +100 %) "
      "y cuánto se separa entre izquierda y derecha."),
    R("3: OSC A, OSC B, NOISE, SUB",
      "Qué pasa por el filtro, cada uno ON u OFF. Lo que está en OFF se lo salta y se une después."),
    N("PRIMERO, LA PAGINA 3",
      "Si CUTOFF no cambia nada, mira la página 3: en INIT todo está en OFF. El SUB con DIRECT se salta el filtro "
      "aunque aquí esté en ON."),
    P("FAT cambia de nombre y de efecto según el tipo de filtro: Smooth, Pain, Width, Damp, Boeuf..."),
    FIG(FilterTypes, "Los cuatro filtros básicos: deja los graves (LP), los agudos (HP), una banda (BP) o la quita."),
    FIG(Slopes, "La pendiente: cuántos dB por octava corta después del CUTOFF. Más dB, corte más tajante."),
    H("Los 76 tipos, por familias"),
    R("LP, HP, BP, PEAK, NOTCH", "Los básicos: LP 6, 12 y 24; HP 6, 12 y 24; BP 12 y 24; PEAK y NOTCH 12 y 24."),
    R("MG LOW 6 a 24", "Los paso bajo de Serum 1, de carácter analógico."),
    R("MG LADDER, MG DIRTY, ACID LADDER", "Ladders, del tipo de los Moog y la TB-303."),
    R("FRENCH LP, BANDREJECT", "Otro paso bajo de color propio y un rechazo de banda."),
    R("ALLPASSES, REVERB", "Mueven la fase o difuminan: más efecto que filtro."),
    R("COMB y FLANGE", "Peines: afinan el sonido a la nota del CUTOFF. Cuerdas pulsadas, metales, rarezas."),
    R("PHASE y PHS48", "Phasers dentro del propio filtro."),
    R("L+H 6, L/B/H 12...", "Dos o tres filtros a la vez: L paso bajo, H alto, B banda, P pico, N notch."),
    H("Un filtro para todas las notas"),
    B("Una nota sola suena exactamente como en Serum."),
    B("En un acorde todas comparten corte, resonancia y el barrido de ENV 2. KEY TRK sigue a la última nota."),
    B("Con mucha resonancia, el acorde zumba con la nota que más suena."),
    N("TRUCO",
      "Para pads de acordes, filtro suave y movimiento lento. Para bajos y solistas, VOICING en MONO o LEGATO: con una "
      "nota, el filtro único no se nota."),
    N("CON LA REVERB HALL",
      "HALL comparte memoria con los filtros comb, ALLPASSES, REVERB, FLANGE y FLG PHS: con HALL encendida pueden "
      "dejar de sonar. PLATE deja sitio para los dos."),
};

constexpr Block kModBlocks[] = {
    P("La modulación mueve el sonido solo: tres envolventes, cuatro LFO, dos fuentes de caos y una matriz que conecta "
      "cada fuente con lo que quieras."),
    H("Las envolventes"),
    PANEL(cEnv | cSelect | cKnobs, "ENV pasa de ENV 1 a ENV 2, ENV 3 y MATRIX."),
    FIG(Adsr, "Attack sube, Decay baja hasta el Sustain, que se mantiene con la tecla; Release, al soltarla."),
    R("1: ATTACK, DECAY, SUSTAIN, RELEASE",
      "Los tiempos van de 0.0 ms a 32 s; SUSTAIN, de -inf dB (nada) a 0.0 dB (todo)."),
    R("2: HOLD, A CURVE, D CURVE, R CURVE",
      "HOLD mantiene el pico antes del decay. Las curvas (-100 a +100 %) doblan cada tramo."),
    R("ENV 1", "El volumen de cada nota. Es la única envolvente por nota."),
    R("ENV 2", "Conectada de fábrica al mando ENV 2 del filtro, y libre en la matriz."),
    R("ENV 3", "Solo sale en la matriz: conéctala tú."),
    N("COMPARTIDAS",
      "ENV 2 y ENV 3 son una para todas las notas. Una nota nueva las arranca desde donde estaban, y se relajan cuando "
      "sueltas la última tecla."),
    H("Los LFO"),
    PANEL(cLfo | cSelect | cOct | cKnobs,
          "LFO pasa de LFO 1 a 4, CHAOS y MATRIX. Con LFO mantenido, OCT+ y OCT- añaden y quitan puntos."),
    R("1: RATE, SKEW, DEST, AMOUNT",
      "Velocidad (0 a 100 Hz, o una figura con BPM), forma (SAW DOWN, TRIANGLE, SAW UP y todo lo de en medio), a dónde "
      "va y cuánto."),
    R("2: BPM, MODE, PHASE, BIPOLAR",
      "Sincronía al tempo; MODE FREE (corre siempre), RETRIG (empieza con cada nota) o ENVELOPE (una vez por nota); "
      "fase de inicio; si oscila a los dos lados."),
    R("3: START, PEAK, END, RANDOM", "Los tres puntos de la forma, y el azar: OFF, S&H (a saltos) o SMOOTH."),
    R("4: POINT X, POINT Y, SMOOTH, DEPTH", "Un punto más en la forma, cuánto se suaviza y la profundidad."),
    R("5: POINT, X, Y, CURVE", "El dibujo punto a punto: hasta 17 puntos, cada uno con su curva."),
    FIG(LfoShapes, "SKEW pasa de diente de sierra que baja a triángulo y a sierra que sube. S&H salta al azar."),
    FIG(Bipolar, "Unipolar solo sube desde el valor del mando; bipolar sube y baja alrededor de él."),
    N("DEST, EL ATAJO",
      "DEST, en la página 1, edita la primera conexión de la matriz que sale de ese LFO. Si no tiene ninguna, toma un "
      "hueco libre. No hace falta abrir la matriz."),
    P("Para dibujar: mantén LFO y pulsa OCT+ para añadir un punto u OCT- para quitarlo. Los extremos no se borran. Los "
      "LFO son compartidos por todas las notas."),
    R("CHAOS",
      "CHAOS 1 (un atractor de Lorenz) y CHAOS 2 (de Rössler): movimiento que no se repite. Cada uno con su velocidad "
      "y "
      "su BPM. En la matriz dan dos salidas cada uno: X e Y."),
    H("La matriz"),
    FIG(Matrix, "Cada conexión: una fuente, cuánto (AMOUNT) y un destino. AUX puede escalar esa cantidad."),
    R("1: SLOT, SOURCE, DEST, AMOUNT", "La conexión (1 a 16), de dónde sale, a dónde va y cuánto (-100 a +100 %)."),
    R("2: SLOT, BIPOLAR, CURVE", "Si la fuente va a los dos lados y con qué curva."),
    R("3: SLOT, AUX, MODE, AUX CURVE",
      "Una segunda fuente que escala la primera: MODE x AUX (más AUX, más modulación) o x 1-AUX (al revés)."),
    R("Fuentes",
      "LFO 1-4, ENV 1-3, CHAOS 1X, 1Y, 2X y 2Y, NOTE, RAND, VELOCITY, MOD WHEEL, AFTERTOUCH, BEND, MACRO 1-4, CC2 "
      "BREATH, CC5 GLIDE y FIXED."),
    R("Destinos", "111: tablas, warps, afinación, niveles, filtro, envolventes, LFO, efectos, GLIDE y LANE 1 a 8."),
    R("LANE 1 a 8",
      "Silencian pistas del secuenciador: con cantidad positiva, la pista suena cuando la fuente pasa de la mitad; "
      "con negativa, al revés."),
    N("LAS MACROS",
      "MACRO 1 a 4 son fuentes: no hacen nada hasta que las conectas aquí. Luego se mueven con los KNOB de HOME, en "
      "modo macros."),
};

constexpr Block kFxBlocks[] = {
    P("FX abre el rack: siete huecos en fila, cada uno con un módulo. El sonido los recorre en orden, así que importa: "
      "una distorsión antes de la reverb no suena igual que después."),
    FIG(FxChain, "El rack: el sonido entra por el hueco 1 y sale por el 7."),
    PANEL(cFx | cKnobs, "FX: el rack. FX otra vez: los mandos de cada efecto, por orden."),
    H("El rack: FX RACK"),
    R("KNOB 1, SLOT", "El hueco, de 1 a 7."),
    R("KNOB 2, SLOT n", "El módulo de ese hueco, o EMPTY."),
    R("KNOB 3, MOVE", "Sube o baja el módulo en la fila (UP / DOWN)."),
    R("KNOB 4, MODE", "El tipo del módulo, u OFF para apagarlo sin quitarlo."),
    P("En INIT el rack trae DISTORTION, CHORUS, DELAY, COMPRESSOR y REVERB, todos apagados, y dos huecos libres."),
    H("Los módulos y sus mandos"),
    R("HYPER/DIM", "UNISON (0-7), RATE, DETUNE, MIX; DIM SIZE, DIM MIX. Engorda y abre."),
    R("DISTORTION", "DRIVE, MIX, LEVEL, FILTER (OFF, PRE, POST); FREQ, Q y LP/HP del filtro que la acompaña."),
    R("FLANGER", "RATE, DEPTH, FEEDBACK, MIX; BPM y WIDTH (0 a 360 deg)."),
    R("PHASER", "RATE, FREQ, DEPTH, FEEDBACK; BPM, WIDTH, POLES (1 a 18) y MIX."),
    R("CHORUS", "RATE (0 a 20 Hz), DEPTH (0 a 26 ms), FEEDBACK, MIX; DELAY 1, DELAY 2 y LPF."),
    R("DELAY", "TIME L, TIME R (de 1/64 t a 1/2), FEEDBACK, MIX; BPM, LINK, FREQ y BW."),
    R("COMPRESSOR",
      "DEPTH, THRESH, UPWARD, TIME; GAIN, MIX y OTT. Con OTT en OFF, una sola banda con RATIO, ATTACK y RELEASE."),
    R("REVERB", "SIZE, DECAY, WIDTH, MIX; LOW CUT, HIGH CUT y SPIN."),
    R("EQ", "Dos bandas: FREQ, Q, GAIN y TYPE de cada una (-24 a +24 dB)."),
    R("FILTER", "Un segundo filtro con todos los tipos: CUTOFF, RES, DRIVE, MIX; FAT y STEREO."),
    R("PUMP",
      "El bombeo de un sidechain, sin bombo: RATE (de bar a 1/16, con puntillos y tresillos), DEPTH, RELEASE y CURVE."),
    H("Los tipos, con MODE"),
    R("DISTORTION",
      "TUBE, SOFT CLIP, HARD CLIP, DIODE 1 y 2, LINEAR FOLD, SINE FOLD, ZERO-SQUARE, DOWNSAMPLE, ASYM, RECTIFY, SINE "
      "SHAPER, STOMP BOX, TAPE SAT. y SOFT SAT."),
    R("DELAY", "NORMAL, PING-PONG (rebota de un lado a otro) y TAP."),
    R("REVERB", "PLATE, más corta y brillante, y HALL, una sala grande."),
    R("EQ", "Banda 1: LO SHELF, PEAK o HP. Banda 2: HI SHELF, PEAK o LP."),
    N("COMPRESION OTT",
      "OTT en ON vuelve el compresor multibanda: sube lo flojo y aplasta lo fuerte en tres bandas. Es el sonido "
      "apretado de tanta música de Serum. Empieza con poco MIX."),
};

constexpr Block kPlayBlocks[] = {
    P("Cómo se toca: cuántas notas suenan y cómo se enlazan, en qué escala, con qué acordes y con qué arpegio. GLO "
      "abre VOICING, SEL (SCL) abre SCALE y ARP abre el arpegiador."),
    H("VOICING"),
    PANEL(cGlo | cSelect | cKnobs, "GLO: VOICING. GLO otra vez: GLOBAL."),
    R("MONO", "POLY o MONO. En MONO, al soltar la nota de arriba vuelve la que sigas pisando."),
    R("LEGATO", "Las notas ligadas no reinician las envolventes. Vale también para acordes."),
    R("GLIDE", "El portamento: de OFF a 8 s, cuánto tarda en deslizarse de una nota a otra."),
    R("ALWAYS", "Desliza también entre notas sueltas, no solo entre ligadas."),
    R("Página 2", "RAND CUT, RAND TUN, RAND PAN y RAND VOL: un poco de azar distinto en cada nota."),
    FIG(Glide, "Con GLIDE la altura se desliza; con LEGATO, solo cuando la nota nueva entra antes de soltar la otra."),
    H("Cuántas notas caben"),
    P("Hasta ocho notas a la vez, de un total de 16 voces. Cada nota gasta más voces cuanto más unison tenga, y "
      "siempre "
      "suena al menos una. La pantalla de VOICING dice cuántas caben con el sonido actual, por ejemplo «POLY, 7 "
      "NOTES». Si una nota nueva no cabe, SHRUB le cede una voz sin chasquidos."),
    FIG(VoicePool, "Esquema: con unison alto, cada nota ocupa más sitio. El manual no da la fórmula exacta."),
    N("MAS NOTAS",
      "QUALITY en DRAFT (GLOBAL, página 5) lee los osciladores con algo menos de detalle y deja tocar más notas en "
      "sonidos polifónicos. CPU MHZ en 320 toca alrededor de un 10 % menos."),
    H("SCALE"),
    PANEL(cSel | cKnobs, "SEL (SCL) abre SCALE."),
    R("1: KEY, SCALE, CHORD", "La tónica (-- apagada, o de C a B), una de 51 escalas y el tipo de acorde."),
    R("2: OUT OF KEY", "CORRECT lleva la nota a la más cercana de la escala; MUTE la calla; THRU la deja pasar."),
    R("2: KEYS LIT", "SCALE PAGE ilumina la escala solo en esta página; ALWAYS, siempre."),
    R("2: ARP+SCALE",
      "PRESET: cada preset trae su arpegio, tónica, escala y acorde. GLOBAL: se quedan los tuyos al cambiar."),
    H("Acordes con una tecla"),
    KEYS(kAllBlack,
         "Con CHORD encendido, las once teclas negras eligen el tipo de acorde, desde el Fa# de abajo; las blancas "
         "tocan los grados de la escala."),
    P("Los once tipos, por orden: ROOT, 3RD, TRIAD, 7TH, 9TH, SUS2, SUS4, 6TH, POWER, OPEN y OCTAVE. Cada blanca "
      "construye el acorde con las notas de la escala: en Do mayor con TRIAD, la tecla de Re toca Re menor."),
    H("El arpegiador"),
    PANEL(cArp | cAlgorithm | cSelect | cKnobs,
          "ARP abre la página; ARP otra vez lo enciende o lo apaga. ALGORITHM elige el patrón."),
    FIG(Arp, "Mantienes un acorde y el arpegiador toca sus notas una a una, en orden y a tempo."),
    R("1: RATE, GATE, OCTAVES, CHANCE",
      "La figura (de bar a 1/64, con puntillos y tresillos), cuánto dura cada nota (1 a 200 %), cuántas octavas (1 a "
      "4) "
      "y la probabilidad de cada paso."),
    R("2: OFFSET, REPEATS, RETRIG, LATCH",
      "OFFSET (-8 a +8) y REPEATS (INF, o de 1 a 16) existen, pero el manual no los explica. RETRIG reinicia el "
      "patrón (NOTE, 1/4, 1/2, BAR, 2 o 4 BARS); LATCH lo deja sonando al soltar."),
    R("3: SWING", "De 50 % (recto) a 75 %: retrasa cada segundo paso."),
    R("Patrones",
      "19: UP, DOWN, UP/DOWN, DOWN/UP, UP+DOWN, DOWN+UP, THUMB UP, THUMB UD, PINKY UP, PINKY UD, CONVERGE, DIVERGE, "
      "CON+DIVERGE, PLAYED, CHORD, RANDOM, RAND NO DUP, RAND DRIFT y RAND ONCE."),
    N("GATE Y LEGATO",
      "Con GATE por encima del 100 % las notas se pisan, y en LEGATO se deslizan unas a otras: el bajo ácido sale "
      "solo."),
};

constexpr Block kSeqBlocks[] = {
    P("El secuenciador de SHRUB es de movimiento, como el de Serum: graba lo que haces con los mandos y lo repite en "
      "bucle. También graba notas. Cada preset tiene 27 secuencias, una por tecla, y cada secuencia, ocho pistas."),
    PANEL(cSeq | cRec | cPlay | cSelect | cAlgorithm | cKnobs,
          "SEQ enseña las pistas; REC graba; PLAY arranca y para. SELECT elige pista; ALGORITHM, su largo."),
    FIG(Automation, "Una pista de movimiento: los puntos que dejó el mando, unidos, en bucle."),
    H("Qué guarda una pista"),
    R("Movimiento",
      "Lo que hiciste con un mando. Los valores continuos se deslizan entre puntos; los de tipo o modo saltan."),
    R("Notas", "Las teclas que tocas, una nota a la vez por pista."),
    R("Qué se graba",
      "Casi cualquier mando de sonido: osciladores, filtro, envolventes, LFO y efectos. No la tabla (TABLE). De "
      "GLOBAL, "
      "solo LEVEL, GLIDE y las macros."),
    R("REC NOTES",
      "En GLOBAL, página 4. MONO (de fábrica): todo a una pista, y un acorde se graba como su última nota. POLY: una "
      "pista para cada nota del acorde, las que haya libres. OFF: solo mandos."),
    H("Grabar el movimiento de un mando"),
    S("1", "Pulsa REC. Si suena, graba desde el compás siguiente. Si está parado, cuenta un compás: IN 4 a IN 1."),
    S("2", "Gira un mando. La toma de ese mando acaba cuando dejas de girarlo un momento."),
    S("3", "Pulsa REC otra vez, o PLAY, para terminar."),
    H("Notas paso a paso"),
    FIG(StepSeq, "Cada tecla pone una nota en un paso; GATE decide cuánto dura dentro de él."),
    S("1", "Con el transporte parado, mantén SEQ y pulsa REC: la pantalla dice STEP y enseña el cursor."),
    S("2", "Antes, fija el largo de la pista con ALGORITHM."),
    S("3", "Cada tecla pone una nota en el cursor; varias juntas, un acorde. El cursor avanza al soltar."),
    S("4",
      "KNOB 1, CURSOR: adelante deja un silencio, atrás corrige. KNOB 2, STEP: de 1/32 a un compás. KNOB 3, GATE: "
      "10 a 100 % del paso. KNOB 4, TEMPO."),
    S("5", "Termina con REC, con PLAY o saliendo de SEQ."),
    H("Editar una pista"),
    R("SELECT", "Elige la pista, de 1 a 8."),
    R("KNOB 1 a 4", "POINT, VALUE, TIME y TEMPO: el punto o nota, su valor, su sitio en el tiempo y el tempo."),
    R("ALGORITHM",
      "El largo: de compás en compás hasta 128; con SEQ mantenido, de 1/16 en 1/16. El resumen del manual habla de "
      "mitad y doble: si ves eso, es lo mismo dicho de otra forma."),
    FIG(Polyrhythm, "Cada pista da la vuelta con su propio largo: una de 3 y otra de 4 coinciden cada 12 compases."),
    H("Con SEQ mantenido"),
    PANEL(cSeq | cKeys | cOct | cSel | cKnobs, "SEQ mantenido convierte teclas, octavas, SCL y los KNOB en atajos."),
    R("+ una tecla",
      "Elige la secuencia de esa tecla. Sonando, cambia en el compás siguiente (o en el tiempo o la semicorchea, según "
      "SWITCH)."),
    R("+ tecla mantenida, + tecla vacía", "Copia la secuencia de la primera tecla en la segunda."),
    R("+ OCT-", "Borra el punto seleccionado."),
    R("+ KNOB 1, SWING", "Retrasa las semicorcheas de contratiempo de las pistas de notas. Es el mismo del arpegio."),
    R("+ KNOB 2, OVERLAP", "OFF: cada nota corta la anterior. ON: cada una suena entera."),
    R("+ KNOB 3, PARAMS",
      "STAY: los mandos se quedan donde los dejan las pistas. RETURN: vuelven a su valor al parar o cambiar de "
      "secuencia."),
    R("+ KNOB 4, SWITCH", "Cuándo cambia de secuencia: BAR, BEAT o 1/16."),
    R("+ SCL", "TRANSPOSE SEQ: las teclas o el MIDI transponen la secuencia."),
    DANGER(
        "BORRAR PISTAS",
        "SEQ + OCT+ vacía la pista elegida. Para vaciarlas todas, mantén SEQ + OCT- + OCT+: la pantalla pide CLEAR "
        "ALL? KEEP HOLDING, y si sigues manteniendo las borra. Si sueltas antes, no borra nada. El manual no dice si "
        "son las de una secuencia o las de las 27."),
    N("SECUENCIAS EN DIRECTO",
      "Pulsa SEQ dos veces seguidas: cada tecla elige su secuencia. Pulsa SEQ dos veces más, o una tecla de octava, "
      "para "
      "volver a tocar notas."),
    KEYS(kAllWhite | kAllBlack, "Cada una de las 27 teclas guarda una secuencia del preset."),
    H("Tempo y memoria"),
    B("El tempo lo guarda la FM-1, pero un sonido con secuencia trae el suyo."),
    B("Si llega reloj MIDI, el transporte le sigue. PLAY en la FM-1 recupera el mando."),
    B("El sonido y sus 27 secuencias comparten alrededor de 1 KB por hueco. Si no caben, SAVE suaviza las pistas o las "
      "deja fuera, y lo dice."),
    B("Una pista silenciada desde la matriz (LANE 1 a 8) sigue corriendo, en gris, para no perder el compás."),
};

}  // namespace

const Chapter kMove = FMGUIDE_CHAPTER("Moverse por el panel", kMoveBlocks);
const Chapter kPresets = FMGUIDE_CHAPTER("Presets, macros y guardar", kPresetsBlocks);
const Chapter kOsc = FMGUIDE_CHAPTER("Osciladores, sub y ruido", kOscBlocks);
const Chapter kFilter = FMGUIDE_CHAPTER("El filtro", kFilterBlocks);
const Chapter kMod = FMGUIDE_CHAPTER("Envolventes, LFO y matriz", kModBlocks);
const Chapter kFx = FMGUIDE_CHAPTER("Los efectos", kFxBlocks);
const Chapter kPlay = FMGUIDE_CHAPTER("Voces, escala y arpegio", kPlayBlocks);
const Chapter kSeq = FMGUIDE_CHAPTER("El secuenciador", kSeqBlocks);

}  // namespace shrub
}  // namespace fmguide

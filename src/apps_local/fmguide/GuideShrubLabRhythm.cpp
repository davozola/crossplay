#include "GuideData.h"
#include "GuideShrubChapters.h"
#include "SynthFigures.h"

// SHRUB, LABORATORIO: drums one sound at a time, then textures, a drone and
// risers. Every page, knob and value named here exists on V1.39 with that
// name and range (checked in the editor's engine on 27 November 2026); the
// results were not heard, and each recipe says so.

namespace fmguide {
namespace shrub {

namespace {

using namespace synthfig;

constexpr Block kDrumsBlocks[] = {
    P("Con un sintetizador se pueden fabricar bombos, cajas y platos, y es una de las mejores formas de entender las "
      "envolventes. Pero conviene saber desde el principio qué es SHRUB y qué no es."),
    H("Lo que puede y lo que no"),
    R("Puede", "Sintetizar un sonido de percusión por preset: un bombo, una caja, un plato, un golpe raro."),
    R("Puede", "Tocar ese sonido en ritmo con el secuenciador o el arpegiador, a tempo y con swing."),
    R("No puede", "Sonar como una caja de ritmos: un preset es un solo sonido, no un kit."),
    R("No puede", "Tener un sonido distinto en cada tecla, ni varias pistas de batería a la vez."),
    N("SI QUIERES RITMOS",
      "Para patrones de batería completos, X0X, SLOOP o JIANT FM son cajas de ritmos para la FM-1. Lo que hagas aquí "
      "puedes grabarlo en el ordenador por USB y usarlo como muestra en otro sitio."),
    H("Las tres piezas de casi todo golpe"),
    B("Un tono que cae muy rápido de agudo a grave: el cuerpo."),
    B("Un ruido muy corto: el ataque, el chasquido o el roce."),
    B("Una envolvente de volumen sin sustain: suena y se apaga sola."),
    FIG(Kick, "Un bombo: un seno cuyo tono cae en milésimas mientras el volumen se apaga."),
    H("Dos detalles de SHRUB"),
    B("ENV 2 y ENV 3 son compartidas: si repites golpes muy seguidos, la nueva nota arranca la envolvente desde donde "
      "estaba. Con decays cortos no se nota."),
    B("Para tocar sin mantener las teclas, graba los golpes con el secuenciador paso a paso."),
    LINK(Seq, "El secuenciador paso a paso"),
};

constexpr Block kKickBlocks[] = {
    P("Un bombo electrónico: un seno grave con una caída de tono rapidísima al principio, y un decay que decide si es "
      "seco o largo como un 808."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Envolvente de tono con la matriz, envolventes sin sustain y compresión."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(Kick, "Lo que haces: ENV 2 baja el tono muy deprisa; ENV 1 apaga el volumen."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 1: WT POS 1 (seno). Pág. 3: OCT -1."),
    S("2",
      "ENV, ENV 1: ATTACK 0.0 ms, DECAY unos 350 ms, SUSTAIN -inf dB, RELEASE unos 80 ms. Escucha: un «bum» suave "
      "y sin ataque."),
    S("3",
      "ENV, ENV 2: ATTACK 0.0 ms, DECAY unos 60 ms, SUSTAIN -inf dB, RELEASE 0.0 ms. Pág. 2: D CURVE en negativo, "
      "-50 %, para que caiga más deprisa al principio."),
    S("4",
      "ENV, MATRIX: una conexión libre, SOURCE ENV 2, DEST A COARSE, AMOUNT +40 %. Escucha: ahora es un bombo, con "
      "golpe. Sube o baja AMOUNT hasta que el golpe te guste."),
    S("5",
      "Un clic de ataque: EDIT, SUB/NOISE, pág. 2: COLOR WHITE, NOISE 0 %. ENV, ENV 3: DECAY unos 10 ms, SUSTAIN -inf "
      "dB. Matriz: SOURCE ENV 3, DEST NOISE LEVEL, AMOUNT +30 %."),
    S("6", "FX, rack: hueco 1 DISTORTION en SOFT CLIP (DRIVE 30 %, MIX 40 %), hueco 4 COMPRESSOR en ON."),
    S("7", "Toca siempre la misma tecla, la más baja con OCT- si quieres más grave."),
    H("Variaciones"),
    B("808: DECAY de ENV 1 a 1.50 s y AMOUNT de la matriz más bajo, +15 %."),
    B("Más duro: DISTORTION en HARD CLIP."),
    B("La tabla ICANHASKICK, en OSC A, está pensada para esto."),
    H("Guardar"),
    S("1", "SAVE. Nombre KICK, categoría MISC. SAVE."),
};

constexpr Block kSnareBlocks[] = {
    P("Una caja: un tono corto que da cuerpo y un ruido filtrado que hace de bordonera. Aquí aprendes a mandar al "
      "filtro solo una de las fuentes."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Ruido, enrutar fuentes al filtro, paso alto y el equilibrio entre tono y ruido."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(FilterTypes, "El ruido pasa por un HP: se queda solo con los agudos, el «chas» de la caja."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 1: WT POS 3 (triángulo), LEVEL 50 %."),
    S("2", "EDIT, SUB/NOISE, pág. 2: COLOR WHITE, NOISE 80 %."),
    S("3",
      "EDIT, FILTER, pág. 3: NOISE en ON y OSC A en OFF. Solo el ruido pasa por el filtro. Pág. 1: HP 12, CUTOFF unos "
      "1500 Hz, RES 10 %."),
    S("4", "ENV, ENV 1: ATTACK 0.0 ms, DECAY unos 180 ms, SUSTAIN -inf dB, RELEASE unos 100 ms."),
    S("5",
      "Golpe de tono: ENV, ENV 2: DECAY unos 40 ms, SUSTAIN -inf dB. Matriz: SOURCE ENV 2, DEST A COARSE, AMOUNT +25 "
      "%."),
    S("6", "FX, rack, hueco 5 (REVERB): MODE PLATE, SIZE 30 %, MIX 15 %."),
    H("Variaciones"),
    B("Más cuerpo: sube LEVEL de OSC A. Más bordonera: sube NOISE."),
    B("Clap: ruido solo (OSC A a 0 %), BP 12 a unos 1200 Hz y DECAY de ENV 1 a unos 120 ms."),
    B("Más sucia: COLOR CRUSH."),
    H("Guardar"),
    S("1", "SAVE. Nombre SNARE, categoría MISC. SAVE."),
};

constexpr Block kHatBlocks[] = {
    P("Un charles es casi solo ruido muy agudo y muy corto. Cerrado dura unas centésimas; abierto, medio segundo. Con "
      "esa única diferencia ya tienes los dos."),
    R("Nivel", "Básico."),
    R("Aprendes", "Ruido blanco, filtro paso alto de pendiente fuerte y el decay como forma del sonido."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(Slopes, "Un HP 24 corta los graves de forma tajante: deja solo el siseo."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 1: LEVEL 0 %. EDIT, SUB/NOISE, pág. 2: COLOR WHITE, NOISE 100 %."),
    S("2", "EDIT, FILTER, pág. 3: NOISE en ON. Pág. 1: HP 24, CUTOFF unos 7000 Hz, RES 20 %."),
    S("3",
      "ENV, ENV 1: ATTACK 0.0 ms, DECAY unos 60 ms, SUSTAIN -inf dB, RELEASE unos 40 ms. Escucha: un «ts» seco, el "
      "charles cerrado."),
    S("4", "Para el abierto, DECAY unos 400 ms y RELEASE unos 300 ms."),
    S("5", "Para tocarlo en corcheas sin esfuerzo: ARP en ON, RATE 1/8, LATCH ON, y una tecla."),
    H("Variaciones"),
    B("Metálico, a lo 808: OSC A con cuadrada (WT POS 4), OCT +3 y WARP RM (B), y OSC A también en ON en el filtro."),
    B("Más suave: COLOR PINK."),
    B("Con CHANCE del arpegio al 70 %, se salta golpes al azar."),
    H("Guardar"),
    S("1", "SAVE. Nombre HIHAT, categoría MISC. SAVE."),
};

constexpr Block kPercBlocks[] = {
    P("Golpes que no existen en ninguna batería: una ráfaga de ruido que hace sonar un filtro comb afinado, con el "
      "tono "
      "cambiando al azar en cada golpe. A medio camino entre una marimba rota y un ordenador antiguo."),
    R("Nivel", "Avanzado."),
    R("Aprendes", "Filtros comb, una envolvente que dispara el ruido, la fuente RAND y el arpegiador como generador."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 1: LEVEL 0 %. EDIT, SUB/NOISE, pág. 2: COLOR WHITE, NOISE 0 %."),
    S("2",
      "ENV, ENV 3: ATTACK 0.0 ms, DECAY unos 15 ms, SUSTAIN -inf dB. Matriz: SOURCE ENV 3, DEST NOISE LEVEL, AMOUNT "
      "+80 %. Cada nota es ahora una ráfaga de ruido."),
    S("3",
      "EDIT, FILTER, pág. 3: NOISE en ON. Pág. 1: ALGORITHM hasta COMB +, CUTOFF unos 400 Hz, RES 85 %. Escucha: la "
      "ráfaga hace sonar el comb como una cuerda."),
    S("4", "ENV, ENV 1: ATTACK 0.0 ms, DECAY unos 1.50 s, SUSTAIN -inf dB, RELEASE unos 1.00 s: que el comb suene."),
    S("5", "Matriz: SOURCE RAND, DEST CUTOFF, AMOUNT +25 %. Cada golpe, un tono distinto."),
    S("6", "ARP en ON, ALGORITHM RANDOM, RATE 1/16, CHANCE 60 %, LATCH ON. Mantén dos notas."),
    S("7", "FX, rack: DELAY PING-PONG (1/16., FEEDBACK 40 %) y REVERB en PLATE."),
    N("PLATE, NO HALL", "Con un filtro comb, la reverb HALL puede dejarlo mudo: comparten memoria. Usa PLATE."),
    H("Variaciones"),
    B("Prueba FLANGE + o PHASE 24+ en vez de COMB +."),
    B("CHAOS 1X en vez de RAND: cambios de tono que se mueven, pero no al azar puro."),
    B("Con SCALE en una escala pentatónica, el arpegio queda siempre afinado."),
    H("Guardar"),
    S("1", "SAVE. Nombre COMB PERC, categoría MISC. SAVE."),
};

constexpr Block kAmbientBlocks[] = {
    P("Un paisaje que se toca solo: notas que salen al azar pero siempre dentro de una escala, que nunca se repiten "
      "igual y se pierden en ecos y reverb. Lo dejas sonar y cambia durante minutos."),
    R("Nivel", "Avanzado."),
    R("Aprendes", "Arpegiador aleatorio con CHANCE, escalas, caos, azar por nota y efectos largos."),
    R("Parte de", "Tu PLUCK o tu CINEMA PAD, o un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    PANEL(cSel | cArp | cKnobs, "SEL (SCL) para la escala; ARP para el arpegiador."),
    S("1", "SEL (SCL), pág. 1: KEY D, SCALE Minor Pentatonic. Pág. 2: OUT OF KEY CORRECT."),
    S("2",
      "ARP, ARP otra vez. ALGORITHM RAND DRIFT. Pág. 1: RATE 1/8, GATE 150 %, OCTAVES 3, CHANCE 60 %. Pág. 2: LATCH "
      "ON."),
    S("3", "Toca tres o cuatro notas de la escala y suelta."),
    S("4", "GLO, VOICING, pág. 2: RAND CUT 25 %, RAND PAN 50 %. Cada nota con otro brillo y en otro sitio."),
    S("5",
      "LFO, CHAOS: CHAOS 1 unos 0.10 Hz. Matriz: SOURCE CHAOS 1X, DEST A WT POS, AMOUNT +40 %; otra conexión, SOURCE "
      "CHAOS 1Y, DEST CUTOFF, AMOUNT +20 %."),
    S("6",
      "FX: DELAY PING-PONG, TIME L 1/4., TIME R 1/8., FEEDBACK 55 %, MIX 35 %. REVERB HALL, SIZE 90 %, DECAY 80 %, MIX "
      "40 %."),
    FIG(Polyrhythm, "Si grabas dos mandos en pistas de largos distintos, el conjunto tarda mucho en repetirse."),
    H("Variaciones"),
    B("Graba el CUTOFF en una pista de 3 compases y FAT en una de 5: el sonido solo se repite cada 15."),
    B("Más lento: TEMPO a 70 BPM y RATE 1/4."),
    B("Cambia de escala en directo: KEY y SCALE con el arpegio sonando."),
    H("Guardar"),
    S("1", "SAVE. Nombre AMBIENT GEN, categoría SEQ. SAVE."),
};

constexpr Block kDroneBlocks[] = {
    P("Un sonido sostenido que no termina y va cambiando muy despacio, durante minutos. Útil para meditar, para "
      "ensayar encima o como base de una pieza."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Latch, LFO muy lentos a tempo, movimientos independientes en cada oscilador y FAT."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 1: ALGORITHM hasta SAW>SQUARE. Pág. 2: VOICES 3, DETUNE 10 %."),
    S("2", "EDIT, OSC B: ALGORITHM hasta MODEL D, LEVEL 60 %. Pág. 3: OCT -1, FINE +5 ct."),
    S("3", "EDIT, FILTER, pág. 3: OSC A y OSC B en ON. Pág. 1: MG LOW 18, CUTOFF unos 900 Hz, RES 30 %."),
    S("4", "ENV, ENV 1: ATTACK unos 3.00 s, SUSTAIN 0.0 dB, RELEASE unos 6.00 s."),
    S("5",
      "LFO, LFO 3: pág. 2, BPM ON, MODE FREE. Pág. 1: RATE 8 bar, DEST A WT POS, AMOUNT +50 %. LFO 4 igual, con "
      "RATE 12 bar y DEST B WT POS: los dos osciladores se mueven a ritmos distintos."),
    S("6", "LFO, CHAOS: CHAOS 2 unos 0.05 Hz. Matriz: SOURCE CHAOS 2X, DEST FAT, AMOUNT +40 %."),
    S("7", "FX: CHORUS en ON, REVERB en HALL con SIZE 100 % y MIX 45 %."),
    S("8",
      "Toca una quinta grave, por ejemplo Do y Sol, y pulsa OCT- y OCT+ a la vez: el latch lo deja sonando. Vuelve a "
      "pulsarlos para parar."),
    H("Variaciones"),
    B("Sube RES y deja que el filtro silbe sobre el drone."),
    B("Un pulso lento: en el rack, PUMP con RATE bar."),
    B("Compara con el preset 54 NEWDRONE."),
    H("Guardar"),
    S("1", "SAVE. Nombre DRONE, categoría PAD. SAVE."),
};

constexpr Block kRiserBlocks[] = {
    P("El ruido que sube y sube antes de que entre el tema, y su contrario, el que cae al acabar. Se hacen con una "
      "rampa larga que abre el filtro y sube el tono, sincronizada al tempo para que llegue justo a tiempo."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "LFO en modo ENVELOPE como rampa a tempo, ruido filtrado y modulación a la vez de varios destinos."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    S("1", "EDIT, OSC A, pág. 1: LEVEL 30 %. EDIT, SUB/NOISE, pág. 2: COLOR WHITE, NOISE 70 %."),
    S("2", "EDIT, FILTER, pág. 3: OSC A y NOISE en ON. Pág. 1: BP 12, CUTOFF unos 200 Hz, RES 40 %."),
    S("3",
      "LFO, LFO 1: pág. 2, BPM ON, MODE ENVELOPE, BIPOLAR OFF. Pág. 1: SKEW SAW UP, RATE 4 bar, DEST CUTOFF, AMOUNT "
      "+80 %. Una sola rampa de cuatro compases por nota."),
    S("4", "Matriz: SOURCE LFO 1, DEST A SEMI, AMOUNT +50 %. El tono sube con el filtro."),
    S("5", "ENV, ENV 1: ATTACK unos 2.00 s, SUSTAIN 0.0 dB, RELEASE unos 1.00 s."),
    S("6", "FX: REVERB HALL, MIX 35 %; DELAY 1/8, FEEDBACK 40 %. Con PLAY en marcha, toca la nota en el compás 1."),
    H("El que cae"),
    B("Mismo sonido con SKEW en SAW DOWN: empieza abierto y se cierra en cuatro compases."),
    B("Con RATE 1 bar es un barrido corto, para un corte de un compás."),
    H("Variaciones"),
    B("Riser tonal: NOISE a 0 % y OSC A con VOICES 7, DETUNE 40 %."),
    B("A mano: graba el CUTOFF con REC y te queda en una pista, repetible."),
    B("Un impacto al final: en el rack, DISTORTION en DOWNSAMPLE con MIX bajo."),
    H("Guardar"),
    S("1", "SAVE. Nombre RISER, categoría MISC. SAVE."),
};

}  // namespace

const Chapter kLabDrums = FMGUIDE_CHAPTER("Batería con un sinte", kDrumsBlocks);
const Chapter kLabKick = FMGUIDE_CHAPTER("Kick", kKickBlocks);
const Chapter kLabSnare = FMGUIDE_CHAPTER("Snare", kSnareBlocks);
const Chapter kLabHat = FMGUIDE_CHAPTER("Hi-hat", kHatBlocks);
const Chapter kLabPerc = FMGUIDE_CHAPTER("Percusión experimental", kPercBlocks);
const Chapter kLabAmbient = FMGUIDE_CHAPTER("Textura ambiental generativa", kAmbientBlocks);
const Chapter kLabDrone = FMGUIDE_CHAPTER("Drone evolutivo", kDroneBlocks);
const Chapter kLabRiser = FMGUIDE_CHAPTER("Risers y transiciones", kRiserBlocks);

}  // namespace shrub
}  // namespace fmguide

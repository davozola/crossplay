#include "GuideData.h"
#include "GuideShrubChapters.h"
#include "SynthFigures.h"

// SHRUB, LABORATORIO: how the lab works, then basses and leads. Every page,
// knob and value named here exists on V1.39 with that name and range (checked
// in the editor's engine on 27 November 2026); the results were not heard,
// and each recipe says so.

namespace fmguide {
namespace shrub {

namespace {

using namespace synthfig;

constexpr Block kIntroBlocks[] = {
    P("Cada receta construye un sonido concreto partiendo de cero, y por el camino enseña una idea de síntesis: qué "
      "hace un filtro resonante, para qué sirve el unison, cómo se fabrica un bombo con un seno. No son presets para "
      "copiar, son ejercicios."),
    H("Cómo está hecha cada receta"),
    R("Nivel", "Básico, intermedio o avanzado."),
    R("Aprendes", "Los conceptos que trabaja."),
    R("Parte de", "Casi siempre un hueco INIT, del 101 al 128."),
    R("Paso a paso", "Qué botón, qué página, qué mando y qué valor; y qué deberías oír."),
    R("Variaciones", "Cambios para seguir probando."),
    R("Guardar", "Dónde y con qué nombre."),
    N("ORIENTATIVAS",
      "Todos los parámetros y valores de las recetas existen en SHRUB V1.39 con ese nombre y ese rango: están "
      "comprobados en el motor del editor oficial. Lo que no está comprobado es el resultado de oído. Tómalas como "
      "punto de partida y ajusta escuchando."),
    H("Cómo leer los valores"),
    B("Se escriben como los enseña la pantalla: CUTOFF 400 Hz, ATTACK 5.0 ms, SUSTAIN -12.0 dB, RATE 1/16."),
    B("«Unos 400 Hz» quiere decir que gires hasta que se acerque: los mandos van a saltos y no siempre se llega al "
      "número exacto."),
    B("«EDIT, FILTER, pág. 3, KNOB 1» quiere decir: pulsa EDIT hasta la parte FILTER, cambia a la página 3 con "
      "SELECT y gira el KNOB 1."),
    B("Para conectar algo en la matriz: ENV hasta MATRIX, KNOB 1 elige la conexión libre, KNOB 2 la fuente, KNOB 3 el "
      "destino y KNOB 4 la cantidad."),
    H("Antes de cada receta"),
    S("1", "Gira PRESETS hasta un hueco INIT libre, del 101 en adelante."),
    S("2",
      "Recuerda que en INIT el filtro no recibe nada. Si la receta usa el filtro, el primer paso siempre es ponerlo en "
      "ON para OSC A en su página 3."),
    S("3", "Usa auriculares para los bajos y los bombos: el altavoz no da graves."),
    N("POLIFONIA",
      "Los sonidos con mucho unison dejan sitio para menos notas. Si al tocar acordes desaparecen notas, mira POLY en "
      "VOICING y baja VOICES."),
    LINK(LabSub, "Primera receta: un sub bass"),
};

constexpr Block kSubBlocks[] = {
    P("Un bajo grave y limpio, casi un seno puro, que se siente más que se oye. La base de casi todo el hip hop, el "
      "trap y la electrónica de baile."),
    R("Nivel", "Básico."),
    R("Aprendes", "Formas de onda, octavas, MONO y LEGATO, glide y la envolvente de volumen."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(Waveforms, "El seno (SINE) es la onda sin armónicos: solo la fundamental. Por eso suena tan redondo."),
    H("Paso a paso"),
    PANEL(cEdit | cAlgorithm | cSelect | cKnob1, "EDIT, OSC A: KNOB 1 es WT POS. SELECT pasa a la página 3."),
    S("1", "EDIT, OSC A, pág. 1: WT POS 1. En BASIC SHAPES es el seno. Escucha: un tono redondo, sin brillo."),
    S("2", "Pág. 3, KNOB 1, OCT -2. Escucha: dos octavas más grave; con auriculares, un zumbido profundo."),
    S("3", "GLO, VOICING: MONO en MONO y LEGATO en ON. Ahora toca una nota a la vez, como un bajo de verdad."),
    S("4", "Mismo sitio, GLIDE unos 40 ms. Escucha: al ligar dos notas, la segunda se desliza un poco."),
    S("5",
      "ENV, ENV 1: ATTACK 2.0 ms, SUSTAIN 0.0 dB, RELEASE unos 60 ms. Sin chasquido al principio y corte limpio al "
      "soltar."),
    S("6",
      "Para que se oiga también en altavoces pequeños: FX, rack, hueco 1 (DISTORTION), MODE SOFT SAT. FX otra vez: "
      "DRIVE 20 %, MIX 30 %. Añade armónicos que el oído reconoce como el mismo grave."),
    H("Variaciones"),
    B("En el paso 1, WT POS 3 (triángulo): un poco más de presencia sin perder redondez."),
    B("Estilo 808: ENV 1 con SUSTAIN -inf dB y DECAY 1.50 s; la nota se apaga sola."),
    B("Más pegada: matriz, SOURCE ENV 2, DEST A COARSE, AMOUNT +15 %, y ENV 2 con DECAY 40 ms y SUSTAIN -inf dB. La "
      "nota arranca un poco más aguda y cae."),
    H("Guardar"),
    S("1", "SAVE. Nombre SUB BASS, categoría BASS. SAVE."),
};

constexpr Block kAcidBlocks[] = {
    P("La línea chillona y elástica de la Roland TB-303: un diente de sierra por un filtro resonante que se abre y se "
      "cierra en cada nota, con notas que se deslizan unas a otras."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Filtro ladder, resonancia, envolvente de filtro, legato con glide y distorsión."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(CutoffRes, "La clave del ácido: CUTOFF bajo y RES alta. La envolvente abre el filtro y el pico chilla."),
    H("Paso a paso"),
    PANEL(cEdit | cAlgorithm | cSelect | cKnobs,
          "EDIT hasta FILTER. ALGORITHM elige el tipo; SELECT, la página; los KNOB, los valores."),
    S("1", "EDIT, OSC A, pág. 3: OCT -1. La sierra de INIT queda una octava abajo."),
    S("2", "EDIT hasta FILTER, pág. 3: OSC A en ON. Desde aquí el filtro actúa."),
    S("3",
      "Pág. 1: ALGORITHM hasta ACID LADDER. CUTOFF unos 300 Hz, RES unos 70 %, DRIVE 30 %. Escucha: oscuro y con un "
      "silbido en el corte."),
    S("4", "Pág. 2: ENV 2 +60 %. Ahora ENV 2 abre el filtro en cada nota."),
    S("5",
      "ENV, ENV 2: ATTACK 0.0 ms, DECAY unos 250 ms, SUSTAIN -inf dB, RELEASE unos 50 ms. Escucha el «uau» al "
      "principio de cada nota."),
    S("6", "ENV, ENV 1: RELEASE unos 30 ms. Las notas se cortan secas, como en la 303."),
    S("7",
      "GLO, VOICING: MONO, LEGATO en ON, GLIDE unos 60 ms. Toca ligando notas: se deslizan sin volver a abrir el "
      "filtro."),
    S("8", "FX, rack, hueco 1, MODE TUBE; FX otra vez: DRIVE 30 %, MIX 40 %. Más mordiente."),
    S("9", "Para tocarlo en bucle: ARP en ON, RATE 1/16, OCTAVES 2, GATE 120 %. Mantén una o dos notas."),
    H("Variaciones"),
    B("Sube y baja CUTOFF mientras suena el arpegio, y grábalo con REC: es lo que hacen los temas de acid."),
    B("Cambia OSC A a cuadrada (WT POS 4): la 303 tenía las dos ondas."),
    B("Prueba la tabla ACID en OSC A, o el preset 10 THREE OH THREE para comparar."),
    B("FX, rack, hueco 3 (DELAY): MODE PING-PONG, TIME L y R en 1/8., FEEDBACK 30 %, MIX 20 %."),
    H("Guardar"),
    S("1", "SAVE. Nombre ACID LINE, categoría BASS (o SEQ si lo guardas con arpegio). SAVE."),
};

constexpr Block kReeseBlocks[] = {
    P("Dos dientes de sierra casi iguales, un pelo desafinados, que se baten entre sí y suenan como un motor que gira. "
      "Nació en el techno de Detroit y es la columna del drum and bass."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Batido entre osciladores, FINE, el LFO lento al filtro y por qué un bajo va en MONO."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    PANEL(cEdit | cSelect | cKnobs, "EDIT pasa de OSC A a OSC B. SELECT, las páginas."),
    S("1", "EDIT, OSC A, pág. 3: OCT -1."),
    S("2",
      "EDIT, OSC B, pág. 1: LEVEL 75 %. Su tabla también es BASIC SHAPES; WT POS 2 (sierra). Pág. 3: OCT -1, FINE "
      "+12 ct. Escucha: el sonido empieza a «girar»."),
    S("3", "EDIT, FILTER, pág. 3: OSC A y OSC B en ON."),
    S("4", "Pág. 1: ALGORITHM hasta MG LOW 24. CUTOFF unos 600 Hz, RES 15 %."),
    S("5",
      "LFO, LFO 1: pág. 2, BPM ON y MODE FREE. Pág. 1: RATE 2 bar, DEST CUTOFF, AMOUNT +20 %. El filtro respira "
      "despacio."),
    S("6", "GLO, VOICING: MONO, LEGATO en ON. ENV, ENV 1: RELEASE unos 100 ms."),
    S("7", "FX, rack, hueco 1, MODE TUBE; FX otra vez: DRIVE 25 %, MIX 30 %."),
    H("Variaciones"),
    B("Más batido: FINE de OSC B a +25 ct. Menos: +5 ct."),
    B("Con unison en vez de dos osciladores: OSC B a 0 %, OSC A con VOICES 2 y DETUNE 15 %."),
    B("Las tablas REESE y LONGREECE vienen hechas para esto. Y el preset 18 DETH REECE."),
    B("FX, rack, hueco 4 (COMPRESSOR): MODE ON y OTT en ON con MIX 30 %. El sonido moderno del neurofunk."),
    H("Guardar"),
    S("1", "SAVE. Nombre REESE, categoría BASS. SAVE."),
};

constexpr Block kSynthwaveBlocks[] = {
    P("El bajo que corre en semicorcheas por debajo de la música de los 80 y del synthwave: una sierra con un filtro "
      "que pellizca cada nota, saltando de octava."),
    R("Nivel", "Básico."),
    R("Aprendes", "Envolvente de filtro corta, el sub sin filtrar y el arpegiador como secuenciador de bajo."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    PANEL(cArp | cKnobs | cGlo, "ARP: RATE, GATE y OCTAVES. GLO, GLOBAL: TEMPO."),
    S("1", "EDIT, OSC A, pág. 3: OCT -1."),
    S("2", "EDIT, SUB/NOISE, pág. 1: SHAPE SINE, OCT -1, SUB 40 %. Refuerza el grave sin pasar por el filtro."),
    S("3",
      "EDIT, FILTER, pág. 3: OSC A en ON (SUB se queda en OFF). Pág. 1: MG LOW 24, CUTOFF unos 500 Hz, RES 20 %. Pág. "
      "2: ENV 2 +40 %."),
    S("4", "ENV, ENV 2: ATTACK 0.0 ms, DECAY unos 200 ms, SUSTAIN -inf dB. Cada nota da un golpe de brillo."),
    S("5", "ENV, ENV 1: RELEASE unos 40 ms."),
    S("6", "GLO, GLOBAL, pág. 1: TEMPO 100 BPM."),
    S("7", "ARP, ARP otra vez (encendido). ALGORITHM: UP. RATE 1/16, GATE 60 %, OCTAVES 2."),
    S("8", "Mantén una nota: sale nota, octava, nota, octava. Cambia de nota cada compás."),
    S("9", "FX, rack, hueco 2 (CHORUS): MODE ON. FX hasta CHORUS: MIX 20 %. Un poco de anchura ochentera."),
    H("Variaciones"),
    B("ARP, pág. 2: LATCH ON, y cambia de nota sin mantenerla."),
    B("Sube DECAY de ENV 2 a 400 ms: menos pellizco, más presencia."),
    B("Pon la tabla MODEL D en OSC A: más carácter analógico."),
    H("Guardar"),
    S("1", "SAVE. Nombre SYNTHWAVE BASS, categoría BASS. SAVE."),
};

constexpr Block kLeadBlocks[] = {
    P("Un solista de los de toda la vida: dos sierras un poco desafinadas, un filtro que se abre al atacar, glide "
      "entre "
      "notas y vibrato. Suena a Minimoog o a Prophet."),
    R("Nivel", "Básico."),
    R("Aprendes", "Desafinar dos osciladores, glide en MONO, un LFO como vibrato y el delay a tempo."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    H("Paso a paso"),
    S("1", "EDIT, OSC B, pág. 1: WT POS 2 (sierra), LEVEL 75 %. Pág. 3: FINE +8 ct."),
    S("2", "EDIT, FILTER, pág. 3: OSC A y OSC B en ON. Pág. 1: MG LOW 24, CUTOFF unos 2000 Hz, RES 20 %."),
    S("3", "Pág. 2: ENV 2 +20 %. ENV, ENV 2: DECAY unos 400 ms, SUSTAIN -12.0 dB."),
    S("4", "GLO, VOICING: MONO, LEGATO en ON, GLIDE unos 80 ms."),
    S("5",
      "Vibrato: LFO, LFO 2, pág. 2: BPM OFF, MODE FREE. Pág. 1: RATE unos 5.5 Hz, DEST MAST TUNE y AMOUNT muy poco: "
      "empieza en +2 % y sube hasta que se note sin desafinar."),
    S("6", "FX, rack, hueco 3 (DELAY): MODE PING-PONG. FX hasta DELAY: TIME L y R 1/8., FEEDBACK 30 %, MIX 20 %."),
    S("7", "Hueco 5 (REVERB): MODE PLATE, MIX 15 %."),
    FIG(Glide, "En LEGATO, ligando notas, la altura se desliza durante el tiempo de GLIDE."),
    H("Variaciones"),
    B("Vibrato que llega tarde, como un violinista: mira la receta de cuerdas, usa AUX."),
    B("Un tono más agresivo: OSC B a cuadrada (WT POS 4) y DRIVE del filtro al 30 %."),
    B("Si tienes un teclado MIDI con rueda: matriz, SOURCE MOD WHEEL, DEST MAST TUNE, en vez del LFO fijo."),
    H("Guardar"),
    S("1", "SAVE. Nombre ANALOG LEAD, categoría LEAD. SAVE."),
};

constexpr Block kDigitalBlocks[] = {
    P("Un solista que no podría hacer un sinte analógico: la onda cambia de forma mientras suena, moviéndose por la "
      "wavetable, con un golpe de warp al principio de cada nota."),
    R("Nivel", "Intermedio."),
    R("Aprendes", "Modular WT POS, warp SYNC con envolvente y la matriz."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(Wavetable, "El LFO mueve WT POS: el oscilador recorre las ondas de la tabla mientras suena."),
    H("Paso a paso"),
    PANEL(cEdit | cAlgorithm | cKnob1 | cKnob2 | cKnob3,
          "EDIT, OSC A: ALGORITHM la tabla; KNOB 1 a 3, WT POS, WARP y AMOUNT."),
    S("1",
      "EDIT, OSC A, pág. 1: ALGORITHM hasta FMFM (o RAZOR). Gira WT POS de punta a punta para oír la tabla entera; "
      "déjalo al principio."),
    S("2", "Pág. 2: VOICES 3, DETUNE 15 %."),
    S("3",
      "LFO, LFO 1: pág. 2, BPM ON, MODE RETRIG. Pág. 1: RATE 1/2, DEST A WT POS, AMOUNT +40 %. Escucha: la onda "
      "barre la tabla cada media nota."),
    S("4", "EDIT, OSC A, pág. 1: WARP SYNC, AMOUNT 20 %."),
    S("5",
      "ENV, MATRIX: KNOB 1 una conexión libre, SOURCE ENV 2, DEST A WARP, AMOUNT +50 %. ENV, ENV 2: DECAY unos 300 ms, "
      "SUSTAIN -inf dB. Escucha: un «zap» al principio de cada nota."),
    S("6", "GLO, VOICING: MONO, LEGATO en ON, GLIDE unos 40 ms."),
    S("7", "FX, rack: DISTORTION MODE SOFT CLIP (DRIVE 20 %), DELAY 1/8 y REVERB PLATE."),
    H("Variaciones"),
    B("LFO 1 con SKEW en SAW UP: la tabla sube y vuelve de golpe."),
    B("Cambia el destino a B WT POS y sube OSC B: los dos osciladores se cruzan."),
    B("Prueba el preset 39 INIT TABLEANIM para ver otra forma de animar la tabla."),
    H("Guardar"),
    S("1", "SAVE. Nombre DIGI LEAD, categoría LEAD. SAVE."),
};

constexpr Block kSupersawBlocks[] = {
    P("El muro de sierras del trance y del EDM: siete copias de la nota desafinadas y abiertas de lado a lado. Es "
      "también el mejor ejemplo de lo que cuesta el unison en polifonía."),
    R("Nivel", "Básico."),
    R("Aprendes", "Unison, DETUNE, BLEND y WIDTH, y su precio en notas."),
    R("Parte de", "Un hueco INIT."),
    R("Estado", "Orientativa: valores comprobados en el editor, resultado no escuchado."),
    FIG(Unison, "DETUNE separa las copias en tono; WIDTH las reparte de izquierda a derecha."),
    H("Paso a paso"),
    PANEL(cEdit | cSelect | cKnobs, "EDIT, OSC A, pág. 2: VOICES, DETUNE, BLEND y WIDTH."),
    S("1", "EDIT, OSC A, pág. 2: VOICES 7, DETUNE 30 %, BLEND 75 %, WIDTH +100 %. Escucha: enorme y ancho."),
    S("2", "Mueve DETUNE de 10 % a 60 %: de coro suave a desafinado de verdad. Deja 30 %."),
    S("3", "EDIT, OSC B: WT POS 2, LEVEL 40 %. Pág. 3: OCT +1. Brillo arriba."),
    S("4", "EDIT, FILTER, pág. 3: OSC A y OSC B en ON. Pág. 1: LP 12, CUTOFF unos 5000 Hz."),
    S("5", "ENV, ENV 1: ATTACK 5.0 ms, RELEASE unos 300 ms."),
    S("6", "FX, rack, hueco 5 (REVERB): MODE HALL, MIX 25 %."),
    S("7", "Toca un acorde de tres notas. Mira GLO, VOICING: POLY dice cuántas caben."),
    FIG(VoicePool, "Cada nota con 7 copias ocupa mucho: caben menos notas a la vez."),
    N("SI FALTAN NOTAS",
      "Si un acorde no suena entero, baja VOICES de OSC A a 5, quita el unison de OSC B, o pon QUALITY en DRAFT "
      "(GLOBAL, pág. 5)."),
    H("Variaciones"),
    B("Más anchura sin más voces: en el rack, hueco 6, módulo HYPER/DIM; en sus mandos, DIM MIX 30 %."),
    B("En el hueco 7, módulo PUMP con RATE 1/4: el acorde respira con el tempo, como en un tema de trance."),
    B("Compara con el preset 85 SUPERSAW."),
    H("Guardar"),
    S("1", "SAVE. Nombre SUPERSAW, categoría SYNTH. SAVE."),
};

}  // namespace

const Chapter kLabIntro = FMGUIDE_CHAPTER("Cómo funciona el laboratorio", kIntroBlocks);
const Chapter kLabSub = FMGUIDE_CHAPTER("Sub bass", kSubBlocks);
const Chapter kLabAcid = FMGUIDE_CHAPTER("Acid bass estilo TB-303", kAcidBlocks);
const Chapter kLabReese = FMGUIDE_CHAPTER("Reese bass", kReeseBlocks);
const Chapter kLabSynthwave = FMGUIDE_CHAPTER("Bajo synthwave", kSynthwaveBlocks);
const Chapter kLabLead = FMGUIDE_CHAPTER("Lead analógico", kLeadBlocks);
const Chapter kLabDigital = FMGUIDE_CHAPTER("Lead digital con wavetable", kDigitalBlocks);
const Chapter kLabSupersaw = FMGUIDE_CHAPTER("Supersaw", kSupersawBlocks);

}  // namespace shrub
}  // namespace fmguide

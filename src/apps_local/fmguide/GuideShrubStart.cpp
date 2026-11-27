#include "GuideData.h"
#include "GuideShrubChapters.h"
#include "SynthFigures.h"

// SHRUB, EMPEZAR: what it is, what it can and cannot do, and ten minutes to
// get playing. Checked on 27 November 2026 (see GuideShrub.cpp).

namespace fmguide {
namespace shrub {

namespace {

using namespace synthfig;

constexpr Block kWhatBlocks[] = {
    P("SHRUB convierte la FM-1 en un sintetizador de wavetables (tablas de ondas): una versión reducida de Serum, el "
      "sinte de software más usado en música electrónica, hecha por su propio autor, Steve Duda, de Xfer Records. La "
      "FM-1 deja de ser un sinte FM y suena a Serum: sus osciladores, sus deformaciones de onda, sus filtros y sus "
      "efectos."),
    P("Son tres piezas. El firmware, que se instala en la FM-1. El editor, en shrub.audio/editor, que enseña todo a la "
      "vez, ordena la biblioteca e importa presets de Serum 1. Y el emulador, en shrub.audio/play: la FM-1 entera en "
      "el navegador, para probar sin instalar nada."),
    H("El camino del sonido"),
    FIG(SignalFlow,
        "Dos osciladores, el ruido y el sub entran en un único filtro. Después, ENV 1 da forma al volumen de cada nota "
        "y el rack aplica hasta siete efectos. El sub con DIRECT se salta el filtro y los efectos."),
    N("OJO CON INIT",
      "En la página 3 del filtro decides qué fuentes pasan por él. En el preset INIT están todas en OFF: el filtro no "
      "recibe nada y mover CUTOFF no cambia el sonido hasta que pones OSC A en ON."),
    H("Lo que trae"),
    B("Dos osciladores de wavetables. El mando TABLE ofrece unas 65 tablas de fábrica, el modo ADDITIVE y las tablas "
      "que cargues tú. Cada oscilador tiene unison de hasta 8 voces."),
    B("Los warps de Serum (SYNC, BEND, PWM, ASYM, MIRROR, FM, AM, RM...) y 26 curvas REMAP."),
    B("Un filtro con 76 tipos: SVF, los MG de Serum 1, ladders, combs, flangers, phasers y filtros dobles."),
    B("Tres envolventes, cuatro LFO que se pueden dibujar, dos fuentes de caos y una matriz de 16 conexiones."),
    B("Un rack de siete huecos con once módulos de efectos, del HYPER/DIM al PUMP, con compresión multibanda OTT."),
    B("Escalas, acordes con una tecla, un arpegiador de 19 patrones y un secuenciador que graba notas y el movimiento "
      "de los mandos."),
    B("100 presets de fábrica en siete categorías y 28 huecos INIT para los tuyos."),
    H("Antes de empezar, cuatro cosas"),
    R("Hasta 8 notas", "A la vez, de un total de 16 voces. Un unison gordo deja sitio para menos notas."),
    R("Un solo filtro",
      "Todas las notas comparten el filtro: un acorde se filtra junto. Una nota sola suena como en Serum."),
    R("Velocity fija", "Las 27 teclas tocan siempre con velocity 100. La velocity real llega por MIDI."),
    R("En beta", "Se actualiza a menudo. Esta guía sigue la V1.39 y su manual 1.0, de noviembre de 2026."),
    H("Datos"),
    R("Autor", "Steve Duda, Xfer Records. Gratis, de código cerrado."),
    R("Web", "shrub.audio: instalador, editor, emulador y manual en inglés."),
    R("Comunidad", "El Discord «Shrub Club», enlazado desde la web."),
    N("NO ES DE M-VAVE",
      "Ni lo hace ni lo mantiene M-VAVE, y se instala bajo tu responsabilidad. El manual dice que el instalador solo "
      "escribe la parte que toca una actualización; aun así, lee el capítulo de instalación antes de empezar."),
    LINK(CanCant, "Qué puede y qué no puede hacer"),
    LINK(FirstTen, "Tus primeros 10 minutos con SHRUB"),
};

constexpr Block kCanCantBlocks[] = {
    P("SHRUB es un solo sintetizador muy completo, no una caja de ritmos ni un estudio. Esta tabla sirve para saber "
      "cuándo elegirlo y cuándo te conviene otro firmware de la FM-1."),
    H("Lo que es"),
    R("Arquitectura",
      "Sustractiva con wavetables: dos osciladores, sub, ruido, un filtro, tres envolventes, cuatro LFO, caos, "
      "matriz y siete efectos."),
    R("Polifonía", "Hasta 8 notas, de 16 voces. Con unison alto caben menos. QUALITY en DRAFT deja tocar alguna más."),
    R("Parafonía",
      "Un filtro y ENV 2, ENV 3, LFO y caos compartidos por todas las notas. ENV 1 y el azar por nota sí son de "
      "cada nota."),
    R("Multitímbrico", "No: suena un preset a la vez, en un canal MIDI."),
    R("Percusión",
      "Sintetiza bombos, cajas o platos uno a uno, como sonidos sueltos. No es una caja de ritmos: no tiene pistas "
      "de batería ni un sonido distinto por tecla."),
    R("Secuenciación",
      "Por preset, 27 secuencias de 8 pistas cada una. Graban notas (una a la vez por pista) y movimientos de "
      "mandos."),
    R("Arpegiador", "19 patrones, de 1 a 4 octavas, CHANCE, OFFSET, REPEATS, SWING y LATCH."),
    R("Automatización",
      "Casi cualquier mando de sonido se graba en una pista. Los de tipo o modo saltan, no se deslizan."),
    R("MIDI",
      "Entra por USB, por el minijack MIDI IN y por Bluetooth. Sale por USB y Bluetooth. No hay salida MIDI por el "
      "minijack."),
    R("Sincronía",
      "Sigue el reloj MIDI que le llega y manda el suyo. Por Bluetooth, una FM-1 en LEAD lleva el tempo de otra."),
    R("Audio por USB", "Sí. USB LEVEL, en GLOBAL, decide si sigue al MASTER."),
    R("Serum", "Importa presets de Serum 1 (.fxp), con diferencias. Exporta a Serum 2. No importa de Serum 2."),
    R("Memoria",
      "128 huecos. Cada uno guarda el sonido y sus 27 secuencias en alrededor de 1 KB. Tablas propias: tres bancos "
      "de 16."),
    H("Lo que no hace"),
    P("El manual lo dice claro: no hay rise ni delay en los LFO, ni LFO 5 a 8, ni curva de portamento, ni curvas de "
      "distorsión propias, ni osciladores de muestras, ni síntesis granular ni espectral, ni los clips y el pattern "
      "arp de Serum 2, ni salida MIDI por el minijack."),
    H("Cuándo usar otro firmware"),
    R("Ritmos y patrones", "X0X, SLOOP o JIANT FM: cajas de ritmos con varias pistas."),
    R("FM clásico", "El original de M-VAVE o FM-1+VA, que lo amplía."),
    R("Varias pistas de sinte", "Felucca o JIANT FM."),
    R("Diseñar sonido", "SHRUB: es el que más lejos llega en un solo timbre."),
    N("LA GUIA GENERAL",
      "La guía ANTES DE NADA, en la pantalla de inicio de FM-1 GUÍAS, compara todos los firmwares y explica cómo "
      "pasar de uno a otro sin perder nada."),
};

constexpr Block kFirstTenBlocks[] = {
    P("Diez paradas cortas para empezar a tocar sin leer el manual entero. Necesitas la FM-1 con SHRUB instalado y, "
      "mejor, auriculares: los graves no se oyen en el altavoz."),
    H("1. Explorar los presets"),
    PANEL(cPresets | cAlgorithm | cOct | cKeys,
          "PRESETS recorre los sonidos; ALGORITHM salta de categoría; OCT- y OCT+ cambian de octava."),
    S("1", "Gira PRESETS. El sonido se carga cuando dejas de girar. Toca las teclas."),
    S("2", "Gira ALGORITHM para saltar de categoría: BASS, LEAD, MISC, PAD, PLUCK, SEQ y SYNTH."),
    S("3", "OCT- y OCT+ bajan y suben una octava. Con los dos a la vez, las notas se quedan sonando (latch)."),
    P("Prueba 10 THREE OH THREE, 42 PAD CHILL HAWK, 71 CLASSIC ARP y 85 SUPERSAW."),
    H("2. Los mandos de HOME"),
    PANEL(cHome | cKnobs, "En HOME, los cuatro KNOB tocan el filtro y la posición de las dos tablas."),
    R("KNOB 1 y 2", "CUTOFF y RES: cuánto brillo deja pasar el filtro y cuánto resuena en el corte."),
    R("KNOB 3 y 4", "A POS y B POS: por dónde se lee la tabla de cada oscilador. Cambia el timbre."),
    R("HOME otra vez", "Los KNOB pasan a ser MACRO 1 a 4. Otra vez, vuelven."),
    H("3. Modificar un sonido"),
    S("1", "Elige 42 PAD CHILL HAWK y mantén un acorde."),
    S("2", "Baja CUTOFF poco a poco: el sonido se oscurece. Súbelo: se abre."),
    S("3",
      "Gira PRESETS para irte. La pantalla dice EDITED / TURN AGAIN: gira otra vez para salir sin guardar, o pulsa "
      "SAVE para quedártelo."),
    H("4. Cambiar la wavetable"),
    PANEL(cEdit | cAlgorithm | cKnob1, "EDIT abre OSC A; ALGORITHM elige la tabla; KNOB 1, WT POS, la recorre."),
    S("1", "Pulsa EDIT: estás en OSC A, página 1."),
    S("2", "Gira ALGORITHM para cambiar de tabla. Su nombre sale arriba, y la forma en 3D en medio."),
    S("3", "Gira KNOB 1, WT POS, mientras tocas: la onda cambia poco a poco."),
    FIG(Wavetable, "Una wavetable es una fila de ondas. WT POS elige cuál suena, y moverla cambia el timbre."),
    H("5. Probar el filtro"),
    PANEL(cEdit | cSelect | cKnob1 | cKnob2 | cAlgorithm,
          "EDIT cuatro veces llega a FILTER. SELECT cambia de página; ALGORITHM, el tipo."),
    S("1", "Pulsa EDIT hasta FILTER. Con SELECT ve a la página 3 y comprueba que OSC A está en ON."),
    S("2", "Vuelve a la página 1. Baja KNOB 1, CUTOFF, y sube KNOB 2, RES: oirás un silbido en el corte."),
    S("3", "Gira ALGORITHM para cambiar de tipo: prueba MG LOW 24, ACID LADDER y BP 12."),
    FIG(CutoffRes, "CUTOFF marca dónde empieza a cortar; RES alta añade un pico justo ahí."),
    H("6. Una modulación sencilla"),
    PANEL(cLfo | cKnob3 | cKnob4, "LFO abre LFO 1. KNOB 3, DEST, elige qué mueve; KNOB 4, AMOUNT, cuánto."),
    S("1", "Pulsa LFO. En la página 1, gira KNOB 3, DEST, hasta A WT POS."),
    S("2", "Sube KNOB 4, AMOUNT, a +30 %. La tabla se mueve sola."),
    S("3", "Vuelve a EDIT, OSC A: el mando WT POS lleva ahora un anillo azul que enseña hasta dónde lo mueve el LFO."),
    H("7. Añadir un efecto"),
    PANEL(cFx | cKnob1 | cKnob4, "FX abre el rack. KNOB 1 elige el hueco; KNOB 4, MODE, el tipo o OFF."),
    S("1", "Pulsa FX. Gira KNOB 1 hasta el hueco que tenga REVERB (en INIT, el 5)."),
    S("2", "Gira KNOB 4, MODE, hasta PLATE. Ya suena."),
    S("3", "Pulsa FX otra vez para entrar en los mandos de cada efecto, por orden del rack. Sube MIX con KNOB 4."),
    H("8. Encender el arpegiador"),
    PANEL(cArp | cKnob1 | cOct, "ARP abre su página y, pulsado otra vez, lo enciende. OCT- y OCT+ juntos: latch."),
    S("1", "Pulsa ARP y otra vez ARP: la pantalla deja de decir ARP OFF."),
    S("2", "Mantén un acorde. KNOB 1, RATE, cambia la figura: 1/16, 1/8..."),
    S("3", "Pulsa OCT- y OCT+ a la vez: el arpegio sigue sin que mantengas las teclas. Otra vez, se para."),
    H("9. Grabar una secuencia corta"),
    PANEL(cRec | cPlay | cSeq, "REC graba, PLAY para. SEQ enseña lo grabado."),
    S("1", "Pulsa REC. Suenan cuatro clics de cuenta, de IN 4 a IN 1."),
    S("2", "Toca una melodía de una nota a la vez, o gira un mando: también se graba su movimiento."),
    S("3", "Pulsa PLAY para parar y otra vez PLAY para oírlo en bucle. SEQ enseña las pistas."),
    N("UNA NOTA A LA VEZ",
      "De fábrica, REC NOTES está en MONO: todas las notas van a una pista, y un acorde se graba como su última "
      "nota. En POLY, cada nota del acorde va a una pista. Está en GLOBAL, página 4."),
    H("10. Guardar tu primer sonido"),
    PANEL(cSave | cKnobs, "SAVE, y los cuatro KNOB: hueco, cursor, letra y categoría."),
    S("1", "Pulsa SAVE. KNOB 1 elige el hueco: si el sonido era de fábrica, empieza en el primer INIT libre."),
    S("2", "KNOB 2 mueve el cursor y KNOB 3 cambia la letra. KNOB 4 elige la categoría."),
    S("3", "Pulsa SAVE otra vez. Dirá SAVED."),
    DANGER("SOBRESCRIBE",
           "Guardar en un hueco ocupado sustituye lo que había. Un preset de fábrica se recupera desde el editor, en "
           "LIBRARY; uno tuyo, no, salvo que tengas copia."),
    LINK(Presets, "Más sobre presets, macros y guardar"),
    LINK(LabIntro, "Siguiente paso: el laboratorio de sonidos"),
};

}  // namespace

const Chapter kWhat = FMGUIDE_CHAPTER("Qué es SHRUB", kWhatBlocks);
const Chapter kCanCant = FMGUIDE_CHAPTER("Qué puede y qué no", kCanCantBlocks);
const Chapter kFirstTen = FMGUIDE_CHAPTER("Tus primeros 10 minutos", kFirstTenBlocks);

}  // namespace shrub
}  // namespace fmguide

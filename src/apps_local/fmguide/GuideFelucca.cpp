#include "GuideData.h"

// Felucca 1.8.2, by Leo Kuroshita / Hügelton Instruments (hugelton/Felucca),
// from its wiki and README as of 15 November 2026. Its manual calls the SEL
// button SCL; the guide uses the name printed on the unit and says so.

namespace fmguide {

namespace {

constexpr Block kWhat[] = {
    P("Felucca convierte la FM-1 en un sintetizador de cuatro pistas con trece motores de sonido, un secuenciador de "
      "64 pasos por pista y efectos. Es el firmware del que nacen casi todos los demás, SLOOP y X0X incluidos, y el "
      "más completo para diseñar sonido y programar con detalle."),
    FLOW("PISTA 1>PISTA 2>PISTA 3>PISTA 4|8 VOCES>EFECTOS>SALIDA",
         "Cuatro pistas, cada una con su motor y su sonido, que comparten ocho voces. La batería es un motor más: la "
         "pones en la pista que quieras."),
    H("Lo que trae"),
    B("Trece motores: analógico virtual, FM de seis operadores con parches de DX7, distorsión de fase, chip, sampler, "
      "voz, órgano, granular, modelos físicos, ruido, troceador de breaks y batería."),
    B("Secuenciador con acordes, ligaduras, acento, slide, probabilidad, redobles, condiciones y bloqueos de "
      "parámetro por paso."),
    B("Grabación en vivo con sobregrabación y grabación paso a paso."),
    B("Canciones de hasta 16 secciones, 4 proyectos y 32 presets de usuario."),
    B("LFO, matriz de modulación de 4 ranuras por pista, arpegiador y teclas de acorde."),
    B("Audio estéreo y MIDI por USB, entrada MIDI por minijack, y un editor web para todo."),
    N("SIN BLUETOOTH",
      "Felucca no usa el Bluetooth de la FM-1 ni lo hará: depende de bibliotecas cerradas que no "
      "pueden ir en un firmware libre. Para tocarlo sin ordenador: USB MIDI desde un móvil, o la "
      "entrada MIDI de minijack."),
    N("PROBAR ANTES",
      "La página del proyecto tiene Felucca funcionando en el navegador, con el panel en pantalla. No "
      "hace falta la FM-1 para ver si te gusta."),
};

constexpr Block kInstall[] = {
    S("1", "Abre el instalador web (hugelton.github.io/Felucca) en Chrome o Edge."),
    S("2", "Conecta la FM-1 por USB al ordenador. No hace falta nada más."),
    S("3", "Pulsa Install y espera."),
    P("La versión estable es la 1.8.2. La 1.9.0 fue una prueba experimental y se retiró."),
    N("FM-1 RECIENTES",
      "Las unidades nuevas llevan el chip de memoria UC25HQ80. La 1.8.2 lo reconoce. Con la 1.8.1 o "
      "anteriores en una de esas unidades no se guardan los ajustes ni arranca una actualización: "
      "instala siempre la 1.8.2 o posterior."),
    H("Volver al firmware original"),
    P("El instalador ofrece Return to official V15 (o V16) y hace antes una copia de tu trabajo. También sirve el "
      "actualizador de M-VAVE, pero al volver con él se pueden borrar los proyectos de Felucca y el banco de voces "
      "FM6."),
    H("Copias"),
    P("El editor web guarda y restaura todo: proyectos, presets, muestras y ajustes. La única excepción es el banco de "
      "voces FM6: conserva su archivo .syx."),
    H("Entre versiones"),
    B("Los proyectos de versiones anteriores se abren siempre en las nuevas."),
    B("Al revés no: un proyecto guardado con 1.4 o posterior no se abre en 1.1.5; uno guardado con 1.5 no se abre en "
      "1.2 a 1.4.1; uno de 1.6 con pasos condicionales no se abre en 1.5."),
    N("EN UN MAC", "En macOS 13 a 15, pon MENU, SYSTEM, USB SERIAL en OFF para que aparezca la entrada de audio USB."),
};

constexpr Block kPanelCh[] = {
    PANEL(cAlgorithm | cPresets | cSelect | cKnobs,
          "ALGORITHM elige la pista en todas las páginas; PRESETS cambia el sonido; SELECT es el tempo; KNOB 1 a 4 son "
          "los cuatro valores de la página."),
    P("Felucca se organiza en páginas y capas. Tocar un botón abre su página; tocarlo otra vez pasa a la siguiente. "
      "Los botones de página actúan al soltar. HOME vuelve al inicio."),
    N("SEL ES SCL",
      "El manual de Felucca llama SCL (de scale, escala) al botón que en la FM-1 lleva impreso SEL. Es el "
      "segundo de la fila de arriba."),
    H("Las páginas de cada botón"),
    R("HOME", "HOME, MIXER y CLOCK (BPM, swing y fuente de reloj)."),
    R("SEQ", "STEP (el piano roll, o la rejilla de batería), DETAIL y AUTOMATION."),
    R("GLO", "SONG."),
    R("SAVE", "USER, PRESETS, PHRASES (cargar patrones), PROJECT y TOOLS."),
    R("LFO", "LFO, LFO 2, LFO DEST y MOD."),
    R("EDIT", "EDIT 1, EDIT 2, VOICE, VOICE 2 y VOICE 3, más las propias de cada motor."),
    R("FX, SEL, ENV, ARP", "Sus páginas. En FX, INSERT e INSERT 2 van después del SLICER."),
    R("REC", "Arma la pista para grabar."),
    H("Las capas"),
    PANEL(cFx | cGlo | cSel | cEdit | cRec | cSeq | cSave | cHome | cLfo,
          "Mantenidos: FX, GLO, SEL, EDIT, REC y SEQ abren su capa rápida; SAVE deshace; HOME abre el menú; LFO con un "
          "mando da un cambio momentáneo."),
    R("FX mantenido", "Efectos de directo en las teclas: repetir, marcha atrás, barridos, parada de cinta, congelar."),
    R("GLO mantenido", "Silencios, solos, niveles, TAP de tempo, MIDI LEARN (Re4) y FILL (Mi4)."),
    R("SEL mantenido", "Su capa rápida de escala."),
    R("EDIT mantenido", "Los motores; en una pista DRUM, las negras silencian carriles."),
    R("REC mantenido", "CLEAR, CLICK, COUNT-IN y CLICK LEVEL."),
    R("SEQ mantenido", "SEQ TOOLS, en cualquier página."),
    R("SAVE mantenido", "Deshacer."),
    R("HOME mantenido", "El menú."),
    N("CAPA FIJA",
      "Toca dos veces seguidas el botón de una capa y se queda abierta sin sujetarlo. Tócalo otra vez "
      "para cerrarla."),
    H("Confirmar y volver"),
    PANEL(cOct, "En las páginas de acción y en los diálogos, OCT+ lo hace y OCT- vuelve atrás."),
    P("En el menú, OCT+ y OCT- cambian el valor y HOME lo cierra. Lo que se puede pulsar en cada momento respira "
      "suavemente en lugar de parpadear."),
};

constexpr Block kFirst[] = {
    P("Felucca arranca con cuatro pistas listas. Este recorrido toca cada pieza una vez: sonido, pasos, grabación, "
      "mezcla y guardar."),
    S("1",
      "Toca las teclas: suena la pista elegida. Gira ALGORITHM para cambiar de pista; la cabecera muestra su "
      "número."),
    S("2", "Gira PRESETS para cambiar el sonido de esa pista. Funciona desde cualquier página que no sea una lista."),
    S("3", "Para cambiar de motor, mantén EDIT: su capa es la de los motores. Prueba ANALOG y FM6."),
    S("4",
      "Pulsa EDIT y gira KNOB 1 a 4: son los parámetros principales del motor. EDIT otra vez para la página "
      "siguiente."),
    S("5",
      "Pulsa SEQ: se abre STEP, el piano roll de la pista. Con la música parada, pulsa REC y toca: cada tecla (o "
      "acorde) escribe un paso en el cursor y avanza."),
    S("6", "Pulsa PLAY. Para grabar en vivo, pulsa REC y toca encima: cada pasada se suma."),
    S("7",
      "Elige otra pista y ponle el motor DRUM. En SEQ, STEP verás la rejilla de batería: las teclas blancas son "
      "los pasos y las negras eligen el carril."),
    GRID("KICK:x---x---x---x---|SNARE:----x-------x---|HAT:--x---x---x---x-",
         "Un ritmo básico en la rejilla: elige el carril con una tecla negra y marca sus pasos con las blancas."),
    S("8", "Pulsa HOME dos veces para MIXER y ajusta los niveles. Otra vez para CLOCK: BPM y swing."),
    S("9",
      "Para guardar el sonido: para la música, toca SAVE, elige ranura con KNOB 1, y pulsa OCT+ dos veces. El "
      "nombre se escribe con las teclas."),
    S("10", "Para guardar todo el proyecto: SAVE hasta la página PROJECT."),
    N("SE ACUERDA SOLO",
      "Con MENU, RESTORE LAST activado (lo está de fábrica), la FM-1 arranca con la música tal como "
      "la dejaste. Se guarda sola con la música parada y unos segundos sin tocar nada."),
    N("AYUDA EN PANTALLA",
      "MENU, SYSTEM, HELP en ON muestra una pista breve cada vez que abres una página o una capa. "
      "Muy útil los primeros días."),
};

constexpr Block kEngines[] = {
    P("Cada pista usa un motor. Todos tienen sus propios presets de fábrica, clasificados por categoría: BASS, LEAD, "
      "PAD, PLUCK, KEYS, DRUM, FX y OTHER. En el orden en que los lista la FM-1:"),
    R("ANALOG",
      "Analógico virtual: dos osciladores (con sincronía y suboctava), ruido, saturación y filtro resonante "
      "paso bajo, banda o alto."),
    R("FM6", "FM clásica de seis operadores, basada en Dexed: 32 algoritmos y un parche completo por pista."),
    R("PHASE", "Distorsión de fase, con un plegador de onda."),
    R("LOFI", "Chiptune: pulso, triángulo, sierra, ruido y onda de 4 bits, con envolvente escalonada y arpegio."),
    R("SAMPLE", "Instrumentos muestreados (PIANO, FLUTE, SAX) y tres ranuras para tus muestras."),
    R("VOICE", "Oscilador de formantes: vocales cantadas."),
    R("TRIO", "Tres osciladores con modulación en anillo, sincronía y filtro multimodo."),
    R("WHEEL", "Órgano de ruedas tonales: registros, percusión, clic de tecla, saturación y altavoz rotatorio."),
    R("GRAIN", "Texturas granulares a partir de las muestras."),
    R("PHYS", "Modelos físicos: resonadores, cuerdas, membranas y cuerdas simpáticas."),
    R("NOISE", "Ruido, del analógico al digital: colores, crujidos, registros de desplazamiento, tonos metálicos."),
    R("SLICE", "Un break de batería, una nota de piano o tu muestra, cortados en trozos, uno por tecla."),
    R("DRUM", "Un kit de ocho carriles con voces de batería propias, todas sintetizadas, sin muestras."),
    H("FM6 por dentro"),
    P("El parche se edita operador a operador en la propia FM-1: EDIT, OPERATOR (ratio o frecuencia fija, afinación "
      "fina, nivel), OP ENV (cada etapa de la envolvente, dibujada) y OPERATOR 2 (modo, desafinación, velocidad). "
      "Se trabaja un operador cada vez, sobre el dibujo del algoritmo."),
    R("SLOT F1 a F8", "Parches de fábrica."),
    R("SLOT OWN", "El parche propio de la pista."),
    R("SLOT B1 a B32", "Una voz del banco que hayas enviado desde el editor web, que importa archivos .syx de DX7."),
    H("Los kits de DRUM"),
    P("KIT elige entre el kit estándar, cinco kits analógicos virtuales al modo de las cajas de ritmos clásicas, y "
      "tres más montados con sus piezas. Los números son una pista de a qué recuerdan."),
    R("80", "Bombo de seno profundo y largo, caja con ruido, charles de seis cuadradas."),
    R("10", "Bombo barrido, caja de ruido blanco, palmadas de cola larga, un plato."),
    R("66", "Bombo suave y redondo, caja brillante, charles de ruido, una conga."),
    R("55", "Bombo que cae, charles metálicos agudos, una campana."),
    R("77", "Bombo que crece, palmadas en ráfaga, claves, un plato."),
    R("DEEP, TIGHT, PERC", "Grave y largo; corto y seco; conga, claves y campana."),
    P("Los niveles de cada carril están en EDIT, LANES y LANES 2. El motor DRUM sigue el mapa de teclas General MIDI."),
    N("SIN MUESTRA",
      "Si una ranura de muestra está vacía, SAMPLE, GRAIN y SLICE tocan un seno a la altura de la nota "
      "y la pantalla dice NO SAMPLE una vez."),
};

constexpr Block kSeq[] = {
    P("Cada pista tiene 64 pasos. SEQ abre STEP: un piano roll en las pistas melódicas y una rejilla en las de "
      "batería. Cargar un sonido nunca toca tus patrones."),
    H("Escribir pasos"),
    R("Paso a paso",
      "En SEQ, STEP con la música parada, pulsa REC y toca: el acorde que sujetes es el paso, y al "
      "soltar avanza."),
    R("En vivo", "PLAY, REC y toca. Se sobregraba en cada vuelta."),
    R("Rejilla de batería", "Teclas blancas: pasos. Teclas negras: carriles."),
    R("KNOB 3 en el roll", "La duración de la nota."),
    N("ACORDES A UN DEDO",
      "MENU, CONTROL, CHORD ENTRY en ADD: cada tecla añade su nota al paso, hasta cuatro, y KNOB 1 "
      "avanza. Así un acorde entra dedo a dedo."),
    H("Lo que guarda cada paso"),
    B("Acordes, ligaduras, acento y slide."),
    B("CHANCE: la probabilidad de que suene."),
    B("RATCH: el paso tocado 2, 3 o 4 veces dentro de su duración."),
    B("NUDGE: adelantado o atrasado, en dieciseisavos de paso."),
    B("VEL: la velocidad."),
    B("Una condición: que suene en la vuelta 1 de 2, la 4 de 4, o solo con FILL mantenido (GLO + Mi4)."),
    P("SEQ, DETAIL pone en los cuatro KNOB la probabilidad, el redoble, el desplazamiento y la velocidad del paso "
      "bajo el cursor, dibujados como carriles sobre los pasos."),
    H("Bloqueos de parámetro"),
    P("En SEQ, STEP, mantén un paso y gira un KNOB: ese parámetro queda fijado solo para ese paso. EDIT con el paso "
      "mantenido borra sus bloqueos. La grabación de movimientos de mando paso a paso es la automatización; un icono "
      "marca las tarjetas que controla."),
    P("SEQ, AUTOMATION es una lista de todo lo que pasa en los pasos de la pista: probabilidad, redoble, "
      "desplazamiento, velocidad, bloqueos y automatización. Ahí se cambia, se borra o se convierte un bloqueo en "
      "automatización. Caben 128 registros entre las cuatro pistas."),
    H("Tiempo propio"),
    P("La grabación en vivo guarda cuándo tocaste cada nota. QUANTIZE decide qué suena: en ON (lo normal) todo cae en "
      "la rejilla; en OFF suena tu pulso."),
    H("SEQ TOOLS"),
    P("Mantén SEQ en cualquier página. Las herramientas están en las teclas y cada una es un deshacer (SAVE "
      "mantenido). OCT- devuelve la pista a como estaba al abrir la capa."),
    R("CLEAR, REVERSE, SHIFT", "Borrar, invertir y desplazar el patrón."),
    R("RANDOM", "Un ritmo nuevo, o notas nuevas de la escala sobre el mismo ritmo."),
    R("COOK", "Cambia el patrón un poco en cada pulsación."),
    R("EUCLID", "Un ritmo euclidiano: HITS, STEPS y ROT."),
    R("DOUBLE y HALF", "El patrón dos veces, o su primera mitad."),
    R("KNOB 1 a 4", "LEN (longitud), DIV (división), SWING y GATE."),
    H("Claqueta"),
    P("CLICK puede estar apagado, sonar al grabar o sonar siempre, con cuenta previa de 1 o 2 compases. Va solo a "
      "auriculares y altavoz: nunca al audio USB ni a un patrón. Se ajusta manteniendo REC o en MENU, AUDIO."),
};

constexpr Block kPlay[] = {
    H("Escalas"),
    P("En la página SEL (SCL en el manual), QNT decide cómo responden las teclas a la escala elegida con ROOT y "
      "SCALE."),
    R("QNT OFF", "Teclado cromático normal."),
    R("QNT WHITE", "Solo las blancas, recorriendo la escala. La tecla 5 (Do4) toca la tónica. Las negras callan."),
    R("QNT SNAP", "Todas las teclas, cada una redondeada hacia abajo a la escala."),
    R("QNT SEQ",
      "Como SNAP, y además las notas del secuenciador se ajustan a la escala al sonar. Los pasos no se "
      "modifican."),
    R("QNT SEQ+T", "Como SEQ, y la secuencia sigue a TRN: al girarlo se transponen patrón y teclas a la vez."),
    P("Hay 16 escalas: cromática, mayor, menor natural, dórica, mixolidia, pentatónicas mayor y menor, menor "
      "armónica, frigia, lidia, locria, menor melódica, blues, tonos enteros y dos disminuidas. TRN transpone; los "
      "botones de octava mueven octavas enteras."),
    H("Teclas de acorde"),
    P("Toca SEL otra vez para la página CHORD. CHRD elige qué tocan las teclas: nada, las tríadas o las séptimas de "
      "la escala, o una forma de acorde fija. VOIC elige la disposición. Funcionan al tocar, por MIDI, al grabar y "
      "con el arpegiador."),
    H("Arpegiador"),
    R("Órdenes básicos", "UP, DN, UPDN, RND, ORD (en el orden en que pulsaste) y REPEAT."),
    R("Para acordes", "DNUP, UP+8, CONV, DIVG, PINKY, THUMB, WALK y CHORD."),
    P("Un LED marca el pulso. La pista puede ser MONO, LEGATO o UNISON, con glide."),
    H("El mando SELECT"),
    P("De fábrica, SELECT es el tempo. En MENU, CONTROL, SELECT puedes hacer que pase páginas (PAGE), que sea la "
      "rueda de modulación (MODW) o que fije la velocidad con que tocan las teclas (VEL). Con cualquiera de esos, el "
      "tempo está en GLO + SELECT, en HOME, CLOCK o en el TAP de GLO."),
    H("Presión"),
    P("EDIT, VOICE 3 tiene AFTERTOUCH, apagado de fábrica: lleva la presión de las teclas (solo de Fa3 a Do5) o la "
      "presión de canal MIDI a un destino."),
};

constexpr Block kMod[] = {
    H("LFO"),
    P("Cuatro páginas en el botón LFO. La velocidad puede ser libre (RATE) o sincronizada a una figura, de 4 "
      "compases a 1/32 (SYNC). TRIG decide si reinicia en cada nota o va libre, y POL si es bipolar o unipolar."),
    H("Matriz de modulación"),
    P("LFO, MOD: cuatro ranuras por pista. Las fuentes incluyen controladores MIDI, S&H y SLEW (un valor aleatorio "
      "por ciclo, retenido o deslizado). DEPTH es un destino, así que la matriz puede modular al propio LFO. VEL y "
      "TIME sirven para humanizar: la velocidad de cada nota y un paso tocado un poco tarde."),
    R("ENV SYNC", "En ENV, ENV DEST: ataque, caída y relajación como figuras del tempo."),
    R("SPREAD", "En EDIT, VOICE 3: las voces de una pista POLY o UNISON alternan a izquierda y derecha de su PAN."),
    H("Efectos"),
    FLOW("POR PISTA>DISTORSION>INSERT>SLICER|ENVIOS>CHORUS>DELAY>REVERB|MEZCLA>LIMITADOR",
         "Qué efectos hay y dónde: tres por pista, tres envíos compartidos y un limitador maestro. El dibujo agrupa; "
         "no indica el orden exacto de la señal."),
    R("INSERT", "Saturaciones, CRUSH, phaser, flanger o chorus, con mezcla seco/mojado."),
    R("SLICER", "Una puerta o tartamudeo de 16 pasos sincronizado, con 16 patrones."),
    R("DELAY", "Sincronizado al tempo, con valores con puntillo."),
    R("REVERB", "TYPE: HALL (estéreo y suave, la de fábrica), ROOM o SPRING."),
    H("La capa FX"),
    P("Mantén FX: repetición, marcha atrás, barridos de filtro, parada de cinta, congelado, armonizador (octava "
      "arriba o abajo, con brillo), FLANGER y PHASER a tempo, y REV THROW y ECHO THROW, que abren a tope el envío de "
      "todas las pistas mientras los sujetas. Las teclas negras silencian pistas."),
    N("ELEGIR EL EFECTO",
      "Cualquier tecla blanca puede llevar cualquier efecto: en la capa FX, mantén la tecla y gira "
      "PRESETS. Con MENU, FX LATCH en ON, los efectos se quedan puestos hasta que los vuelves a "
      "pulsar."),
    H("Momentáneo"),
    PANEL(cLfo | cKnobs, "Mantén LFO y gira un KNOB: al soltar LFO, el valor vuelve de golpe a donde estaba."),
    P("Funciona en HOME y en las páginas de sonido y de efectos. Si pulsas OCT+ mientras lo sujetas, el cambio se "
      "queda."),
};

constexpr Block kSave[] = {
    H("Tres cosas que se guardan"),
    R("Presets", "Sonidos. Los de fábrica más 32 ranuras de usuario, con nombre y categoría."),
    R("Proyectos", "Cuatro, A a D. Cada uno son las cuatro pistas con sus sonidos, patrones y automatización."),
    R("Canción", "El orden en que suenan los proyectos."),
    P("La cabecera muestra la pista y la ranura de proyecto de su patrón: «1 A», y «1 A*» cuando lo has modificado."),
    H("Guardar un sonido"),
    S("1", "Para la música."),
    S("2", "Toca SAVE y elige ranura con KNOB 1."),
    S("3", "Pulsa OCT+, y OCT+ otra vez. El nombre se escribe con las teclas."),
    R("INIT", "Carga el primer preset del motor."),
    R("CLEAN INIT", "Su sonido básico, sin nada añadido."),
    H("Proyectos"),
    P("SAVE, PROJECT guarda, carga y borra (ERASE) proyectos. Al cargar con cambios sin guardar, pregunta. Los "
      "proyectos de cualquier versión anterior se abren."),
    R("CLEAR ALL",
      "MENU, SYSTEM: empieza un proyecto nuevo. Cada pista vuelve a su sonido de arranque, sin pasos. "
      "Lo guardado, los presets y los ajustes no se tocan. No se puede deshacer."),
    R("PHRASES", "En SAVE: carga solo patrones."),
    H("Canciones"),
    P("GLO abre SONG: hasta 16 secciones. Cada sección dice qué ranura de proyecto (A a D) toca cada pista, o un "
      "guion para la pista que descansa ahí, y cuántas veces se repite. Se dibuja como una línea de tiempo con un "
      "carril por pista."),
    R("KNOB 1 a 4 en SONG", "SECTION, TRACK, PAT y REPS."),
    R("Teclas 3, 4, 5 y 6", "La3, Si3, Do4 y Re4 fijan la ranura A, B, C o D."),
    H("Deshacer"),
    PANEL(cSave,
          "SAVE mantenido deshace: un paso de deshacer que cubre sonido, pasos, automatización y las "
          "herramientas de SEQ TOOLS."),
    P("El mensaje dice qué ha vuelto: UNDO/REDO T1 STEPS, SOUND o AUTO."),
};

constexpr Block kMenu[] = {
    P("Mantén HOME para el menú, en cinco pestañas. ALGORITHM cambia de pestaña, PRESETS recorre las filas, y "
      "cualquier KNOB, u OCT+ y OCT-, cambia el valor. HOME lo cierra."),
    R("DISPLAY",
      "COLOR (10 paletas, y una propia desde el editor), STYLE, LARGE (letra grande), ANIM, LEDS, SCREEN "
      "OFF, SCOPE y HOME (osciloscopio, pistas o niveles)."),
    R("CONTROL", "HOLD, KNOB ACCEL, FX LATCH, BPM LOCK, SCALE LEDS, STEP PREVIEW, CHORD ENTRY y SELECT."),
    R("AUDIO", "SPEAKER EQ, USB LEVEL, CLICK, CLICK LEVEL, COUNT-IN y TUNE."),
    R("MIDI", "MIDI IN, T1 a T4 CHANNEL, AUTO SELECT, SCREEN WAKE y LEARN CLEAR."),
    R("SYSTEM", "USB SERIAL, RESTORE LAST, HELP, 2ND CORE, CLEAR ALL, CALIBRATION, INFO y ABOUT."),
    N("PARA GRABAR",
      "SPEAKER EQ también afecta a los auriculares y al audio USB: déjalo en FLAT al grabar. USB LEVEL "
      "en FIXED manda siempre el audio USB a nivel completo, sin depender de MASTER."),
    H("MIDI"),
    P("Entra por USB y por el minijack TRS. MIDI IN decide a qué pista va cada canal:"),
    R("CH1-4", "Los canales 1 a 4 tocan las pistas 1 a 4. Es lo de fábrica."),
    R("SEL", "Cualquier canal toca la pista elegida."),
    R("CH5-8, CH9-12, CH13-16", "Otro bloque de cuatro, por ejemplo para dos FM-1 en un mismo cable."),
    R("TRACK", "Cada pista con su canal, de 1 a 16, OMNI u OFF."),
    P("Controladores fijos: 5 glide, 7 nivel, 10 panorama, 71 resonancia, 72, 73 y 75 relajación, ataque y caída, 74 "
      "brillo, 91, 93 y 94 envíos de reverb, chorus y delay. Program Change carga el sonido de una pista."),
    R("MIDI LEARN", "GLO + Re4: asigna hasta 16 controladores más a cualquier parámetro de cualquier pista."),
    H("Audio por USB"),
    P("La FM-1 aparece como una entrada de audio estéreo llamada Felucca, a 44.1 o 48 kHz, sin controlador. Graba la "
      "salida maestra."),
    H("Si algo falla"),
    R("No arranca tras actualizar",
      "Prueba otro cable de datos, cierra todo lo que use MIDI y lanza de nuevo el "
      "instalador."),
    R("Sale como WL80UBOOT",
      "Es el modo de arranque del chip. Se puede recuperar; mira el capítulo de rescates de "
      "«Antes de nada»."),
    R("Un botón hace otra cosa", "MENU, SYSTEM, CALIBRATION."),
    R("No aparece el audio en Mac", "MENU, SYSTEM, USB SERIAL en OFF."),
    H("Chuleta"),
    R("Pista", "ALGORITHM."),
    R("Sonido", "PRESETS."),
    R("Motor", "EDIT mantenido."),
    R("Tempo", "SELECT, o HOME, CLOCK."),
    R("Pasos", "SEQ."),
    R("Herramientas", "SEQ mantenido."),
    R("Mezcla rápida", "GLO mantenido."),
    R("Deshacer", "SAVE mantenido."),
    R("Sí / no", "OCT+ / OCT-."),
    R("Menú", "HOME mantenido."),
};

constexpr Chapter kChapters[] = {
    FMGUIDE_CHAPTER("Qué es Felucca", kWhat),
    FMGUIDE_CHAPTER("Instalar y volver", kInstall),
    FMGUIDE_CHAPTER("El panel: páginas y capas", kPanelCh),
    FMGUIDE_CHAPTER("Primeros diez minutos", kFirst),
    FMGUIDE_CHAPTER("Los trece motores", kEngines),
    FMGUIDE_CHAPTER("El secuenciador", kSeq),
    FMGUIDE_CHAPTER("Escalas, acordes y arpegios", kPlay),
    FMGUIDE_CHAPTER("Modulación y efectos", kMod),
    FMGUIDE_CHAPTER("Presets, proyectos y canción", kSave),
    FMGUIDE_CHAPTER("Menú, MIDI y chuleta", kMenu),
};

}  // namespace

const Guide kGuideFelucca = {
    "FELUCCA", "Cuatro pistas, trece motores y un secuenciador a fondo.",  "V 1.8.2",
    kChapters, static_cast<int>(sizeof(kChapters) / sizeof(kChapters[0])),
};

}  // namespace fmguide

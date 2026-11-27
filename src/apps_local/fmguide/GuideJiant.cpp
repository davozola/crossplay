#include "GuideData.h"

// JIANT FM V10.7, by Juan Jiant (juanjiant-bit/JIANT), from its docs/MANUAL.es.md
// (written against V10.7.16) and its project page as they stood on 18 November
// 2026. Like Felucca's, its manual calls the SEL button SCL; the guide uses the
// name printed on the unit and says so. Keys are numbered as in every guide:
// whites 1-16 from F3, blacks 1-11 from F#3.
//
// The firmware publishes a build most days and its page names move between
// them. Where the manual disagrees with itself about a page name, the guide
// names the group (the tab the screen shows) rather than the page.

namespace fmguide {

namespace {

constexpr Block kWhat[] = {
    P("JIANT FM convierte la FM-1 en un instrumento completo de seis pistas: doce motores de síntesis (ninguno usa "
      "muestras), un secuenciador de 64 pasos por pista, efectos, canciones y un generador que escribe ideas de tema "
      "enteras. Nace de Felucca y lo lleva hacia componer y tocar en directo sin ordenador. Lo hace Juan Jiant, en "
      "Buenos Aires; es gratis y de código abierto (GPL-3.0)."),
    FLOW("T1 DRUM-X>T2 ANALOG>T3 PHASE|T4 FM6>T5 VOICE>T6 ANIMAL|VOCES>EFECTOS>MASTER",
         "Las seis pistas tal como arrancan. Cualquier motor puede ir en cualquier pista. De fábrica suenan cuatro: T5 "
         "y T6 duermen hasta que pones TRACKS en 6."),
    H("Lo que trae"),
    B("Doce motores: ANALOG, FM6 (seis operadores, 32 algoritmos, bancos de DX7), PHASE, MATH, VOICE (canta palabras "
      "en castellano, inglés y japonés), WHEEL, NOISE, ANIMAL (pájaros, insectos y ranas por modelos físicos), STRING, "
      "ADD, EXT y la batería DRUM-X."),
    B("DRUM-X: ocho sonidos sintetizados, once kits y un mando MORPH que mueve el kit entero entre dos caras."),
    B("Secuenciador con piano roll por pista, acordes, hasta dos componentes con condición por paso, cabezales extra, "
      "microtiempo de 1/64 de paso y bloqueos de parámetro."),
    B("EUCLID escribe ritmos y melodías euclidianos con cuatro mandos, y MUTATE los va cambiando solo."),
    B("GENE, un generador que arma un tema entero (motores, sonidos, partes, escala, tempo y efectos) por género, o "
      "rompiendo todas las reglas."),
    B("Delay, cinco reverbs, chorus, slicer, efectos de directo, filtro DJ sobre la mezcla y master."),
    B("Seis proyectos de cuatro secciones cada uno, con un arreglo de hasta 16 filas."),
    B("Teclas sensibles a la fuerza y a la presión, sin modificar la FM-1."),
    B("Audio por USB (también cada pista por su canal), MIDI por USB, minijack y Bluetooth, y editor, instalador y "
      "emulador en el navegador."),
    N("PROBAR ANTES",
      "El emulador (juanjiant-bit.github.io/JIANT/emu) es el firmware real, con sonido, en el navegador. Sirve para "
      "aprender las capas sin la FM-1: las filas del teclado del ordenador son las teclas y Mayús + clic mantiene un "
      "botón."),
    N("NO ES FELUCCA",
      "Aunque nace de Felucca, se maneja de otra manera: aquí SELECT recorre páginas y el tempo es GLO + SELECT. No "
      "tiene sampler ni muestras de usuario, y sí seis pistas, Bluetooth y teclas con fuerza."),
    N("VERSIONES",
      "Sale una versión casi cada día. Esta guía sigue el manual de la V10.7, de noviembre de 2026. Si una página no "
      "está donde dice la guía, gira SELECT por la sección: el pie de la pantalla dice siempre dónde estás."),
    H("Para quién es"),
    P("Para quien quiere que la FM-1 le proponga música y luego meterle mano: un tema por género en dos gestos, una "
      "batería que cambia sola, una voz que canta tu letra. Es el más grande de los firmwares de estas guías y el que "
      "más tarda en aprenderse, pero todas sus secciones se manejan con las mismas seis reglas."),
};

constexpr Block kInstall[] = {
    S("1",
      "Abre el instalador (juanjiant-bit.github.io/JIANT/webapp/installer) en Chrome o Edge, en un ordenador. También "
      "vale Chrome en Android con un cable OTG; en iPhone y iPad no se puede."),
    S("2",
      "Conecta la FM-1 encendida, directamente y con un cable USB de datos. Cierra el DAW, la app de M-VAVE y "
      "cualquier otra pestaña que use MIDI."),
    S("3", "Si la FM-1 lleva otro firmware de la comunidad, vuelve antes al original con el botón Volver al oficial."),
    S("4",
      "Pulsa Instalar y no desenchufes. El instalador mira primero qué lleva la FM-1 y solo escribe sobre el V16 o el "
      "V15 original, o sobre JIANT."),
    N("NUNCA ENCIMA DE OTRO",
      "JIANT no se instala directamente sobre Felucca, SLOOP, X0X, FM-1+VA ni ningún otro firmware de la comunidad: "
      "hacerlo ya ha dejado alguna FM-1 sin arrancar. Pasa siempre por el original."),
    N("FM-1 CON V14 O V13", "Si el original es anterior al V15, usa también Volver al oficial para ponerle el V16."),
    H("Volver al firmware original"),
    P("El botón Volver al oficial del instalador pide el archivo FM-1.fwsc de M-VAVE: descarga el firmware V16 de "
      "m-vave.com/download, descomprime el ZIP y elige ese archivo (el V15 también vale). Lo comprueba byte a byte y, "
      "antes de escribir, guarda una copia completa de lo que tengas."),
    P("El firmware original no abre la música de JIANT. Para recuperarla más adelante, reinstala JIANT y restaura la "
      "copia con el editor."),
    H("Copias"),
    P("El editor web (juanjiant-bit.github.io/JIANT/webapp/editor), en Projects, tiene Save backup: un solo archivo "
      "con la música actual, los 6 proyectos con sus 4 secciones, los arreglos y escenas, los 32 presets de usuario y "
      "la configuración. Restore backup lo devuelve todo."),
    N("EL BANCO FM6",
      "El banco de voces FM6 (B1 a B32) no va en la copia. Conserva tus archivos .syx para volver a enviarlos."),
    N("DESDE FELUCCA",
      "JIANT no tiene muestras de usuario: las USR1 a USR3 de Felucca se pierden al instalar. Guarda antes tus WAV."),
    H("Entre versiones"),
    B("Haz copia antes de actualizar. Un proyecto guardado con una versión nueva puede no abrirse en una anterior."),
    B("Desde la V10.6p hay 6 proyectos, no 8: los 7 y 8 de versiones anteriores hay que pasarlos antes a 1-6."),
    B("Los proyectos muy antiguos (de JIANT anterior a 1.0.3, o de Felucca 1.0 y 1.1) ya no se leen."),
    N("FM-1 RECIENTES",
      "Las unidades vendidas desde finales de 2026, con el logo en el frente, llevan otro chip de memoria. JIANT "
      "anterior a la V10.6z no lo conoce y no guarda nada al apagar: instala siempre la versión actual."),
    H("Si algo falla"),
    R("Recuadro «Qué hacer»", "El instalador explica cada error. COPIAR INFORME copia los datos para pedir ayuda."),
    R("No encuentra la FM-1",
      "Encendida, cable de datos, sin concentrador, y ningún otro programa usando su puerto MIDI."),
    R("Se cortó a medias", "No apagues la FM-1. Reconecta el cable y pulsa el mismo botón: sigue donde estaba."),
    R("Pantalla en negro",
      "Conecta e instala de nuevo. Si el ordenador la ve como WL80UBOOT, prueba otro cable; si no vuelve, se recupera "
      "con FM-1 Transporter."),
    R("BLUETOOTH MISSING",
      "Falta la parte Bluetooth, que también lleva GENE y el guardado de canciones. Otras herramientas solo escriben "
      "el firmware principal: reinstala con el instalador web."),
    H("Combinaciones de rescate"),
    PANEL(cOct | cRec, "Los dos botones de octava y REC, al encender o con la FM-1 en marcha."),
    R("OCT- y OCT+ juntos, 5 s", "Con la FM-1 en marcha: modo actualización, con cuenta atrás. Suelta para cancelar."),
    R("OCT- y OCT+ al encender",
      "HARDWARE CALIBRATION: enseña a la FM-1 cada botón y cada mando. Úsala si una tecla o un botón hace otra cosa."),
    R("REC al encender", "Arranca con un solo núcleo durante esa sesión, por si se cuelga con dos."),
    N("JIANT CRASH",
      "Una pantalla roja con números es un cuelgue: hazle una foto y la FM-1 se reinicia sola a los 4 segundos. El "
      "último queda en MENU, SYSTEM, ABOUT, LAST CRASH. Tras un cuelgue con dos núcleos, pasa sola a uno."),
};

constexpr Block kMove[] = {
    PANEL(cSelect | cAlgorithm | cPresets | cKnobs,
          "SELECT recorre las páginas, ALGORITHM elige la pista, PRESETS carga sonidos y KNOB 1 a 4 son las cuatro "
          "tarjetas de la pantalla."),
    P("JIANT tiene diez secciones, una por botón, y todas se manejan igual: lo aprendes una vez y ya las conoces "
      "todas. Son seis reglas."),
    N("SEL ES SCL",
      "El manual de JIANT, como el de Felucca, llama SCL (de scale, escala) al botón que en la FM-1 lleva impreso SEL. "
      "Es el segundo de la fila de arriba."),
    H("Las seis reglas"),
    S("1",
      "SELECT recorre las páginas. Cada sección es una sola cadena de páginas: una por paso de SELECT, y frena en la "
      "primera y en la última."),
    S("2",
      "El botón de la sección entra o salta. Desde otra sección, te devuelve a la página donde la dejaste. Si ya estás "
      "dentro, salta a la primera página del grupo siguiente, en ronda."),
    S("3",
      "PRESETS carga presets. Siempre: los sonidos de la pista en todas partes, los patrones en SEQ y el género en "
      "GENE. Nunca cambia de página ni de pista."),
    S("4",
      "KNOB 1 elige. Si una página elige algo (un paso, una ranura, una etapa, un operador, un sonido del kit, una "
      "fila), lo elige KNOB 1. La pista, siempre ALGORITHM."),
    S("5",
      "Mantener es la capa. Un botón mantenido pone sus atajos en las teclas y los mandos, y la pantalla dibuja el "
      "teclado con el nombre de cada tecla. Suelta y vuelves."),
    S("6",
      "La pantalla dice dónde estás. Arriba, las pestañas son los grupos de la sección, con un punto por página. "
      "Abajo: SECCIÓN, GRUPO y página, como FX · DELAY · DLY 2, y su número en la cadena."),
    H("Las secciones y sus grupos"),
    R("HOME", "ECO (las criaturas y MACROS), BEING (la pista en grande y ROUTES) y GENE, el generador."),
    R("EDIT", "SOUND, TONE, FOCUS y VOICE. En FM6, OPERATOR en lugar de FOCUS. En DRUM: KIT, SOUND, LEVELS y TONE."),
    R("SEQ", "STEP, LENGTH, EUCLID, PLAYHEADS, STRETCH, COMP, AUTO y GENERATE."),
    R("FX", "TRACK, BUSES, DELAY, REVERB, CHORUS, SLICER, PUNCH y MASTER."),
    R("GLO", "LIVE, MIX, COMPS, GLOBAL y SYSTEM."),
    R("ENV", "TRACKS (las seis envolventes a la vez), ENV y STAGE."),
    R("LFO", "MOD (la matriz), LFO y MSEQ."),
    R("SEL", "KEY (la escala), CHORD y SCENE."),
    R("ARP", "ARP y ARP 2, un solo grupo."),
    R("SAVE", "SONG (el tema), PROJECT y SOUND."),
    R("PLAY y REC", "PLAY arranca y para. REC arma la grabación en la pista elegida."),
    N("ALGUNAS SE ESCONDEN",
      "Hay páginas que solo aparecen donde hacen falta: las de operadores en una pista FM6, las de kit en una DRUM, "
      "WORDS en una VOICE. La cadena salta las demás."),
    H("Las capas"),
    PANEL(
        cFx | cGlo | cSel | cEdit | cRec | cSeq | cSave | cLfo | cHome,
        "Mantenidos, estos botones abren su capa. ENV y ARP no tienen. El tiempo de espera es MENU, CONTROL, HOLD: 0,4 "
        "segundos de fábrica; una tecla o un mando la abre al instante."),
    R("FX mantenido", "Efectos de directo en las teclas, y FILTER, CRUSH, THROW y DEPTH en los mandos."),
    R("GLO mantenido", "La mezcla: silencios, solos, niveles, TAP de tempo y UNDO."),
    R("SEL mantenido", "Cualquier tecla fija la tónica. Mandos: ROOT, SCALE, ROTATE y PITCH."),
    R("EDIT mantenido", "INIT y RECALL del sonido, y en una pista DRUM los silencios de cada sonido."),
    R("EDIT mantenido + un mando",
      "MOMENTARY: el mando cambia su valor en la página que estabas viendo y, al soltar EDIT, vuelve solo a donde "
      "estaba. No queda guardado ni grabado."),
    R("SEQ mantenido", "SEQ TOOLS: borrar, invertir y desplazar la secuencia."),
    R("REC mantenido", "CLEAR y CLICK. Mantenido solo y soltado sin tocar nada más: UNDO."),
    R("SAVE mantenido", "Las secciones en vivo: tocarlas, guardarlas, SONG REC."),
    R("LFO mantenido", "Las macros: KNOB 1 a 4 son M1 a M4. Al soltar quedan enganchadas hasta que pulsas LFO."),
    R("HOME mantenido", "El MENU, a los 0,7 segundos."),
    N("TRES VISTAS",
      "Con el botón de una capa mantenido, gira ALGORITHM: MAP (el teclado con sus atajos), KNOBS (los cuatro mandos "
      "de la capa) y STATE (lo que está activo ahora)."),
    H("Un botón mantenido y SELECT"),
    R("HOME + SELECT", "GENE, el generador de temas, desde cualquier página."),
    R("EDIT, SEQ, LFO o FX + SELECT", "Los dados de esa parte: sonido, secuencia, LFO o efectos."),
    R("GLO + SELECT", "El tempo, desde cualquier página."),
    R("ENV, SEL, ARP o SAVE + SELECT", "Entra en esa sección y recorre su cadena."),
    H("Confirmar y volver"),
    PANEL(cOct, "En las páginas de acción y en los diálogos, OCT+ lo hace (YES) y OCT- vuelve atrás (NO)."),
    P("Una tarjeta de acción (SAVE, LOAD, ERASE, CLEAR) se arma girando su mando a la derecha: se tiñe de violeta y "
      "dice OCT+. Pulsa OCT+ para ejecutarla. Todo lo destructivo pregunta antes."),
    P("Fuera de eso, OCT- y OCT+ son la octava del teclado, de -3 a +3. Los dos a la vez la devuelven a 0."),
    H("La pantalla"),
    R("Arriba",
      "Parado o sonando, el BPM, la pista con su motor y su sonido, la octava, el medidor de carga (cinco barritas) y "
      "la batería."),
    R("Las tarjetas",
      "Cuatro, una por KNOB. La que giras se enciende en naranja. Un nombre en violeta: ese valor está automatizado o "
      "tiene un bloqueo."),
    R("Los colores",
      "Como una cámara térmica: frío (índigo, violeta) es poco; caliente (rojo, naranja, crema) es mucho. Violeta es "
      "lo elegido y naranja lo activo."),
    R("Pieles", "MENU, DISPLAY, SKIN: AIR, COLORS, BOTTOM y HUD. Cambian el vestido, no lo que hace cada página."),
    H("HOME, el ecosistema"),
    P("HOME dibuja una criatura por pista: su especie depende del motor, su forma de sus cuatro mandos, y se hincha y "
      "se calienta con su propio sonido. Cierra los ojos con cada golpe y abre la boca con el nivel. La pista elegida "
      "es la más grande; una silenciada duerme. En HOME, KNOB 1 a 4 son los cuatro controles más útiles del motor."),
    H("El MENU"),
    P("HOME mantenido lo abre. SELECT recorre todas las filas, ALGORITHM salta de pestaña (DISPLAY, CONTROL, AUDIO, "
      "SYSTEM), cualquier mando u OCT+ y OCT- cambian el valor, y HOME lo cierra y guarda la configuración."),
    H("Combinaciones útiles"),
    R("PLAY mantenido", "PANIC: para el transporte y corta todas las notas, también las colgadas por MIDI."),
    R("GLO + PLAY", "Reinicia desde el principio sin parar."),
    R("GLO + tecla 9 (Sol4)", "UNDO del último cambio grande. Otra vez: rehacer."),
    R("EDIT + PRESETS", "Salta al primer sonido del motor siguiente o anterior."),
    R("SEQ + tecla 16 (Sol5)", "CLR STEP: borra el paso del cursor y sus bloqueos."),
    R("SEQ + REC", "CLEAR ALL SEQUENCES: los pasos de las seis pistas. Pregunta antes."),
    R("SAVE + tecla 12 (Do5)", "NAME: renombra la ranura, la sección o el proyecto."),
    R("REC + botón de sección", "Borra la automatización de esa parte en la pista elegida, sin preguntar."),
};

constexpr Block kFirst[] = {
    P("Al encender, la firma se dibuja sola y llegas a HOME con una banda ya afinada: batería, un punteado, una "
      "sierra, un bajo FM, un coro y un silbido. Todo seco y sin pasos."),
    H("Tu primer sonido"),
    S("1", "Gira ALGORITHM para elegir pista. Con TRACKS en 4 se detiene en T4."),
    S("2", "Gira PRESETS para recorrer sonidos y toca las teclas. La lista es una sola, con todos los motores."),
    S("3", "Gira KNOB 1 a 4: en HOME son los cuatro controles más útiles del motor de esa pista."),
    S("4",
      "Si no te convence, mantén EDIT y gira SELECT: el sonido se tira a los dados y oyes la variación al instante. "
      "OCT+ se la queda, OCT- devuelve el sonido de antes."),
    H("Tu primer tema"),
    S("1",
      "Mantén HOME y gira SELECT: se abre GENE en EXPLORE, sin género y con las reglas rotas. Si prefieres uno, "
      "elígelo con PRESETS o KNOB 1."),
    S("2", "Pulsa OCT+: aparece una idea de tema completa. Pulsa PLAY."),
    S("3",
      "Pulsa OCT+ otra vez para una pieza nueva. Si está demasiado cargada, gira SELECT a la página siguiente, baja "
      "DRUMS o ARP y pulsa OCT+: la misma pieza con solo eso cambiado."),
    S("4", "Para volver a lo que había antes de generar: mantén GLO y pulsa la tecla 9 (Sol4). Es UNDO."),
    S("5",
      "Si te gusta, para con PLAY, pulsa SAVE y gira SELECT hasta SECTIONS. Gira KNOB 4 a la derecha hasta que diga "
      "OCT+ y pulsa OCT+. Escribe un nombre o deja el que ofrece y pulsa OCT+ otra vez: SAVED."),
    S("6",
      "Haz una variación (cambia pasos, silencia una pista) y guárdala en la sección B: en SECTIONS, KNOB 1 hasta B, y "
      "KNOB 4 y OCT+ como antes."),
    S("7",
      "Gira SELECT hasta ARRANGE. OCT+ añade una fila; KNOB 2 elige su sección y KNOB 3 sus compases. Pon una fila "
      "para A y otra para B y pulsa PLAY: el tema toca las filas en orden."),
    FLOW("HACER MUSICA>SECCION>ARRANGE",
         "Todo el ciclo de JIANT: hacer música, guardarla en una sección y encadenar secciones en el arreglo."),
    N("SE PIERDE AL APAGAR",
      "De fábrica la FM-1 enciende en cero: lo que suena vive en la memoria volátil hasta que lo guardas en una "
      "sección. El capítulo 11 explica el guardado; léelo antes de hacer algo que quieras conservar."),
    N("T5 Y T6 DORMIDAS",
      "Si una pista no suena y la pantalla dice T5-T6 ASLEEP, es que TRACKS está en 4. Ponlo en 6 en GLO, SYSTEM, KNOB "
      "1, y vigila el medidor de carga."),
    H("Teclas con fuerza y presión"),
    KEYS(white(1) | white(2) | white(3) | white(4) | white(5) | white(6) | white(7) | white(8) | white(9) | white(10) |
             white(11) | white(12) | black(1) | black(2) | black(3) | black(4) | black(5) | black(6) | black(7) |
             black(8),
         "De Fa3 a Do5 las teclas miden la fuerza y la presión. Las siete de arriba, de Do#5 a Sol5, no tienen sensor "
         "y tocan con la media de tus golpes recientes."),
    P("JIANT lee la pastilla de carbón que hay bajo cada tecla, sin tocar el aparato. La fuerza llega al sonido, a "
      "los acordes, al arpegiador, a la grabación y a la salida MIDI. Cada tecla aprende sola tu forma de tocar."),
    R("VELOCITY", "MENU, CONTROL: OFF, SOFT, NORMAL o HARD. En OFF todas las teclas suenan a 100."),
    R("VEL MIN", "La fuerza del toque más suave. De fábrica, 20."),
    R("AFTERTOUCH",
      "Aprieta más una tecla ya pulsada: es la fuente AT de la matriz. En los sonidos nuevos abre el filtro. OFF, LOW, "
      "MID o HIGH."),
    R("VEL del sonido",
      "EDIT, grupo VOICE, KNOB 3 de VOICE 2: cuánta fuerza le llega a ese sonido. Los bajos de fábrica vienen en 0, "
      "para sonar parejos."),
    R("VEL LEARN RESET", "Vuelve a la calibración de fábrica de las teclas."),
    H("Teclas de expresión"),
    P("Con una nota mantenida, OCT- hace entrar un vibrato, como una rueda de modulación, y OCT+ dobla la nota hacia "
      "arriba hasta el intervalo de MENU, AUDIO, OCT BEND (2 semitonos de fábrica). Al soltar vuelven solas. Sin "
      "ninguna nota mantenida, OCT- y OCT+ cambian la octava como siempre."),
};

constexpr Block kEngines[] = {
    P("Doce motores, todos de síntesis. PRESETS los recorre en una sola lista, motor tras motor, y EDIT + PRESETS "
      "salta de motor en motor. EDIT 1 y EDIT 2 tienen los ocho mandos propios de cada uno; HOME, los cuatro mejores."),
    R("ANALOG",
      "Dos osciladores y filtro por voz. Ondas SAW, SQR, TRI, SIN, PWM, SYNC, RING, SAW3 y PLS3. INT es el intervalo "
      "del segundo oscilador. En HOME: CUT, RES, ATK y REL."),
    R("FM6",
      "FM de seis operadores (el motor de Dexed), 32 algoritmos dibujados y cuatro voces por pista. En HOME: MOD, "
      "RATIO, MEG y SLOT."),
    R("PHASE", "Distorsión de fase, con dos ondas, DCW y realimentación (FB). En HOME: DCW, ENV, DETUNE y REL."),
    R("MATH",
      "Fórmulas que suenan: BYTE (bytebeat) o FLOAT (hecho de senos). ALGO elige entre 32 y VAR la muta; MOTION, BEND "
      "y LOOP la deforman."),
    R("VOICE", "Una voz por formantes: vocales, habla y aliento. Y canta letras: más abajo."),
    R("WHEEL",
      "Órgano de tiradores: registros (REG), percusión, clic de tecla, DRIVE y ROTOR. EDIT, ROTOR tiene DEPTH y DUST, "
      "el crujido de un órgano viejo."),
    R("NOISE", "Ruido coloreado y metálico: ANLG, DUST, LFSR y META. PEAK es su resonancia."),
    R("ANIMAL", "Cantos de animales por modelos físicos. Más abajo."),
    R("STRING", "Una cuerda pulsada, golpeada o frotada (EXCITE), y el feedback de una guitarra."),
    R("ADD", "Aditiva: diez espectros en MODE, y MORPH, BRIGHT y PARTIAL."),
    R("EXT", "Una señal de fuera: la entrada de audio USB, o SWARM, que hace música con el Bluetooth de alrededor."),
    R("DRUM", "DRUM-X, la batería. Tiene su capítulo."),
    N("SIN CORTES",
      "Cambiar una pista de motor no corta nada: el anterior se apaga en unos milisegundos y las notas mantenidas "
      "siguen en el nuevo."),
    H("FM6 por dentro"),
    P("Los mandos de EDIT 1 y EDIT 2 son macros sobre el parche. SLOT elige el parche: F1 a F8 son de fábrica, OWN es "
      "uno editado o importado y B1 a B32 es un banco de DX7 enviado desde el editor web."),
    R("OPERATOR", "KNOB 1 elige el operador; después RATIO o FIXED, FINE y LEVEL."),
    R("OP ENV", "La envolvente del operador: etapa, RATE y LEVEL."),
    R("OPERATOR 2", "MODE, DTUN y VEL de cada operador."),
    P("Editar un operador convierte el parche en propio de la pista (SLOT OWN) y se guarda con el proyecto. Una voz "
      "del banco se copia a la pista, así que el proyecto la conserva aunque luego envíes otro banco."),
    H("VOICE canta"),
    P("Se activa en EDIT, WORDS, con LANG en ES, EN o JA, o eligiendo un preset de palabras: CANTOR ES, CANTORA ES, "
      "SINGER EN, UTA JA, WORD CHOIR, SCAT EN, ROBOT EN o WHISPER JA. Cada nota canta la sílaba siguiente de la letra; "
      "mantenida, sostiene la vocal, y al soltar suenan las consonantes finales. Varias voces a la vez son un coro."),
    R("LYRIC",
      "TEXT es tu letra. W1 a W16 son frases de palabras reales sacadas a suerte, siempre las mismas para cada número. "
      "S1 a S8 son scat: sílabas sin sentido."),
    R("THEME", "De qué hablan las frases a suerte: LOVE, NIGHT, SEA, SKY, ANY, ROAD, DREAM o FIRE."),
    R("POS", "AUTO avanza sola. Un número hace que la próxima nota cante esa sílaba: sirve para empezar un verso."),
    R("LOOP", "FREE recorre toda la letra. 1, 2, 4 u 8 vuelve al principio cada tantas vueltas: un estribillo."),
    R("ARTIC y SHIFT", "Consonantes nítidas o arrastradas, y una voz más grande o más pequeña."),
    P("Para escribir la letra: EDIT, WORDS y OCT+. Las teclas escriben como en un teléfono antiguo, hasta 63 "
      "caracteres; KNOB 3 propone una frase y OCT+ la confirma. Desde el ordenador, el editor web tiene el recuadro "
      "Lyric."),
    B("El castellano se lee tal como se escribe, sin tildes. La ñ se escribe NY: NINYO."),
    B("El japonés va en romaji, una nota por mora: sakura son tres notas."),
    B("El inglés sale de un diccionario de unas 2.100 palabras; fuera de él se entiende peor."),
    B("Un guion suelto sostiene la vocal una nota más. Un guion bajo une dos palabras: MI_AMOR."),
    N("SE ENTIENDE A MEDIAS",
      "Es síntesis por formantes, como un chip de voz antiguo que canta. Las vocales y s, f, t, k, p, ch, m, n y l se "
      "entienden; b, d y g son aproximadas, y con notas muy cortas se pierde."),
    H("ANIMAL"),
    P("Hace los cantos como los hacen los animales: una fuente que vibra o golpea y uno o dos resonadores. Una nota "
      "empieza un canto, así que una secuencia de notas es una secuencia de cantos, afinada con el resto. MODE elige "
      "la fuente y cambia el nombre de los mandos."),
    R("CMOS", "Un circuito de chips que pía, como una máquina de pájaros, monos y bichos."),
    R("PULSE", "Grillos, cigarras, ranas y sapos."),
    R("WHSTL", "Silbidos de pájaro."),
    R("WING", "Mosquito, abeja y mosca: un aleteo que pasa."),
    R("GLOTT", "Mamíferos: lobo, perro, rugido y ronroneo."),
    R("RATE, GROUP y TEMP",
      "Cuántas sílabas por segundo, cómo se agrupan (HOLD, TRILL o tantas sí y tantas no) y la temperatura, que "
      "acelera a insectos y ranas."),
    R("SIZE", "Mueve el animal entero: menos es más pequeño y agudo."),
    N("DONDE SUENAN BIEN",
      "Los 24 presets están pensados para tocarse alrededor de Do5. Ranas y mamíferos, una o dos octavas por debajo."),
    H("El filtro, en todos"),
    R("EDIT, FILTER", "TYPE, CUT, RES y un cuarto mando: DRIVE en ANALOG, el DIST de la pista en los demás."),
    R("TYPE", "LP, HP, BP, LP24 (24 dB por octava), MS20 (agresivo, grita con RES alto), COMB y COMB-."),
    R("COMB y COMB-", "Un peine afinado a la nota que tocas: metálico el primero, hueco como un clarinete el segundo."),
    R("MORPH y DRIFT",
      "En ANALOG y PHASE, EDIT, MORPH desliza la onda hacia las siguientes. DRIFT desafina cada voz a su manera, como "
      "un analógico viejo."),
    H("Las voces"),
    R("EDIT, VOICE", "POLY, MONO, LEG (ligado) o UNI (unísono); GLIDE; y PRIO: LAST, LOW o HIGH."),
    R("VOICE 2", "ALLOC, DETUNE (la apertura del unísono) y VEL."),
    P("Las seis pistas comparten las voces: 12 con los dos núcleos en marcha, 8 con uno. FM6 toca como mucho cuatro "
      "por pista."),
    H("FOCUS"),
    P("En los sintes, EDIT, FOCUS pone en grande uno de los ocho parámetros del motor: KNOB 1 lo elige, KNOB 2 lo "
      "ajusta, KNOB 3 le pone una fuente de modulación y KNOB 4 la cantidad. Es la forma rápida de modular algo sin "
      "pasar por la matriz."),
    H("Cada motor, su criatura"),
    P("En EDIT, SOUND y en HOME, BEING cada motor dibuja la suya, viva con el sonido: ANALOG una célula, FM6 una "
      "colonia de operadores cableados como el algoritmo, PHASE un caracol, MATH una oruga, WHEEL nueve gusanos de "
      "tubo, NOISE un enjambre, VOICE una cabeza que articula lo que canta y ANIMAL un bicho entre pájaro, insecto y "
      "mamífero."),
};

constexpr Block kDrum[] = {
    P("DRUM-X es un kit de ocho sonidos sintetizados en el momento: BD (bombo), SD (caja), CP (palmada), CH y OH "
      "(charles cerrado y abierto), TM (tom), RS (aro) y CB (cencerro). Cada sonido tiene dos caras, A y B, y el mando "
      "MORPH mueve el kit entero de una a otra: es el mando para tocar en directo."),
    R("Los once kits",
      "DRUM-X, 909 CLUB, 808 BOOM, LOFI DUST, INDUSTRIAL, ACOUSTIC, DUB ROOTS, TECHNO RAW, BREAKS, HIP HOP y LATIN "
      "PERC. Se eligen con PRESETS, como cualquier sonido."),
    H("Las teclas"),
    KEYS(white(5) | white(6) | black(6),
         "El kit está en las mismas teclas en todas las octavas: Do es el bombo, Re la caja y Fa# el charles cerrado. "
         "Aquí, Do4, Re4 y Fa#4."),
    P("En una pista DRUM, OCT- y OCT+ no cambian qué sonido toca cada tecla, sino cómo suena. La pantalla lo dice al "
      "cambiar. Con el mapa EXTENDED, el de fábrica:"),
    R("OCT +2 y +3", "RATCH SETS: el golpe repetido dentro del paso mientras mantienes la tecla."),
    R("OCT +1", "FILLS: redobles de verdad, nueve clases, con un platillo en el tiempo fuerte siguiente."),
    R("OCT 0", "El kit de siempre."),
    R("OCT -1", "KICK / TOM: bombo en las teclas de abajo y tom en las de arriba, afinados por semitonos."),
    R("OCT -2", "FX KIT: los sonidos en variantes más largas, plegadas o con más ruido."),
    R("OCT -3", "FLAM: un golpe de adorno suave y el golpe unos 20 ms después."),
    R("Mapa CLASSIC",
      "EDIT, KIT 4, KNOB 2 (KEY MAP). Entonces: +3 doble golpe, +2 y +1 redobles al paso, -1 SHORT (mitad de cola), -2 "
      "GHOST (nota fantasma) y -3 FLAM."),
    N("MELODIA DE CENCERRO",
      "KEY MAP también puede poner un solo sonido, de KICK a BELL, afinado en las 27 teclas de la octava -1."),
    H("Las páginas de EDIT"),
    P("Van primero el kit entero, después un sonido, y al final los volúmenes y el estéreo."),
    R("KIT", "MORPH, TUNE, TONE y DECAY. Son también los mandos de HOME."),
    R("KIT 2", "NOISE, FM ENV (un filo en el ataque que deja la cola limpia), FOLD (un plegador de onda) y DRIVE."),
    R("KIT 3",
      "BP (un pasa banda sobre los metales), FEEDBK, HANDS (más gente aplaudiendo, nunca a tempo) y GLUE, un "
      "compresor que pega los golpes."),
    R("SOUND 1 a 4",
      "Un sonido cada vez: KNOB 1 lo elige, y tocar su tecla también. Cara A o B, afinación y su caída, DECAY, NOISE, "
      "COLOR, forma de onda, LAYER y SEND a un efecto."),
    R("LEVELS y LEVELS 2", "El volumen de cada uno de los ocho sonidos."),
    R("X-MOD", "MORPH oscilando solo, a tempo: RATE, DEPTH, SHAPE y RAND, que da a cada golpe su propio MORPH."),
    R("STEREO", "IMAGE reparte el kit por el estéreo. El bombo se queda cerca del centro."),
    N("EL GUIRO",
      "En SOUND 3, el filtro de ruido GR convierte cualquier sonido en un güiro: un golpe fuerte toca la pasada "
      "entera y uno suave solo su comienzo. Es el del kit LATIN PERC."),
    H("La rejilla"),
    P("En una pista DRUM, SEQ, STEP no es un piano roll sino una rejilla. Las 16 teclas blancas son los pasos de la "
      "página y las negras 1 a 8 eligen el carril, y lo tocan."),
    KEYS(kAllWhite | black(1) | black(2) | black(3) | black(4) | black(5) | black(6) | black(7) | black(8),
         "Blancas: los 16 pasos. Negras 1 a 8: los carriles BD, SD, CP, CH, OH, TM, RS y CB."),
    R("Negra 9, mantenida", "ACC: las blancas ponen y quitan acentos."),
    R("Negras 10 y 11", "Página anterior y siguiente, cuando el patrón pasa de 16 pasos."),
    R("KNOB 1 a 4", "STEP, LANE, HIT y ACC."),
    R("HIT", "Cómo golpea ese sonido en ese paso: nada, ON, x2, x4, x8, SHORT, GHOST o FLAM."),
    GRID("BD:x---x---x---x---|SD:----x-------x---|CH:x-x-x-x-x-x-x-x-|OH:--x---x---x---x-",
         "Un ritmo básico: elige el carril con una tecla negra y marca sus pasos con las blancas."),
    P("Cada sonido guarda su propio modo en cada paso: un redoble de charles grabado sobre un paso con bombo repite "
      "solo el charles. Lo que tocas con las octavas se graba tal como sonó."),
    H("Silenciar"),
    R("EDIT + negras 1 a 8", "Silencia cada sonido del kit."),
    R("GLO + negras 7 a 10", "Silencia por grupos: KICK, SNARE, HAT y PERC."),
    H("Dados y vuelta atrás"),
    R("EDIT + SELECT", "Tira los dados del kit. OCT+ se lo queda, OCT- devuelve el de antes."),
    R("EDIT + tecla 1 (Fa3)", "INIT: trae de vuelta el kit de fábrica. Pregunta antes."),
    N("UN KIT POR SECCION",
      "Todas las pistas DRUM comparten un kit, y se guarda con cada sección del proyecto, no con los presets de "
      "usuario. Si otra pista toca DRUM-X, cambiar de kit en una distinta no lo toca: KIT OF T4 KEPT."),
    N("POR MIDI",
      "Las notas 35 a 81 tocan el mapa GM de batería. Por debajo y por encima son las variantes de las octavas."),
};

constexpr Block kSeq[] = {
    P("Cada pista tiene su patrón de 64 pasos, con su propio largo, su figura de paso, su swing y su duración de nota. "
      "Patrones de largos distintos giran unos contra otros."),
    P("Todas las páginas de SEQ muestran el mismo dibujo: arriba, los compases del patrón en pequeño; debajo, el "
      "piano roll del compás con el cursor enmarcado y el cabezal en naranja. Cada página hace algo distinto encima."),
    H("Los grupos de SEQ"),
    R("STEP", "El piano roll, o la rejilla en una pista DRUM. Es la página al encender."),
    R("LENGTH", "El largo (LEN, de 1 a 64), la figura del paso (DIV), SWING y GATE, y los ajustes de pista."),
    R("EUCLID", "EUCLID, MODES y MUTATE. Capítulo 8."),
    R("PLAYHEADS", "Tres cabezales más leyendo los mismos pasos."),
    R("STRETCH", "Estirar, repetir, empujar y rotar la secuencia."),
    R("COMP", "Los dos componentes del paso."),
    R("AUTO", "La automatización y su lista. En una pista VOICE, también LYRIC STEP."),
    R("GENERATE", "El generador de pista, donde puede escribirla."),
    H("Escribir pasos"),
    R("KNOB 1", "Mueve el cursor por los 64 pasos y entra solo en el compás siguiente."),
    R("KNOB 2", "Transpone la nota del paso."),
    R("KNOB 3, TIME",
      "La duración. A la derecha alarga la nota paso a paso, hasta 16. A la izquierda la acorta, y más allá convierte "
      "el paso en ligadura (TIE) y luego en silencio (REST), que borra sus notas."),
    R("KNOB 4, VEL", "La fuerza del paso, de 1 a 127. Más allá de 127: ACC (acento), SLD (ligado) y A+S, los dos."),
    P("Un paso guarda hasta cuatro notas. Una nota solo se alarga sobre pasos vacíos: la siguiente la frena y la "
      "pantalla dice NEXT NOTE."),
    H("Grabar por pasos"),
    S("1", "En STEP, con la música parada, pulsa REC. La pantalla dice KEYS WRITE STEPS."),
    S("2", "Toca una nota o un acorde: entra en el paso del cursor y, al soltar, el cursor avanza."),
    S("3", "Con las teclas pulsadas, gira KNOB 1 o SELECT: la nota dura esos pasos."),
    S("4",
      "Con las teclas pulsadas, pulsa ARP: el acorde se escribe arpegiado. KNOB 1 es la figura, KNOB 2 el orden y "
      "KNOB 3 las octavas."),
    P("Armado y parado, sin teclas pulsadas, SELECT mueve el cursor en lugar de cambiar de página."),
    H("Grabar tocando"),
    P("Pulsa REC en cualquier otro sitio: la pista queda armada y arranca la música. Lo que tocas se suma al patrón, "
      "ajustado a sus pasos, y los mandos que muevas se graban como automatización. Se graba también cuánto dura cada "
      "nota y con qué fuerza."),
    R("REC Q", "Ajuste de pista. En 100 % la grabación cae en la rejilla; en 0 % conserva tu tiempo al 1/64 de paso."),
    R("CLICK y COUNT-IN", "MENU, AUDIO: el clic (OFF, REC u ON), su nivel y uno o dos compases de cuenta."),
    R("REC + tecla 1 (Fa3)", "Borra los pasos y la automatización de la pista. UNDO lo recupera."),
    H("Bloqueos de parámetro"),
    P("En STEP, mantén un paso (su tecla en la rejilla, o la nota que estás entrando) y gira un mando: ese paso recibe "
      "su propio valor del parámetro que ese mando edita en la última página de sonido que visitaste. Con el paso "
      "mantenido, tocar EDIT borra sus bloqueos."),
    N("AUTOMATION FULL",
      "Bloqueos y automatización comparten 64 registros por sección, entre todas las pistas. Al parar, las curvas "
      "grabadas se aligeran solas para que quepa más."),
    R("AUTOMATION", "PLAY la activa o desactiva; EVENT y LOCK cuentan los registros; CLEAR los borra."),
    R("AUTO LIST", "Cada registro en una lista: fila, paso, parámetro y valor. OCT+ sobre uno lo cambia de tipo."),
    H("Copiar y pegar"),
    P("En las páginas con el roll o la rejilla, las 16 teclas blancas son los pasos del compás que se ve."),
    R("Copiar un paso", "Mantén su tecla y gira SELECT."),
    R("Copiar un tramo", "Mantén la primera y la última tecla y gira SELECT. Las teclas 1 y 16: el compás entero."),
    R("Pegar", "Mantén la tecla del paso de destino y toca REC."),
    P("Se copia el paso entero, con notas, duración, acento, componentes y bloqueos. El portapapeles se conserva al "
      "cambiar de pista, pero no se pega de batería a melodía ni al revés. En una pista melódica armada para grabar, "
      "las teclas escriben notas: desarma antes."),
    H("Los componentes del paso"),
    P("Cada paso admite dos componentes, en COMP y COMP 2. Todos tienen los mismos cuatro campos: KNOB 1 ACTION (qué "
      "hace), KNOB 2 LENGTH (cuántos pasos dura), KNOB 3 CONDITION (en qué vueltas actúa) y KNOB 4 VALUE."),
    R("ARP", "Un arpegio durante su duración, sobre el acorde del paso o sobre la escala."),
    R("SEQ", "Cada vuelta, este paso toca la siguiente nota escrita del patrón: tus notas, recombinadas."),
    R("OCT+ y OCT-", "Esos pasos, de una a tres octavas arriba o abajo."),
    R("RATCH", "Cada paso partido en 2 a 8 golpes, iguales, apagándose o creciendo."),
    R("STUT", "Este paso otra vez en lugar de los siguientes."),
    R("MUTE y TRIG",
      "MUTE calla sus pasos cuando la condición se cumple. TRIG es lo contrario: solo suenan cuando se cumple."),
    R("TRANS y RAND", "Mueve las notas por la escala: tantos grados, o al azar."),
    R("LOCK y RAMP",
      "LOCK aplica los bloqueos del paso solo cuando toca. RAMP desliza un parámetro hasta el valor bloqueado, en "
      "pasos o compases, con una curva."),
    R("SHIFT y CHORD", "Los pasos siguientes suenan desde otro lugar del patrón; la nota del paso suena como acorde."),
    H("Las condiciones"),
    R("ALWAYS y 25, 50, 75 %", "Siempre, o a suerte."),
    R("1ST y !1ST", "Solo en la primera vuelta tras PLAY, o en todas menos esa."),
    R("N:M", "La vuelta N de cada M, con M hasta 16. 4:4 es la cuarta de cada cuatro. Con ! delante, lo contrario."),
    R("FILL", "Mientras mantienes FX."),
    R("PRE y NEI", "Si la condición anterior de la pista, o la de la pista vecina, se cumplió."),
    R("ELSE, solo en COMP 2", "Actúa uno solo: el primero si su condición se cumple, y si no el segundo."),
    N("UN EJEMPLO",
      "Para que una nota suene solo en la cuarta vuelta: en su paso, ACTION TRIG y CONDITION 4:4. Las vueltas se "
      "cuentan por pista, cada vez que su patrón da la vuelta."),
    R("GLO, COMPS",
      "Un carril de 16 pasos con componentes que actúan sobre varias pistas a la vez. Las blancas eligen el paso y las "
      "negras 1 a 6 las pistas."),
    H("Ajustes de toda la pista"),
    R("PITCH", "Transpone la pista hasta 24 semitonos. No en baterías."),
    R("CHANCE", "En 100 % toca todos los pasos; menos, los va raleando."),
    R("HUMAN", "La pista toca como una persona: cada nota un poco distinta en fuerza, entrada y duración cada vuelta."),
    H("STRETCH"),
    R("KNOB 1, STRETCH", "x2 a x8 separa los pasos; /2 a /8 los junta. El largo cambia con ellos."),
    R("KNOB 2, REPEAT", "Copia el patrón detrás de sí mismo, de 2 a 8 veces o hasta llenar los 64 pasos."),
    R("KNOB 3, NUDGE",
      "Tres modos, que se eligen con OCT- y OCT+ en esa página: toda la secuencia un paso a un lado; el microtiempo "
      "del paso del cursor, en 1/64 de paso; o el propio paso, que cambia de sitio con su vecino."),
    R("KNOB 4, ROTATE", "La secuencia leída desde otro paso, sin mover nada. Se puede automatizar."),
    P("STRETCH y REPEAT actúan mientras giras y parten siempre de la secuencia original: x2 y luego x4 es x4. UNDO lo "
      "deshace."),
    H("PLAYHEADS"),
    P("Tres cabezales más leen los mismos pasos que la pista. KNOB 1 elige uno; RATE es su velocidad respecto a la "
      "pista (de 1/4 a x4, u OFF), DIR su sentido (FWD, REV, PING o RND) e INTVL su intervalo en grados de la escala. "
      "Salen cánones y patrones que se desfasan consigo mismos."),
    H("SEQ TOOLS"),
    KEYS(white(1) | white(2) | white(3) | white(4) | white(8) | white(16),
         "Con SEQ mantenido: tecla 1 CLEAR, 2 REVERSE, 3 y 4 SHIFT a izquierda y derecha, 8 CLR AUTO y 16 CLR STEP."),
    P("En una pista DRUM se añaden las herramientas del carril elegido: tecla 9 CLR, 10 REV y 11 FILL. Los mandos son "
      "LEN, DIV, SWING y GATE. Estas herramientas no tienen UNDO."),
    R("PRESETS en SEQ", "Abre PHRASES, el cargador de patrones. OCT+ carga el elegido; OCT- vuelve."),
    H("Escala, acordes y arpegio"),
    P("La escala es una sola para todo el tema: en SEL, KNOB 1 es ROOT (la tónica), KNOB 2 SCALE (16 escalas) y KNOB 3 "
      "QUANT. Cada sección guarda la suya, así que un tema puede cambiar de tonalidad."),
    R("QUANT",
      "OFF; SNAP ajusta las teclas a la escala; WHITE hace que las blancas la recorran y las negras callen; SEQ ajusta "
      "además las notas del secuenciador."),
    R("ALL TR", "KNOB 4: mueve todas las secuencias por la escala. Si el roll no suena donde dibuja, mira aquí."),
    R("SEL, CHORD",
      "Una tecla toca un acorde. En la escala: TRIAD, 7TH, ADD9, SUS2 y SUS4. Formas fijas: MAJ, MIN, 7, MAJ7, MIN7 y "
      "POWER."),
    R("VOICING y STRUM",
      "CLOSE, LEAD (la inversión más cercana a la anterior), OPEN o +BASS. STRUM rasguea las notas de grave a agudo."),
    R("ARP", "MODE, RATE, OCT y GATE; en ARP 2, SWING, PROB, HOLD y ORDER. Quince modos, además de OFF."),
    R("ARP, modo TRNS", "Las teclas no tocan notas: transponen la secuencia de la pista según su distancia a Do4."),
    N("TECLAS QUE SE QUEDAN",
      "Con el arpegio en OFF, HOLD ON deja las notas sonando al soltarlas, hasta el acorde siguiente."),
};

constexpr Block kGenerate[] = {
    P("JIANT escribe música contigo de tres maneras: EUCLID, que reparte golpes y notas con cuatro mandos; GENE, que "
      "arma un tema entero; y los dados, que tiran una sola parte. Todo se oye al instante y todo se puede deshacer."),
    H("EUCLID: batería"),
    S("1", "Elige la pista DRUM con ALGORITHM, pulsa SEQ y gira SELECT hasta EUCLID."),
    S("2",
      "Con KNOB 4 (LANE) en ALL, gira KNOB 1 (DENSITY, de 1 a 8): sale un kit entero que suena. KNOB 2 (STEPS) es su "
      "largo y KNOB 3 (ROTATE) gira el bombo y la percusión."),
    S("3", "Para retocar un sonido, lleva KNOB 4 a su carril: HITS, STEPS y ROTATE escriben entonces solo ese."),
    S("4", "Gira SELECT un paso a la derecha, a MODES: ACCENT, GHOST, RATCHET y VARY."),
    S("5", "OCT+ da una variación nueva con los mismos ajustes; OCT- vuelve a la anterior."),
    H("EUCLID: melodía"),
    S("1",
      "Pon antes la tonalidad en SEL. Si no, EUCLID busca la de las otras pistas (FROM T y su número) o elige una "
      "(NEW KEY)."),
    S("2", "En EUCLID: KNOB 1 HITS (exactamente esas notas), KNOB 2 STEPS, KNOB 3 ROTATE y KNOB 4 VARY."),
    S("3",
      "En MODES: MOTION (0 es una nota pedal; más, saltos mayores), HARMONY (acordes en los tiempos fuertes), OCTAVE "
      "y SLIDE."),
    R("VARY",
      "De 0 a 50 varía la secuencia sin añadir compases. De 51 a 100 la convierte en cuatro vueltas, A A' A'' B, cada "
      "vez más distintas."),
    N("CON TU SECUENCIA",
      "Si la pista ya tiene una secuencia tuya, EUCLID la transforma con las mismas notas (YOUR SEQ) y te la devuelve "
      "intacta al volver HITS o DENSITY a 0: YOUR SEQUENCE BACK."),
    H("MUTATE"),
    P("La tercera página del grupo: cambia el patrón de EUCLID solo, mientras suena, alrededor de lo que dejaste "
      "puesto, que es el ancla."),
    R("KNOB 1, EVERY", "Cada cuántos compases muta: OFF, 1, 2, 4, 8 o 16."),
    R("KNOB 2, AMOUNT", "Cuánto cambia cada vez."),
    R("KNOB 3, MODE",
      "ORBIT se aleja del ancla y vuelve; DRIFT parte cada vez de la anterior y se va alejando; PULSE alterna una "
      "mutación y el ancla."),
    R("KNOB 4, WHAT", "GROOVE solo dónde caen los golpes; RHYTHM, FEEL y ALL van sumando cosas."),
    R("OCT+ y OCT-", "En esta página: mutar ya, y volver al ancla."),
    N("EN UN MINUTO",
      "Arma un patrón en EUCLID, ve a MUTATE, pon EVERY en 2 y pulsa PLAY. OCT- te devuelve el original."),
    H("GENE, el generador de temas"),
    P("HOME + SELECT desde cualquier página, o HOME tocado hasta su tercer grupo. Son tres páginas: SELECT las "
      "recorre, OCT+ arma la pieza y OCT- vuelve a las criaturas."),
    R("Página 1", "GENRE, SEED (la semilla, de 0000 a 9999), ENERGY y FORM."),
    R("Página 2", "DRUMS, BASS, CHORDS y ARP: cada parte en OFF, en un tanto por ciento o en AUTO."),
    R("Página 3", "LEAD, COMPLEX, SWING y HUMAN."),
    R("GENRE",
      "EXPLORE (ningún género, todas las reglas rotas), RANDOM (a menudo un mutante de dos géneros) o uno de la lista. "
      "También lo cambia PRESETS."),
    P("Los géneros: MINIMAL TECHNO, HARD TECHNO, HARD BASS, HOUSE, DEEP HOUSE, TECH HOUSE, TRIBAL, CUMBIA, REGGAETON, "
      "DEMBOW, ELECTRO, BREAKBEAT, DRUM & BASS, tres IDM, KRAUT, EBM, NEO CLASSIC, FUGA, ELECTRO METAL, TECHNO ROCK, "
      "DREAM POP, PHONK, BAILE FUNK, JAZZ y HIP HOP. Cada uno sabe su tempo, sus escalas, su batería, su bajo y su "
      "espacio."),
    R("FORM: LOOP", "Un bucle de 2 o 4 compases."),
    R("FORM: PARTS",
      "Guarda además las secciones A, B (un respiro sin batería ni bajo) y C (más denso), y el arreglo."),
    R("FORM: SONG", "Una forma de club: intro, subida, descarga y respiro, ya ordenadas en el arreglo."),
    R("FORM: MY LOOP", "Convierte el bucle que estás tocando en un tema. Tu bucle queda intacto como sección A."),
    N("LA MISMA SEMILLA",
      "Los mismos ajustes dan siempre la misma pieza. OCT+ sin tocar nada tira una semilla nueva; girar un mando y "
      "OCT+ da la misma pieza con solo eso cambiado. La pantalla recuerda el género y la semilla de la última."),
    N("LO REEMPLAZA TODO",
      "Cada pieza sustituye pasos, sonidos, kit, mezcla y automatización de todas las pistas. Tus secciones guardadas "
      "no se pisan nunca, y UNDO devuelve lo que había."),
    H("Los dados"),
    P("Un botón mantenido y SELECT tira esa parte de la pista elegida. Se oye al momento, con el antes y el ahora en "
      "pantalla: SELECT otra vez tira de nuevo, OCT+ se lo queda y OCT- devuelve lo que había."),
    R("EDIT + SELECT", "El sonido: un preset de su motor a suerte, con cada parámetro movido un poco."),
    R("SEQ + SELECT", "Una secuencia nueva, la que escribiría GENE para esa pista, encajada con las demás."),
    R("LFO + SELECT", "El LFO de la pista, apuntado sobre todo al filtro."),
    R("FX + SELECT", "La distorsión y los envíos de la pista, y el delay y la reverb compartidos."),
    R("LFO + tecla 1 (Fa3)", "Rutas de macro nuevas en las pistas que suenan. Tecla 2 (Sol3): las borra."),
    H("UNDO"),
    PANEL(cGlo | cRec, "GLO mantenido y la tecla 9 (Sol4), o REC mantenido solo y soltado sin tocar nada más."),
    P("Un paso atrás sobre el último cambio grande, sin parar la música. Otra vez, y lo rehace. Guarda un solo paso y "
      "se pierde al apagar."),
    R("Lo que cubre",
      "Una pieza de GENE, unos dados, una pista o todas borradas, NEW PROJECT, INIT y RECALL, y una sección cargada "
      "con LOAD."),
    R("Lo que no",
      "Saltos de sección, cambiar de proyecto desde la capa SAVE, borrar filas del tema, nada de lo guardado, las "
      "SEQ TOOLS ni los giros normales de un mando."),
    N("SONG PLAYS",
      "Mientras suena un tema no se generan piezas, ni se editan pasos, ni hay UNDO: SONG PLAYS, STOP IT TO EDIT. Para "
      "el tema, o vuelve al bucle con SAVE + tecla 13 (Re5)."),
};

constexpr Block kFx[] = {
    P("FX es una cadena de ocho grupos. Los del grupo TRACK son de la pista elegida; el resto, de todo el tema."),
    FLOW("PISTAS>ENVIOS>DELAY|REVERB>CHORUS>SLICER|DJ FILTER>MASTER>SALIDA",
         "Cada pista manda parte de su señal al delay, la reverb y el chorus. El slicer corta lo que le envíes, y el "
         "master cierra la mezcla."),
    H("Por pista"),
    R("SENDS", "La primera página: DIST y los envíos a CHORUS, DLY y REV."),
    R("DIST", "Cantidad, TYPE (SOFT, HARD, FOLD, CRUSH, RECT o AMP, un amplificador de guitarra) y TONE."),
    R("AUTO PAN", "La pista oscilando por el estéreo: DEPTH, RATE, SYNC (a tempo o libre) y WAVE."),
    R("BUSES", "Lo esencial de los cuatro efectos compartidos en una página."),
    H("Delay"),
    R("DLY",
      "TIME (a tempo, de 4 compases a 1/32, o libre hasta casi 3 segundos), FEEDBK, TONE y MIX. Pasado el 100 % de "
      "FEEDBK, los ecos se sostienen y saturan suave."),
    R("DLY 2", "WIDE (el eco derecho más tarde), PITCH y SPRAY (el delay granular) y MOD, la deriva de una cinta."),
    R("DLY 3", "TYPE (ANALOG, DIGITAL o REVERS), PING (de lado a lado), DRIVE y R TIME, el tiempo del eco derecho."),
    H("Reverb"),
    R("ROOM y SPRING", "Una sala clásica y un tanque de muelles."),
    R("SHIMR", "Sube una octava en cada pasada."),
    R("RESO", "Cuatro cuerdas afinadas a la escala del tema. Con FOLLOW siguen la nota más grave de una pista."),
    R("CLOUD", "Una nube granular. Con SIZE al máximo se congela."),
    R("Los mandos", "TYPE, SIZE (la caída) y dos que cambian según el tipo. En REVERB 2: MOD, RATE, FILT y WIDE."),
    H("Chorus y slicer"),
    R("CHORUS", "RATE, DEPTH y STEREO."),
    R("SLICER", "Corta la mezcla a ritmo: MODE (OFF, GATE o STUT), PAT (16 patrones), RATE y DEPTH."),
    R("SL BUS", "Qué entra en el slicer: cada pista y los retornos de efectos, uno a uno."),
    R("SL PITCH", "Las repeticiones siguen una secuencia de afinación de 16 pasos."),
    H("Master"),
    R("MASTER", "CLIP (carácter, no volumen), PUNCH (engorda la batería), DUCK (el bombo agacha lo demás) y REL."),
    R("TAPE y EQ", "Cinta y un ecualizador de cuatro bandas, en el mismo grupo."),
    R("DJ FILTER",
      "GLO, LIVE, página MANO, KNOB 4. Un solo mando sobre toda la mezcla: a la izquierda pasa bajos, a la derecha "
      "pasa "
      "altos, y en el centro no hace nada."),
    N("CARGA",
      "Si el medidor se llena o hay chasquidos, GLO, SYSTEM, KNOB 2: LOAD en LITE simplifica las reverbs caras y el "
      "delay granular; en ECO quita además slicer y chorus. Los proyectos conservan sus ajustes."),
    H("Envolventes"),
    R("ENV", "ATK, DEC, SUS y REL de la pista."),
    R("ENV DEST", "La envolvente hacia FILTER, PITCH y SHAPE. Con LOOP ON se repite mientras mantienes la nota."),
    R("ENV 2", "Una segunda envolvente que no va a ningún sitio sola: elígela como fuente en la matriz."),
    R("TRACKS", "Las seis envolventes a la vez. ALGORITHM elige la pista."),
    R("STAGE", "Una etapa en grande: KNOB 1 la elige y KNOB 2 la ajusta."),
    H("LFO"),
    R("LFO", "RATE, WAVE (SIN, TRI, SAW, SQR, S&H o DRIFT, un azar suave), MODE y FADE."),
    R("RATE a tempo", "Girado a la izquierda más allá del más lento, se engancha al compás: de 16 compases a 1/32."),
    R("MODE", "TRIG reinicia el LFO en cada frase nueva; FREE corre libre y ninguna nota lo mueve."),
    R("LFO DEST", "Cuánto va a PITCH, FILTER, SHAPE y AMP."),
    H("La matriz MOD"),
    P("Cuatro ranuras por pista, en LFO, MOD: KNOB 1 elige la ranura, KNOB 2 la fuente (SRC), KNOB 3 el destino "
      "(DEST) y KNOB 4 la cantidad (AMT)."),
    R("Fuentes",
      "LFO, ENV, ENV2, VEL (fuerza), KEY (altura), RAND, NOISE, AT (presión), MODW y EXPR (rueda y pedal por MIDI), M1 "
      "a M4 (las macros), STEP (el MSEQ) y PROX (la cercanía de un móvil)."),
    R("Destinos",
      "PITCH, CUT, SHP, AMP, PAN, los envíos, la velocidad del LFO, VIB, los ocho parámetros del motor (E1 a E8), el "
      "master y los buses de delay, reverb y chorus."),
    R("MSEQ", "Un secuenciador de modulación de 16 pasos por pista: KNOB 1 el paso, KNOB 2 su nivel, LEN y SLEW."),
    N("EL DELAY, SECUENCIADO", "La fuente STEP sobre el destino D.TIM secuencia el tiempo del delay."),
    H("Macros M1 a M4"),
    PANEL(cLfo | cKnobs, "Con LFO mantenido, KNOB 1 a 4 son las cuatro macros."),
    P("Al soltar LFO quedan enganchadas: los mandos siguen moviendo las macros en cualquier página, y arriba pone "
      "LATCH, hasta que pulsas LFO otra vez. Cada encendido arranca con rutas a suerte en las pistas que no tienen "
      "ninguna; nunca apuntan a la afinación."),
    R("HOME, MACROS y ROUTES", "Las macros con sus mandos, y un dibujo de adónde va cada una."),
    P("Las rutas son ranuras normales de la matriz, con fuente M1 a M4: se editan a mano en LFO, MOD. Los valores de "
      "las macros se guardan con cada sección."),
};

constexpr Block kLive[] = {
    P("JIANT está hecho para tocarse: silencios y solos, efectos que duran lo que dura un dedo, secuencias que se "
      "tuercen sin perder el compás y secciones que entran en el compás siguiente."),
    H("Silencios y solos"),
    KEYS(black(1) | black(2) | black(3) | black(4) | black(5) | black(6) | white(1) | white(2) | white(3) | white(4) |
             white(5) | white(6) | white(7) | white(8) | white(9),
         "Con GLO mantenido: negras 1 a 6 silencian T1 a T6 y se quedan; blancas 1 a 6 son solos mientras las "
         "mantienes; tecla 7 quita todos los silencios, 8 es TAP y 9 UNDO."),
    R("Negras 7 a 10", "Con GLO: los grupos de la batería, KICK, SNARE, HAT y PERC."),
    R("Los mandos", "Con GLO: el nivel de T1 a T4. Con T5 o T6 elegida, de T3 a T6."),
    R("GLO, MIX", "MIXER: LEVEL, PAN, REV y BPM de la pista elegida. SENDS: sus cuatro envíos."),
    H("FX: efectos de directo"),
    P("Con FX mantenido cada tecla es un efecto que actúa mientras la pulsas y deshace al soltar."),
    KEYS(white(1) | white(2) | white(3) | white(4) | white(5),
         "Sobre el sonido: teclas 1, 2 y 3 REPEAT a 1/8, 1/16 y 1/32; tecla 4 pasa bajos y tecla 5 pasa altos."),
    KEYS(white(6) | white(7) | white(8) | white(9) | white(10) | white(11) | white(12) | white(13) | white(14) |
             white(15),
         "Sobre las notas: 6 y 7 octava abajo y arriba, 8 medio tempo, 9 y 10 notas cortas y largas, 11 y 12 STUTTER, "
         "13 ataques lentos, 14 ARP y 15 RANDOM."),
    R("Negras 1 a 6", "Silencian T1 a T6 solo mientras las mantienes."),
    R("Negra 8 (La#4)", "A qué pistas afectan los efectos de nota: ALL, SYN (los sintes) o DRM (las baterías)."),
    R("Negras 7, 9 y 10", "LOOP 3, 5 y 7: cada pista repite sus últimos pasos, en bucles impares contra el compás."),
    R("Negra 11 (Fa#5)", "GLITCH: la mezcla troceada en semicorcheas, siempre igual, así que tiene ritmo."),
    R("Los mandos", "FILTER, CRUSH, THROW (lanza la mezcla al delay y la reverb) y DEPTH."),
    R("Dejarla abierta",
      "Mantén FX hasta que salga su mapa y suelta sin tocar nada: [FX] LOCKED. Las dos manos libres; OCT- lo apaga "
      "todo y FX la cierra."),
    R("Grabar los efectos",
      "Con REC armado y sonando, los efectos de nota se graban en un carril de 64 pasos, que es la página FX, PUNCH. "
      "FX + tecla 16 (Sol5) lo borra."),
    H("LIVE: torcer las secuencias"),
    P("GLO, grupo LIVE. Modificadores sobre las seis secuencias que nunca tocan los pasos: cambian cómo se recorren, "
      "y la pista siempre vuelve en fase. Dos páginas que mueven lo mismo: MANO, para tocar, y RIELES, para dejar "
      "puesto y ver."),
    R("RIELES, KNOB 1", "PISTAS: a cuáles afectan los otros tres mandos. ALL, una, SYN o DRM."),
    R("RIELES, KNOB 2 a 4",
      "RATE (de /4 a x4), CORTE (solo los primeros pasos) y DENS: a la izquierda ralea, a la derecha repite golpes."),
    P("En MANO las teclas no tocan notas: son una botonera. Mantén negras 1 a 6 para elegir pistas; ninguna es "
      "todas."),
    KEYS(kAllWhite,
         "MANO: tecla 1 mitad, 2 doble, 3 y 4 tresillo y puntillo, 5 a 8 LOOP de 8, 4, 2 y 1 pasos, 9 REV, 10 PING, 11 "
         "y 12 RATCH, 13 THIN, 14 y 15 octava arriba y abajo, 16 RETURN."),
    R("Negra 7 (Sol#4)", "LATCH: cada blanca enciende su modificador y otro toque lo apaga."),
    R("Negras 9, 10 y 11", "El cabezal un paso atrás o adelante, y RND: los pasos en cualquier orden."),
    R("MANO, los mandos", "RATE, LOOP y THIN sobre las pistas elegidas, y el DJ FILTER."),
    R("OCT- tocado", "RETURN: las pistas elegidas pierden sus modificadores en el tiempo siguiente."),
    R("OCT- mantenido", "BYPASS: el original mientras lo aprietas."),
    R("OCT+", "COMMIT: pregunta y, con OCT+ otra vez, escribe en las pistas lo que suena."),
    N("NO SE GUARDAN",
      "Los modificadores son interpretación: PLAY no los borra y un proyecto no los guarda. Queda lo que escribes."),
    H("Proyectos, secciones y filas"),
    FLOW("PROYECTO>SECCION A-D>FILAS",
         "Seis proyectos. Cada uno, cuatro secciones y un arreglo de hasta 16 filas; cada fila es una sección que "
         "suena de 1 a 64 compases."),
    P("Una sección es una foto completa de la música: sonidos, pasos y automatización de cada pista, el kit, el "
      "carril de efectos, el slicer, las macros, el tempo y los efectos."),
    H("SAVE: las secciones en vivo"),
    KEYS(white(1) | white(2) | white(3) | white(4) | white(5) | white(6) | white(7) | white(8),
         "Con SAVE mantenido: teclas 1 a 4 tocan las secciones A a D; teclas 5 a 8 guardan la música en A a D."),
    R("Tocar una sección", "Sonando, entra en el compás siguiente. Parada, al instante."),
    R("Dos o más a la vez", "Una cadena rápida: suenan por turno, una y otra vez."),
    R("SAVE + OCT+ y OCT-", "Guardar en la sección que suena, y recuperarla tal como quedó guardada."),
    R("Tecla 13 (Re5)", "LOOP o SONG: qué toca PLAY, el bucle o el tema."),
    R("Tecla 14 (Mi5)", "SONG REC: las secciones que vayas tocando se escriben como filas del tema."),
    R("Tecla 16 (Sol5)", "Salta a ARRANGE."),
    R("KNOB 1", "Otro proyecto. Cambia al soltar SAVE, solo en parado, y carga su sección A."),
    H("Armar el tema"),
    P("SAVE abre en TIMELINE: el tema en bloques, y la fila elegida abierta pista por pista."),
    R("TIMELINE, los mandos",
      "KNOB 1 la fila, KNOB 2 su sección, KNOB 3 sus compases y KNOB 4 la VAR de la pista elegida."),
    R("OCT+ y OCT-", "Copiar la fila detrás de sí misma, y borrarla (pregunta; no se puede deshacer)."),
    R("Teclas 1 a 6", "Silencian T1 a T6 solo en esa fila."),
    R("Teclas 8 a 11", "FROM: la pista elegida toca en esa fila la parte de otra sección, A a D."),
    R("VAR",
      "Cambia la pista solo en esa fila: MUTE, THIN, FILL, HALF, REV, SHIFT, una octava arriba o abajo, o VAR 1 a 4, "
      "mutaciones cada vez más fuertes."),
    R("PLAY en TIMELINE", "Pone en bucle la fila elegida. PLAY otra vez deja seguir el tema desde ahí."),
    R("PARTS", "Las cuatro secciones como tarjetas. OCT+ pone una en bucle: la forma rápida de oírla sola."),
    R("CLIPS",
      "Las seis pistas por las cuatro secciones, como una rejilla. OCT+ lanza una celda en el compás siguiente. Con "
      "REC armado, lo que lances queda escrito como tema."),
    R("ARRANGE",
      "Las filas y el final: KNOB 1 más allá de la última fila llega a END, donde KNOB 2 elige STOP, de x2 a x8 o "
      "LOOP. PLAY aquí toca el tema desde el principio."),
    R("Escenas", "En ARRANGE, KNOB 4 a la derecha guarda en la fila los silencios y las macros de ese momento."),
    N("PATTERN A EMPTY",
      "El tema no arranca si una fila apunta a una sección vacía. Guarda música en ella o cambia la fila."),
    N("TU SONIDO SE QUEDA",
      "Si cambias el sonido de una pista mientras suena el tema, lo conserva en las secciones siguientes. EDIT + tecla "
      "2 (Sol3), RECALL, le devuelve el guardado."),
    H("SCENE: una tecla, toda la banda"),
    P("En SEL, página SCENE. Con SCENE en ON, cada tecla llega a la vez a todas las pistas que tengan un papel, y "
      "cada una la toca a su manera."),
    R("Los papeles", "KEEP (sigue a lo suyo), CHORD, ARP, BASS (una octava abajo), LEAD (una arriba) y DRUM."),
    R("Los mandos", "KNOB 1 SCENE, KNOB 2 la pista, KNOB 3 su papel y KNOB 4 su acorde, su patrón o su estilo."),
    R("OCT+ y OCT-", "Repartir los papeles a suerte, y ponerlas todas en KEEP."),
    P("Con REC armado y sonando, cada pista graba lo que tocó en sus propios pasos: una sola toma escribe toda la "
      "escena."),
};

constexpr Block kSave[] = {
    P("Este es el capítulo que hay que leer despacio. JIANT guarda varias cosas en sitios distintos y de maneras "
      "distintas, y lo que suena ahora mismo no está guardado en ninguno."),
    H("Qué se guarda y cómo"),
    R("La música que suena", "Vive en la memoria volátil. No se guarda sola: guárdala en una sección."),
    R("Una sección", "SAVE, SECTIONS, en parado; o la capa SAVE mientras suena."),
    R("El arreglo", "Solo, un par de segundos después de cada cambio, con la música parada."),
    R("Un preset de usuario", "SAVE, USER, en parado. 32 ranuras, de U01 a U32."),
    R("La configuración", "Sola: el MENU al cerrarlo."),
    R("El banco FM6", "Se envía desde el editor web y no va en la copia de seguridad."),
    N("APAGAR NO GUARDA",
      "De fábrica, MENU, SYSTEM, RESTORE LAST está en OFF y la FM-1 enciende en cero. En ON, guarda sola una copia "
      "10 segundos después del último cambio, parada y sin tocar nada, y la recupera al encender. Nunca guarda en el "
      "momento de apagar."),
    H("Guardar una sección"),
    S("1", "Para la música. Guardar necesita el transporte parado: si no, STOP TO SAVE."),
    S("2", "Pulsa SAVE y gira SELECT hasta SECTIONS."),
    S("3", "KNOB 1 elige la sección, de A a D. La que dice NOW es de la que vino la música."),
    S("4", "Gira KNOB 4 a la derecha hasta que la tarjeta SAVE diga OCT+, y pulsa OCT+."),
    S("5", "Si la sección ya tenía algo, pregunta. OCT+ para sobrescribir."),
    S("6", "Se abre NAME con un nombre listo. Cámbialo o no, y pulsa OCT+. La pantalla dice SAVED."),
    R("Cargar", "KNOB 3 a la derecha (LOAD) y OCT+. Para la música y la reemplaza; UNDO devuelve la anterior."),
    R("Borrar", "KNOB 2 a la derecha (ERASE) y OCT+. Pregunta antes."),
    R("Otro proyecto", "En PROJECTS, KNOB 1 lo elige sin cambiar la música que suena: sirve para guardarla allí."),
    R("SAVE SONG AS", "Copia el tema entero, con secciones y arreglo, a otro proyecto."),
    N("GUARDADO RAPIDO",
      "La capa SAVE guarda mientras suena (teclas 5 a 8, u OCT+), pero no llega a la memoria permanente hasta que "
      "paras. Para siempre antes de apagar."),
    H("Poner nombres"),
    P("La pantalla NAME sale al guardar y con SAVE + tecla 12 (Do5). Hasta 12 caracteres. Las teclas blancas escriben "
      "como en un teléfono antiguo: AB, CD, EF, y así; vuelve a tocar antes de un segundo para la letra siguiente."),
    R("Fa# y La#", "Cursor a la izquierda y a la derecha."),
    R("Sol#, Do# y Re#", "Espacio, borrar y cambiar entre letras y números."),
    R("KNOB 1 y KNOB 2", "Mover el cursor y cambiar el carácter."),
    R("OCT+ y OCT-", "Escribir y cancelar."),
    H("Presets de usuario"),
    P("En SAVE, USER: KNOB 1 la ranura, KNOB 2 LOAD, KNOB 3 ERASE y KNOB 4 SAVE; gira a la derecha para armar y pulsa "
      "OCT+. Guardan el motor, su sonido y un patrón de 16 pasos, pero no el kit de batería. Aparecen en PRESETS "
      "después de los de fábrica."),
    R("SAVE, PRESETS",
      "Buscar sonidos: KNOB 1 el número, KNOB 2 el motor, KNOB 3 marca favorito y KNOB 4 filtra por tipo."),
    R("SAVE, TOOLS",
      "CLEAR SEQ, INIT SOUND, DELETE ROW, CLEAR SONG y NEW PROJECT. Se ejecutan manteniendo OCT+ un segundo."),
    H("Mensajes al guardar"),
    R("STOP TO SAVE", "Para la música con PLAY y repite."),
    R("SAVED (NO FLASH)", "No encontró la memoria: quedó solo en volátil. Reinicia; si sigue, haz copia y reinstala."),
    R("SECTION FULL", "La sección no cabe. Acorta patrones o borra automatización; la guardada queda intacta."),
    H("Sistema"),
    R("TRACKS", "GLO, SYSTEM, KNOB 1: 4 pistas (de fábrica) o 6."),
    R("LOAD", "KNOB 2: FULL, LITE o ECO. Cuánto puede pesar el sonido."),
    R("ROUTE", "KNOB 3: CH1-6 (cada canal MIDI toca su pista) o SEL (todos tocan la pista elegida)."),
    R("GLO, GLOBAL", "BPM (de 40 a 240), SWING, CLOCK (INT, USB +BT o TRS) y TUNE."),
    R("KEY SPLIT", "También en el grupo SYSTEM: parte el teclado entre dos pistas."),
    H("El MENU, lo que importa"),
    R("DISPLAY", "COLOR (once paletas), SKIN, SCOPE, LARGE (letra grande), LEDS y SCREEN OFF."),
    R("CONTROL", "HOLD, FX LATCH, BPM LOCK, SCALE LEDS, MIDI OUT, LEARN y los cuatro ajustes de las teclas."),
    R("AUDIO", "SPEAKER EQ, USB LEVEL, CLICK, COUNT-IN, TRACKS, LOAD y OCT BEND."),
    R("SYSTEM", "USB, RESTORE LAST, CPU CORES, CPU CLOCK, BLUETOOTH, CALIBRATION, INIT SYSTEM y ABOUT."),
    R("SCALE LEDS", "Ilumina las teclas de la escala, la tónica más brillante."),
    R("BPM LOCK", "Los generadores respetan tu tempo."),
    H("MIDI y USB"),
    P("Por USB, la FM-1 es a la vez MIDI y una entrada de audio para el ordenador, sin instalar nada. Aparece con el "
      "nombre FM-1. El minijack es solo entrada MIDI."),
    R("MIDI OUT", "MENU, CONTROL: KEYS (las teclas), CLOCK (además el reloj), ALL (además todo lo que toca) u OFF."),
    R("Canales", "T1 a T6 por los canales 1 a 6. El canal 16 es el del tema."),
    R("Cambio de programa",
      "En el canal de una pista, carga su sonido. En el 16: 0 a 3 saltan a A-D y 16 a 21 cargan proyecto."),
    R("Controles", "CC 7 nivel, 10 panorama, 74 corte, 71 resonancia, 91, 93 y 94 envíos, y 16 a 23 el motor."),
    R("MIDI LEARN", "MENU, CONTROL, LEARN. Gira un mando de la FM-1 y mueve el de tu controlador. Hasta diez."),
    R("USB: STEMS", "MENU, SYSTEM, USB: ocho canales de audio, uno por pista y dos con los efectos."),
    R("USB: HOST", "Un teclado MIDI USB enchufado directo, con su propia alimentación o un concentrador alimentado."),
    H("Bluetooth"),
    R("BLUETOOTH: ON", "MIDI por Bluetooth. Se conecta desde una app del móvil; aparece como JIANT FM-1."),
    R("PROX", "Una fuente de modulación según lo cerca que está el móvil conectado. Se elige en la matriz."),
    R("Calibrar PROX", "OCT+ en la fila BLUETOOTH. Con el móvil lejos, OCT-; apoyado en la FM-1, OCT+."),
    R("BLUETOOTH: AIR", "Solo escucha los aparatos de alrededor. Para PROX sin móvil en la mano, y para SWARM."),
    H("Si algo no responde"),
    R("SELECT no cambia el tempo", "El tempo es GLO + SELECT, la tecla TAP de GLO o el BPM de GLOBAL."),
    R("Los mandos mueven otra cosa", "Las macros están enganchadas: arriba pone [LFO] LATCH. Pulsa LFO."),
    R("T5 o T6 no suenan", "TRACKS está en 4."),
    R("Sigue sonando algo borrado", "Es el carril de efectos: mantén REC y pulsa FX."),
    R("Una nota colgada", "PLAY mantenido: PANIC."),
    R("Chasquidos", "TRACKS en 4, LOAD en LITE o ECO y menos voces. Mira ABOUT, AUDIO LOAD."),
    R("Un botón hace otra cosa", "Calibra: MENU, SYSTEM, CALIBRATION."),
    R("No guarda al apagar", "Una FM-1 reciente con JIANT antiguo: instala la versión actual."),
    H("Chuleta"),
    R("Moverse", "SELECT páginas, ALGORITHM pista, PRESETS sonidos, KNOB 1 elige, HOME mantenido el MENU."),
    R("Hacer y volver", "OCT+ y OCT-."),
    R("Tempo", "GLO + SELECT."),
    R("UNDO", "GLO + tecla 9, o REC solo."),
    R("Un tema nuevo", "HOME + SELECT y OCT+."),
    R("Dados", "EDIT, SEQ, LFO o FX + SELECT."),
    R("Silenciar", "GLO + negras 1 a 6."),
    R("Efectos de directo", "FX mantenido y las teclas."),
    R("Guardar ya", "SAVE + teclas 5 a 8. Y parar antes de apagar."),
    R("Tocar una sección", "SAVE + teclas 1 a 4."),
    R("Borrar la pista", "REC + tecla 1."),
    R("Borrar un paso", "SEQ + tecla 16."),
    R("Pánico", "PLAY mantenido."),
};

constexpr Chapter kChapters[] = {
    FMGUIDE_CHAPTER("Qué es JIANT FM", kWhat),
    FMGUIDE_CHAPTER("Instalar, volver y copias", kInstall),
    FMGUIDE_CHAPTER("Moverse: las seis reglas", kMove),
    FMGUIDE_CHAPTER("Primer sonido, primer tema", kFirst),
    FMGUIDE_CHAPTER("Los doce motores", kEngines),
    FMGUIDE_CHAPTER("DRUM-X, la batería", kDrum),
    FMGUIDE_CHAPTER("El secuenciador", kSeq),
    FMGUIDE_CHAPTER("EUCLID, GENE y los dados", kGenerate),
    FMGUIDE_CHAPTER("Efectos, modulación, macros", kFx),
    FMGUIDE_CHAPTER("Directo y canciones", kLive),
    FMGUIDE_CHAPTER("Guardar, sistema y chuleta", kSave),
};

}  // namespace

const Guide kGuideJiant = {
    "JIANT FM",
    "Seis pistas, doce motores y un generador que escribe temas enteros.",
    "V 10.7",
    kChapters,
    static_cast<int>(sizeof(kChapters) / sizeof(kChapters[0])),
    "18-11-2026",
    nullptr,
    0,
};

}  // namespace fmguide

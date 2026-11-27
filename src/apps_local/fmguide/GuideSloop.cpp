#include "GuideData.h"

// SLOOP 2.8.5, by 3dSam (isod89/sloop-fm1), from its GUIDE.md of 3 November
// 2026.

namespace fmguide {

namespace {

constexpr Block kWhat[] = {
    P("SLOOP convierte la FM-1 en una caja de ritmos para tocar en directo. Tiene cuatro pistas: tres sintetizadores "
      "y una batería. No programas pasos: tocas, y el bucle se cierra solo con el tempo que llevabas. Después grabas "
      "encima, capa sobre capa."),
    FLOW("PISTA 1>PISTA 2>PISTA 3>BATERIA",
         "Tres pistas de sinte, que comparten ocho voces, y una de batería con "
         "seis voces propias. Cada pista tiene un color y su mando KNOB."),
    R("Pista 1, azul", "Un sinte. Su mando es KNOB 1. Al empezar: el bajo 808 BOOM."),
    R("Pista 2, verde", "Un sinte. KNOB 2. Al empezar: RHODES."),
    R("Pista 3, amarilla", "Un sinte. KNOB 3. Al empezar: LOFI FLUTE."),
    R("Pista 4, naranja", "La batería: 16 sonidos, uno por tecla blanca. KNOB 4. Al empezar: el kit 808."),
    P("La pantalla y todo el editor toman el color de la pista elegida. El blanco siempre señala lo que estás "
      "tocando, y el rojo siempre significa que está grabando."),
    H("Lo que trae"),
    B("153 sonidos de fábrica sobre doce motores, y 46 kits de batería."),
    B("Grabación en vivo con sobregrabación, y un secuenciador de pasos para corregir."),
    B("27 efectos de pinchazo sobre toda la mezcla, uno por tecla."),
    B("Acordes con un dedo, en la tonalidad que elijas."),
    B("Una canción de cuatro secciones, A a D, que se encadenan tocando."),
    B("Audio y MIDI por USB, y un editor web con piano roll y carga de muestras."),
    N("PARA QUIEN",
      "Es el firmware más inmediato de los cuatro: en un minuto suena algo. Si lo que buscas es diseñar "
      "un sonido operador por operador, Felucca o FM-1+VA te darán más."),
};

constexpr Block kInstall[] = {
    S("1", "Abre la página del instalador (isod89.github.io/sloop-fm1) en Chrome o Edge."),
    S("2", "Conecta la FM-1 por USB con un cable de datos, directamente, sin concentrador."),
    S("3", "Pulsa INSTALL, permite el acceso MIDI y espera a que diga Done. La FM-1 se reinicia con el logo de SLOOP."),
    P("El instalador comprueba el paquete antes de escribir y el cargador de SLOOP lo vuelve a comprobar antes de "
      "arrancarlo: un paquete dañado se rechaza sin tocar nada."),
    H("Actualizar"),
    P("Actualizar conserva tus proyectos, presets, muestras, kits y ajustes. Aun así, guarda una copia en el editor "
      "antes de cada actualización. Si piensas volver a una versión antigua (2.3 o anterior), la copia es "
      "imprescindible: esas versiones no leen los proyectos nuevos."),
    H("Copia de seguridad"),
    P("En el editor web, Projects, Backup, Save a backup escribe todo en un archivo: la música en curso, las secciones "
      "A a D, los 32 presets de usuario, el banco FM6, tus kits y muestras, y los ajustes. Restore from a file lo "
      "devuelve todo; para antes la canción."),
    H("Volver al firmware original"),
    P("Desde la versión 2.8 se hace con el actualizador de M-VAVE, M-UPGRADE, y el archivo oficial FM-1.fwsc (V16). "
      "El botón de volver que tenía el instalador web se quitó porque no era fiable en todos los sistemas. Cierra "
      "antes cualquier otra aplicación que use MIDI."),
    N("SI NO ARRANCA",
      "Mantén OCT- solo mientras enciendes: aparece SLOOP USB RESCUE y puedes instalar de nuevo desde "
      "el navegador. Si una instalación se cortó, la FM-1 espera en modo actualización: pulsa INSTALL "
      "otra vez."),
};

constexpr Block kPanelCh[] = {
    PANEL(cAlgorithm | cPresets | cSelect,
          "Los tres mandos que más usarás: ALGORITHM elige la pista, PRESETS su "
          "sonido o el kit, SELECT el tempo."),
    R("ALGORITHM", "Elige la pista, 1 a 4, en cualquier pantalla."),
    R("PRESETS", "El sonido de la pista elegida; en la batería, el kit."),
    R("SELECT",
      "En la pantalla TRACKS y dentro de una capa, el tempo. En una página, pasa a la anterior o la "
      "siguiente."),
    R("KNOB 1 a 4", "Los cuatro diales que la pantalla dibuja abajo."),
    R("OCT- y OCT+",
      "En un sinte, la octava (los dos a la vez: octava 0). En la batería, mantenidos mientras tocas: "
      "golpes fantasma y golpes fuertes."),
    H("Las dos vidas de cada botón"),
    P("Cada botón de función hace dos cosas. Si lo tocas, abre sus páginas, como en cualquier firmware: tócalo otra "
      "vez para la página siguiente. Si lo mantienes, abre una capa: mientras está pulsado, las 16 teclas blancas y "
      "los cuatro KNOB hacen otra cosa, y la pantalla dibuja las teclas como 16 casillas, en cuatro filas de cuatro."),
    PANEL(cFx | cEdit | cArp | cSeq | cSel | cGlo | cSave, "Los siete botones con capa. ENV y LFO solo abren páginas."),
    R("FX", "Tocar: páginas de efectos. Mantener: efectos de pinchazo."),
    R("EDIT", "Tocar: páginas del motor. Mantener: borrar."),
    R("ARP", "Tocar: arpegiador. Mantener: repetición de nota (redobles)."),
    R("SEQ", "Tocar: secuenciador. Mantener: pasos."),
    R("SEL", "Tocar: escalas. Mantener: tonalidad y acordes."),
    R("GLO", "Tocar: ajustes globales. Mantener: mezcla."),
    R("SAVE", "Tocar: canción o páginas de guardar. Mantener: secciones."),
    N("DEJAR UNA CAPA FIJA",
      "Mantén el botón de la capa y toca HOME. La capa se queda abierta con las dos manos "
      "libres. Cualquier otro botón la suelta. PLAY, REC y las octavas siguen funcionando "
      "dentro."),
    H("Los botones que no tienen capa"),
    R("PLAY", "Arranca y para las cuatro pistas. Funciona dentro de cualquier capa."),
    R("REC",
      "Tocar: grabar. Mantener unos 2 s: borrar la pista elegida (un anillo se llena; suelta antes y no pasa "
      "nada)."),
    R("HOME",
      "Tocar: pantalla TRACKS, y desde ella el visualizador. Mantener: el menú (pantalla, luces, audio, "
      "sistema)."),
    R("ENV y LFO", "Sus páginas de envolvente y de LFO."),
};

constexpr Block kFirst[] = {
    P("Con la FM-1 recién arrancada en SLOOP, esto te lleva de cero a un bucle con batería, bajo, acordes y efectos."),
    S("1",
      "Gira ALGORITHM hasta la pista 4 (naranja). Las teclas blancas tocan 16 sonidos de batería. Gira PRESETS "
      "para elegir kit: prueba 808 o BOOMBAP."),
    KEYS(white(1) | white(3) | white(5), "Para empezar: tecla 1, bombo; tecla 3, caja; tecla 5, charles."),
    S("2",
      "Pulsa REC. La pantalla dice rec ready. Toca un ritmo libremente, a tu tempo, sin claqueta. Mantén OCT- "
      "mientras golpeas para notas fantasma y OCT+ para golpes fuertes."),
    S("3",
      "Pulsa REC en el «uno» siguiente a tu último compás. El bucle se cierra: su duración fija el tempo, los "
      "golpes se ajustan a la rejilla y empieza a sonar."),
    S("4",
      "Pulsa REC otra vez mientras suena: grabas encima. Mantén ARP y mantén la tecla del charles (tecla 5): un "
      "redoble en semicorcheas."),
    S("5", "Gira ALGORITHM a la pista 1 (azul, 808 BOOM), pulsa REC y toca una línea de bajo."),
    S("6",
      "Mantén SEL y pulsa la tecla de la tonalidad de tu tema (por ejemplo Re). Elige la pista 2, mantén SEL y "
      "gira KNOB 1 hasta 7TH: cada tecla blanca es ahora un acorde de la tonalidad, con un dedo."),
    S("7",
      "Mantén FX y mantén una tecla, blanca o negra: un efecto sobre toda la mezcla. Sin soltar FX, gira KNOB 2 "
      "para DUST y KNOB 3 para DUCK."),
    S("8", "¿Un error? Mantén EDIT y pulsa OCT-: deshacer."),
    N("EL METRONOMO",
      "La luz de PLAY parpadea en cada tiempo: es un metrónomo silencioso. Para uno que suene: GLO, "
      "GLOBAL, CLICK."),
};

constexpr Block kRecord[] = {
    P("SLOOP graba en vivo y cada pasada se suma a la anterior. Las notas van al paso más cercano tal como las "
      "oíste: descuenta el retraso entre la tecla y el sonido, así que lo que tocas a tiempo cae a tiempo."),
    H("Qué hace REC"),
    R("Sonando", "Graba la pista elegida al instante. REC otra vez deja de grabar; el bucle sigue."),
    R("Parado, con notas", "Se arma (rec ready, la luz parpadea). Tu primera nota arranca el bucle y es el paso 1."),
    R("Parado, vacío", "Se arma para una toma libre: el tempo te sigue a ti."),
    P("Mientras graba, la luz de REC queda fija. Si giras ALGORITHM, la toma pasa a la pista siguiente sin parar."),
    H("La toma libre"),
    S("1",
      "En un proyecto vacío, pulsa REC y toca. La pantalla muestra los segundos y el bucle que saldría ahora "
      "mismo, por ejemplo 2 bars 92 bpm."),
    S("2",
      "Pulsa REC en el «uno» tras tu último compás. SLOOP elige 1, 2 o 4 compases al tempo más cercano y lo pone "
      "a sonar. Las cuatro pistas toman esa duración."),
    S("3", "PLAY durante una toma libre la descarta. Una toma se cierra sola a los 24 segundos."),
    H("La pantalla REC"),
    P("Con REC armado y antes de la primera nota, los cuatro KNOB ajustan cómo se graba:"),
    R("KNOB 1, mode", "Solo en proyecto vacío. free: toma libre. tempo: graba al tempo fijado con SELECT."),
    R("KNOB 2, length", "La duración del bucle de la pista: 1, 2 o 4 compases."),
    R("KNOB 3, start", "note: tu primera nota arranca. count: pulsa PLAY y cuenta un compás de clics antes."),
    R("KNOB 4, quant",
      "on: lo grabado cae en los pasos. off: conserva tu pulso; cada paso guarda cuánto te "
      "adelantaste o atrasaste."),
    H("Qué se graba y qué no"),
    B("Sintes: notas y acordes con su duración. Una pista POLY guarda hasta 4 notas por paso; los bajos y solistas "
      "suelen ser MONO y guardan una."),
    B("Batería: los golpes con su nivel (fantasma, suave, normal, fuerte) y los redobles de ARP."),
    B("Nunca: los efectos de pinchazo, la claqueta, ni lo que toques dentro de una capa que usa las teclas para "
      "otra cosa."),
    H("Swing"),
    P("De 0 (recto) a 100. Para todas las pistas: KNOB 1 en la pantalla TRACKS. Para una pista: mantén SEQ y gira "
      "KNOB 3. El de la pista se suma al global."),
};

constexpr Block kDrums[] = {
    P("En la pista 4 cada tecla blanca es un sonido. Una tecla negra toca el sonido de la blanca que tiene a su "
      "izquierda: dos dedos sobre el mismo sonido, para redobles rápidos."),
    KEYS(kAllWhite, "Las 16 teclas blancas de la batería, numeradas."),
    R("1 y 2", "Bombo y bombo 2."),
    R("3 y 4", "Caja y palmada."),
    R("5, 6 y 7", "Charles cerrado, abierto y de pedal. El cerrado y el de pedal cortan al abierto."),
    R("8 y 9", "Aro y caja 2."),
    R("10 y 11", "Timbal grave y agudo."),
    R("12 y 13", "Crash y ride."),
    R("14, 15 y 16", "Shaker, conga y cencerro."),
    H("Cuatro niveles"),
    P("Como las teclas no miden la fuerza, cada golpe tiene uno de cuatro niveles: GHOST, SOFT, NORM y HARD. Mantén "
      "OCT- al golpear para fantasma y OCT+ para fuerte; después se pueden cambiar en el secuenciador. Los golpes "
      "suaves suenan también más oscuros."),
    H("Los kits"),
    R("1 a 5", "ACOUSTIC, una batería de estudio muestreada, y cuatro tratamientos: DEEP, TIGHT, BRIGHT, DUST."),
    R("6 a 37",
      "32 kits sintetizados: 808, 909, 606, TRAP, DRILL, BOOMBAP, LO-FI, HOUSE, TECHNO, JUNGLE, DEMBOW, "
      "AMAPIANO, LATIN, CHIP, JAZZ y más."),
    R("38 a 42", "USR1 a USR4 y USR3+4: tus propios kits de muestras, cargados desde el editor."),
    R("43 a 46", "SYN1 a SYN4: kits sintetizados que diseñas en el editor."),
    P("Todos los kits están igualados de volumen, y cambiar de kit nunca toca el patrón."),
    H("La pantalla DRUMS"),
    P("En TRACKS, con la batería elegida, toca EDIT o SEQ: se abre DRUMS. Tócalo otra vez para alternar entre la "
      "rejilla de pasos y la página del kit (KNOB 1 kit, KNOB 2 nivel, KNOB 3 reverb)."),
    GRID("KICK:x-----x---x-----|SNARE:----x-------x---|HAT:x-x-x-x-x-x-x-x-",
         "Un patrón de ejemplo en la rejilla: cada fila es un sonido y cada columna un paso."),
    N("SILENCIAR SONIDOS",
      "Mantén EDIT y pulsa una tecla negra: entras en modo MUTE y las teclas blancas silencian "
      "o recuperan cada sonido."),
};

constexpr Block kLayers[] = {
    P("Las capas son el corazón de SLOOP. Mantén el botón y las teclas y los KNOB cambian de trabajo. Lo que sigue "
      "es lo que hace cada una."),
    H("FX: pinchazos"),
    P("Mantén FX y mantén una tecla: el efecto actúa sobre toda la mezcla mientras dure la tecla. Los bucles y la "
      "puerta van sincronizados al tempo."),
    KEYS(kAllWhite | kAllBlack, "Con FX mantenido, cada una de las 27 teclas es un efecto."),
    R("Blancas 1 a 4", "Bucle de 1/4, 1/8, 1/16 y 1/32."),
    R("Blancas 5 a 8", "Tartamudeo en tresillos, marcha atrás, parada de cinta, media velocidad."),
    R("Blancas 9 a 12", "Barrido paso bajo, barrido paso alto, teléfono, bit crush."),
    R("Blancas 13 a 16", "Alias, puerta en 1/16, eco con puntillo, bamboleo de cinta."),
    R("Negras 1 a 3", "KILL LOW, KILL MID, KILL HIGH: quitan graves, medios o agudos, como en una mesa de DJ."),
    R("Negras 4 y 5", "REWIND (el disco tirado hacia atrás) y OCT UP."),
    R("Negras 6 a 8", "FLANGER, AUTO-PAN y FREEZE (la mezcla congelada en la reverb)."),
    R("Negras 9 a 11", "Bucle en tresillo de corchea, MONO y FADE OUT en dos tiempos."),
    R("FX + KNOB 1 a 4",
      "Filtro maestro, DUST (sampler viejo y vinilo), DUCK (bombeo desde el bombo) y filtro de la "
      "pista elegida."),
    R("FX + OCT- / OCT+", "REV THROW y ECHO THROW: los envíos de reverb o de eco a tope mientras lo mantienes."),
    H("EDIT: borrar"),
    R("EDIT + tecla", "Borra ese sonido o esa nota del patrón."),
    R("EDIT + KNOB 1 a 3", "SHIFT (desplaza el patrón), LENGTH (doble o mitad) y TRANSPOSE."),
    R("EDIT + OCT- / OCT+", "Deshacer y rehacer. Un nivel: la última grabación, borrado o edición."),
    H("ARP: redobles"),
    R("ARP + tecla",
      "Repite la nota mientras la mantienes. KNOB 1 es la velocidad. Grabando, quedan como redobles "
      "dentro del paso."),
    H("GLO: mezcla"),
    R("GLO + teclas 1 a 4", "Silenciar cada pista."),
    R("GLO + teclas 5 a 8", "Solo de cada pista."),
    R("GLO + tecla 9 y 10", "Relleno, y relleno de un compás."),
    R("GLO + tecla 16", "Marcar el tempo a golpes."),
    R("GLO + KNOB 1 a 4", "El nivel de las pistas 1 a 4."),
    N("SIN NOTAS", "Las teclas que pulses con FX, SEQ o GLO mantenidos nunca suenan como notas ni se graban."),
};

constexpr Block kSteps[] = {
    H("SEQ: los pasos"),
    P("Mantén SEQ y las 16 teclas blancas son 16 pasos de la pista elegida. Las cuatro primeras negras, u OCT- y "
      "OCT+, cambian de página de pasos. Sirve para corregir lo grabado o escribir a mano."),
    R("SEQ + KNOB 1", "Qué sonido (batería) o qué nota (sinte) escribes."),
    R("SEQ + KNOB 2", "DIV: la división de la pista."),
    R("SEQ + KNOB 3", "SWING de la pista."),
    R("SEQ + KNOB 4", "LENGTH: la duración del patrón."),
    H("Un paso, a fondo"),
    P("Con SEQ mantenido, mantén también la tecla de un paso y gira:"),
    R("KNOB 1", "Batería: qué sonido. Sinte: la nota."),
    R("KNOB 2", "Batería: nivel del golpe. Sinte: la fuerza."),
    R("KNOB 3", "RATCHET: el paso se repite de 1 a 4 veces dentro de sí."),
    R("KNOB 4", "NUDGE: adelanta o atrasa el paso respecto a la rejilla."),
    R("PRESETS o ALGORITHM", "Bloquean un sonido o un parámetro solo para ese paso."),
    R("OCT+", "Condición de relleno: el paso solo suena con el relleno."),
    R("OCT-", "Limpia el desplazamiento, los bloqueos y la condición."),
    H("Tonalidad y acordes"),
    P("Mantén SEL y pulsa una tecla: esa es la tonalidad de la canción. Con SEL mantenido, los KNOB eligen CHORD, "
      "SCALE, KEYS y TRANSPOSE."),
    R("KEYS OFF", "Todas las teclas, cromático. Es lo normal."),
    R("KEYS SNAP", "Todas las teclas, redondeadas a la escala."),
    R("KEYS WHITE", "Las blancas recorren la escala desde la tecla 5 (Do4); las negras callan."),
    R("CHORD activado", "Cada blanca es un acorde de la escala. La tecla 5 es el acorde I."),
    H("CHORD+: las negras cambian el acorde"),
    KEYS(black(1) | black(2) | black(3) | black(4) | black(5),
         "Con CHORD activado, mantén una de las cinco primeras negras mientras tocas una blanca."),
    R("Negra 1 (Fa#)", "Mayor por menor, o al revés."),
    R("Negra 2 (Sol#)", "Añade la séptima."),
    R("Negra 3 (La#)", "Sus4."),
    R("Negra 4 (Do#)", "Añade la novena."),
    R("Negra 5 (Re#)", "Inversión."),
    N("NEGRAS MUDAS",
      "Si en un sinte las teclas negras no suenan, es que está tocando acordes o escala. SEL, CHORD "
      "OFF y KEYS OFF lo devuelven a cromático."),
};

constexpr Block kSong[] = {
    P("La FM-1 guarda una canción: cuatro secciones, A a D, y el orden en que suenan. Una sección es un bucle "
      "completo: las cuatro pistas con sus sonidos, patrones y kit, y el tempo y la tonalidad. Lo que suena mientras "
      "trabajas es tu bucle de trabajo, aparte de las cuatro."),
    FLOW("BUCLE>SAVE 5-8>SECCION A-D|SECCION>SAVE 1-4>SUENA",
         "Guardar copia el bucle en una sección; tocarla la pone a "
         "sonar en el compás siguiente."),
    H("La capa SAVE"),
    KEYS(white(1) | white(2) | white(3) | white(4) | white(5) | white(6) | white(7) | white(8) | white(13) | white(14) |
             white(16),
         "Con SAVE mantenido: teclas 1 a 4 tocan, 5 a 8 guardan, 13, 14 y 16 son la canción."),
    R("SAVE + teclas 1 a 4", "Toca la sección A, B, C o D. Sonando, entra en el compás siguiente."),
    R("Varias, sin soltar SAVE",
      "Cadena rápida: A B B C, hasta ocho, con repeticiones. Al soltar suenan en orden, en "
      "bucle."),
    R("SAVE + teclas 5 a 8", "Guarda el bucle en A, B, C o D. Sobre una sección usada, pulsa otra vez antes de 3 s."),
    R("SAVE + tecla 13", "Alterna entre modo bucle y modo canción."),
    R("SAVE + tecla 14", "SONG REC: escribe en la canción cada sección que toques y cuánto dura."),
    R("SAVE + tecla 16", "La pantalla SONG."),
    H("Hacer una canción tocando"),
    S("1",
      "Haz un bucle (la estrofa) y guárdalo: SAVE + tecla 5. Cámbialo (el estribillo) y guárdalo en B, un puente "
      "en C y un final en D."),
    S("2", "Tócalas en directo con SAVE + teclas 1 a 4."),
    S("3", "Graba la estructura mientras la tocas: SAVE + tecla 14 para empezar y otra vez, o parar, para acabar."),
    S("4", "SAVE + tecla 13 pasa a modo canción: PLAY la toca entera y para al final."),
    H("Deshacer, borrar, guardar"),
    R("Deshacer / rehacer", "EDIT + OCT- / OCT+."),
    R("Borrar la pista", "REC mantenido 2 s."),
    R("Borrar el patrón", "SAVE, TOOLS, CLRSQ."),
    R("Bucle nuevo", "SAVE, TOOLS, NEW: las cuatro pistas vacías a 90 BPM. Las secciones guardadas no se tocan."),
    R("Guardar un sonido", "SAVE, USER: elige SLOT (1 a 32) y SAVE."),
    N("VALORES GO",
      "LOAD, SAVE, ERASE, NEW, CLRSQ e INIT piden confirmación: gira un clic, aparece AGAIN, y gira otro "
      "clic antes de segundo y medio."),
    P("El autoguardado es automático: con la música parada y sin tocar nada un par de segundos, el trabajo queda en "
      "memoria. Al encender, SLOOP vuelve exactamente a donde lo dejaste."),
};

constexpr Block kSounds[] = {
    P("PRESETS recorre los 153 sonidos por familias (bajos, teclas, órganos, pads, solistas, punteados y campanas, "
      "acordes, efectos) con la familia junto al nombre. Después vienen tus 32 presets de usuario. Todos están "
      "igualados de volumen."),
    N("FAVORITOS",
      "En SAVE, PRESETS, gira KNOB 4 a la derecha: el sonido pasa a favoritos, hasta 16, que salen los "
      "primeros en la lista."),
    H("Los doce motores"),
    P("Cada sonido es un parche completo sobre un motor. Sus ocho parámetros están en EDIT 1 y EDIT 2 (toca EDIT una "
      "y dos veces)."),
    R("ANALOG", "Analógico virtual: dos osciladores y filtro. 8 voces."),
    R("DIGITAL", "FM de 4 operadores, 8 algoritmos. 8 voces."),
    R("PHASE", "Distorsión de fase, al estilo Casio CZ. 8 voces."),
    R("LOFI", "Chip de 8 bits: pulso, triángulo, ruido, arpegio de chip. 8 voces."),
    R("SAMPLE", "Sampler: piano, bajo, vibráfono, metales, cuerdas, flauta y tus muestras USR1 a USR4. 8 voces."),
    R("VOICE", "Voz por formantes: coros, talkbox. 4 voces."),
    R("TRIO", "Tres osciladores con filtro multimodo. 8 voces."),
    R("WHEEL", "Órgano de ruedas tonales con percusión y altavoz rotatorio. 8 voces."),
    R("GRAIN", "Granular sobre cualquier muestra. 3 voces."),
    R("FM6", "FM de seis operadores con parches de DX7. 6 voces."),
    R("PHYS", "Modelos físicos: cuerdas, barras, membranas. 3 voces."),
    R("NOISE", "Ruido, del analógico al digital. 8 voces."),
    P("Las tres pistas de sinte se reparten un fondo común de 8 voces. Si una parte necesita una voz y no queda, toma "
      "la más antigua que esté sonando."),
    H("Parches de DX7"),
    P("FM6 es el núcleo de síntesis de Dexed. Los ocho valores de EDIT son macros sobre el parche. Para editar un "
      "parche entero, o importar un archivo SysEx de DX7 (una voz o un banco de 32), usa el panel FM6 del editor "
      "web. Miles de parches de DX7 suenan en la FM-1."),
    H("Efectos por pista y maestros"),
    R("FX, páginas", "FX, FILTER, SLICER, DLY y REV/CHO; INSERT e INSERT 2 para el efecto propio de la pista."),
    R("Maestros", "DUST, DUCK, FILT y ROLL sobre toda la mezcla, en GLO, MASTER."),
    R("GLO, TEXTURE", "Una cama de vinilo, cinta o lluvia bajo la mezcla."),
};

constexpr Block kMore[] = {
    H("MIDI"),
    P("Un teclado en el canal 1, 2 o 3 toca siempre la pista 1, 2 o 3. Ponlo en el canal 4 (o del 4 al 16) para tocar "
      "y grabar la pista elegida. Los canales se cambian en GLO, MIDI."),
    R("GLO, SYSTEM", "MIDI (salida), SYNC (reloj) e IN."),
    R("GLO, MIDI 2", "IN, OTHER, OUT y BEND."),
    R("MIDI LEARN", "GLO + negra 1. Para olvidar lo aprendido: HOME mantenido, SYSTEM, MIDI LEARN CLEAR."),
    H("Audio por USB"),
    P("La FM-1 aparece en el ordenador como entrada de audio. Si la grabación suena baja: menú de HOME, USB AUDIO en "
      "FULL. Si en un móvil suena distorsionada: AUDIO, USB RATE en 44.1 KHZ, y apaga y enciende. En un Mac, deja USB "
      "SERIAL en OFF."),
    H("Si algo falla"),
    R("No encuentra la FM-1",
      "Chrome o Edge, cable de datos, sin concentrador, permite MIDI y cierra todo lo demás "
      "que use MIDI."),
    R("Las notas se mueven", "SLOOP cuantiza a la división de la pista. Usa quant off en la pantalla REC, o 1/32."),
    R("Las notas se apagan",
      "El procesador está al límite. SLOOP apaga una voz cada vez, nunca el bajo ni el solista. "
      "Menos notas mantenidas o un motor más ligero."),
    R("STOP FIRST", "Ese cambio espera a que pares la música."),
    R("EMPTY SECTION", "La sección que has pedido está vacía: guarda antes un bucle en ella."),
    H("Chuleta"),
    R("Tempo", "SELECT en TRACKS, o GLO + tecla 16."),
    R("Elegir pista", "ALGORITHM."),
    R("Sonido o kit", "PRESETS."),
    R("Grabar", "REC. Mantenido 2 s: borrar la pista."),
    R("Deshacer", "EDIT + OCT-."),
    R("Fijar una capa", "Botón de la capa + HOME."),
    R("Silenciar pistas", "GLO + teclas 1 a 4."),
    R("Secciones", "SAVE + 1 a 4 tocar, 5 a 8 guardar."),
    R("Efectos en vivo", "FX + cualquier tecla."),
    R("Acordes", "SEL + KNOB 1."),
    R("Menú", "HOME mantenido."),
    R("Versión", "HOME mantenido, SYSTEM, ABOUT."),
};

constexpr Chapter kChapters[] = {
    FMGUIDE_CHAPTER("Qué es SLOOP", kWhat),
    FMGUIDE_CHAPTER("Instalar y actualizar", kInstall),
    FMGUIDE_CHAPTER("El panel: tocar y mantener", kPanelCh),
    FMGUIDE_CHAPTER("Tu primer bucle", kFirst),
    FMGUIDE_CHAPTER("Grabar", kRecord),
    FMGUIDE_CHAPTER("La batería", kDrums),
    FMGUIDE_CHAPTER("Las capas", kLayers),
    FMGUIDE_CHAPTER("Pasos, tonalidad y acordes", kSteps),
    FMGUIDE_CHAPTER("Secciones y canción", kSong),
    FMGUIDE_CHAPTER("Sonidos, motores y efectos", kSounds),
    FMGUIDE_CHAPTER("MIDI, problemas y chuleta", kMore),
};

}  // namespace

const Guide kGuideSloop = {
    "SLOOP",
    "Caja de ritmos para tocar en directo: tres sintes y una batería.",
    "V 2.8.5",
    kChapters,
    static_cast<int>(sizeof(kChapters) / sizeof(kChapters[0])),
    "03-11-2026",
    nullptr,
    0,
};

}  // namespace fmguide

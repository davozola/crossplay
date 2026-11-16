#include "GuideData.h"

// X0X 1.2.0, by Charles Vestal (charlesvestal/fm1-x0x), from its docs/MANUAL.md
// of 11 November 2026.

namespace fmguide {

namespace {

constexpr Block kWhat[] = {
    P("X0X convierte la FM-1 en una caja de ritmos de las de antes: una 909, una 808, dos 303 con generador de líneas "
      "ácidas y un reproductor de breaks, las cinco partes sonando a la vez y en estéreo. Viene ajustado para big "
      "beat: hace ruido en cuanto pulsas PLAY."),
    FLOW("909>808>303A>303B>BREAK|MEZCLA>REVERB>DELAY|COMPRESOR>SALIDA",
         "Cinco partes, cada una con su propio patrón; una reverb y un delay de cinta compartidos, y un compresor "
         "maestro que bombea con el bombo."),
    R("909",
      "Once voces: bombo, caja, timbales, aro y palmada modelados, con charles y platos muestreados. Color "
      "naranja."),
    R("808",
      "Once pistas, con los interruptores de la original: timbal o conga, aro o claves, palmada o maracas. "
      "Rojo."),
    R("303A y 303B",
      "Dos 303 idénticas, cada una con su sonido y su línea, y un generador que las escribe por ti. "
      "Verde y azul."),
    R("BREAK", "Un bucle de batería cortado en ocho trozos que se reordenan mientras suena. Violeta."),
    H("Lo que trae"),
    B("32 patrones. Cada parte puede tocar un patrón distinto, con su propia longitud (hasta 64 pasos) y velocidad."),
    B("Una canción de hasta 192 compases."),
    B("Grabación de movimientos de mando, bloqueos por paso y probabilidad."),
    B("Audio estéreo y MIDI por USB, sin controladores."),
    N("EN BETA",
      "El autor lo declara en beta: sirve para tocar a diario, pero un patrón muy cargado lleva la FM-1 al "
      "límite. Cuando pasa, X0X baja un poco la calidad para no cortar el sonido."),
    N("PROBAR ANTES", "La página del proyecto tiene un emulador: el mismo código en el navegador, con sonido."),
};

constexpr Block kInstall[] = {
    S("1", "Abre el instalador de X0X (charlesvestal.github.io/fm1-x0x/install) en Chrome o Edge."),
    S("2", "Conecta la FM-1 con un cable USB de datos."),
    S("3", "Pulsa Install y no desenchufes mientras escribe. Al acabar, la FM-1 arranca en X0X."),
    N("NO ES EL MISMO", "El instalador de la página de Felucca instala Felucca, no X0X. Cada firmware tiene el suyo."),
    H("Actualizar"),
    P("Se hace igual, y conserva tu proyecto: patrones, sonidos, canción y ajustes. Instalar otro firmware no lo "
      "conserva: al volver a X0X empiezas con un proyecto vacío. Guárdalo antes en un archivo."),
    H("Copia de seguridad"),
    P("X0X guarda un solo proyecto. Para tener más, o para conservarlo mientras pruebas otro firmware, abre la página "
      "de backup con la FM-1 conectada."),
    R("Back up",
      "Escribe un archivo .x0b con sonidos, los 32 patrones, la canción, los movimientos, el tempo y los "
      "ajustes."),
    R("Restore",
      "Para X0X, elige el archivo y pulsa Restore. La FM-1 pregunta RESTORE FROM USB?: SEL para sí, HOME "
      "para no."),
    P("La copia no incluye tus breaks: guarda sus archivos WAV y vuelve a cargarlos."),
    H("Volver al firmware original"),
    P("Al final de la misma página del instalador está Back to the stock firmware. Descarga el archivo de la FM-1 "
      "desde el enlace, elígelo y pulsa el botón. Si la FM-1 no lo acepta, usa el archivo V15 del segundo enlace. "
      "El actualizador de M-VAVE, M-UPGRADE, también sirve."),
};

constexpr Block kPanelCh[] = {
    PANEL(
        cAlgorithm | cPresets | cSelect | cOct,
        "ALGORITHM elige la parte; PRESETS, el patrón; SELECT pasa de página; OCT- y OCT+ avanzan de 16 en 16 pasos."),
    R("ALGORITHM", "La parte: 909, 808, 303A, 303B o BREAK. En una lista, cambia el valor."),
    R("PRESETS", "El patrón siguiente. En la pantalla de una parte, solo el de esa parte; en HOME, el de las cinco."),
    R("SELECT", "Página anterior o siguiente. En una lista, sube y baja."),
    R("OCT- y OCT+",
      "Los 16 pasos anteriores o siguientes. En HOME y SONG, patrones 1 a 16 o 17 a 32. En el teclado "
      "de la 303, la octava."),
    R("KNOB 1 a 4",
      "Los cuatro valores de abajo de la pantalla. Con ENV mantenido, un giro lo devuelve a su valor de "
      "fábrica."),
    H("Los botones"),
    P("Un botón que abre una pantalla abre siempre la misma; púlsalo otra vez para su página siguiente."),
    R("HOME", "El patrón: las cinco partes, qué patrón tocan y sus silencios. Otra vez: la canción."),
    R("SEQ", "Los pasos de la parte elegida, con LENGTH y RATE."),
    R("EDIT", "El sonido de la parte elegida. Las teclas siguen escribiendo pasos."),
    R("ARP", "TB-3PO, el generador de líneas (en una 303)."),
    R("LFO", "El mezclador y el máster."),
    R("FX", "La reverb y el delay."),
    R("GLO", "Los ajustes."),
    R("SEL", "Convierte la pantalla en una lista con todo lo que tiene, y más. En una pregunta, es el sí."),
    R("SAVE", "Guarda todo. Mantenido: copia el patrón."),
    R("PLAY y REC", "Arrancar y parar; grabar."),
    N("QUE HACE ESTE BOTON",
      "Mantén cualquier botón un segundo, solo: una tarjeta explica qué hace en esa pantalla y "
      "sus combinaciones. Al soltar no pasa nada."),
    H("Las teclas"),
    KEYS(kAllWhite | kAllBlack,
         "Las 16 blancas son los pasos (en HOME, los patrones). Las 11 negras eligen la pista "
         "de batería, tocan los trozos del break o silencian partes."),
    H("Con HOME mantenido"),
    R("HOME + REC / PLAY", "Deshacer y rehacer. Hasta 32 pasos atrás."),
    R("HOME + SELECT", "El tempo."),
    R("HOME + SEQ, a golpes", "Marcar el tempo. Desde el tercer golpe."),
    R("HOME + tecla blanca", "Elegir patrón desde cualquier pantalla."),
    R("HOME + tecla negra", "Silenciar una pista de batería, o un canal en el mezclador."),
    H("La pantalla"),
    P("La línea de arriba dice la parte (en su color), el patrón que suena (P3; «P3 >5» si el 5 viene después), el "
      "tempo y si está sonando o grabando. Un punto gris pequeño avisa de cambios sin guardar. El pie dice qué hacen "
      "las teclas en esa pantalla."),
    N("LISTAS",
      "SEL muestra todos los mandos de la pantalla agrupados y, al final, MORE: los ajustes que no están en "
      "ninguna página. En una lista, SELECT mueve, ALGORITHM cambia y HOME vuelve."),
};

constexpr Block kFirst[] = {
    P("De encender a guardar tu primer ritmo. Al arrancar, la pantalla muestra la 909 y todas las partes están "
      "vacías."),
    S("1", "Un bombo. Pulsa la tecla negra 1 para elegirlo (lo oirás) y luego las blancas 1, 5, 9 y 13."),
    S("2", "Pulsa PLAY. El bombo suena en cada tiempo."),
    S("3", "Una palmada. Tecla negra 7, y blancas 5 y 13. Con el patrón sonando, elegir un tambor es silencioso."),
    S("4", "Charles abiertos. Tecla negra 9, y blancas 3, 7, 11 y 15."),
    GRID("BD:x---x---x---x---|CP:----x-------x---|OH:--x---x---x---x-",
         "Lo que llevas hasta aquí: bombo en los cuatro tiempos, palmada en el dos y el cuatro, charles abierto a "
         "contratiempo."),
    S("5",
      "Una línea de 303. Gira ALGORITHM dos clics hasta 303A y pulsa ARP. Pulsa OCT+: cada pulsación escribe "
      "una línea nueva. Sigue hasta que te guste una."),
    S("6", "Dale forma. Pulsa EDIT: KNOB 1 abre y cierra el filtro y KNOB 2 es la resonancia."),
    S("7",
      "El break. Gira ALGORITHM dos clics más hasta BREAK y pulsa las 16 teclas blancas, para que suene todo el "
      "compás."),
    S("8", "Silencia una parte. Pulsa HOME y la tecla negra 5: el break calla. Otra vez, y vuelve."),
    S("9",
      "Graba un barrido. ALGORITHM a 303A, EDIT, pulsa REC, gira KNOB 1 despacio un compás o dos y pulsa REC otra "
      "vez. El barrido se repite en cada vuelta."),
    S("10", "Guarda. PLAY para parar y luego SAVE."),
    N("SE GUARDA SOLO",
      "Con AUTOSAVE (activado de fábrica), X0X guarda por su cuenta cuando el patrón está parado y "
      "llevas cuatro segundos sin tocar nada. Nunca guarda mientras suena."),
};

constexpr Block kDrums[] = {
    P("Cada caja tiene once pistas, una en cada tecla negra."),
    KEYS(kAllBlack, "Las once teclas negras, numeradas, son las once pistas de batería."),
    R("Negra 1 y 2", "Bombo (BD) y caja (SD), en las dos."),
    R("Negras 3 a 5", "Timbal grave, medio y agudo. En la 808 pueden ser congas."),
    R("Negra 6", "Aro (RS). En la 808, aro o claves."),
    R("Negra 7", "Palmada (CP). En la 808, palmada o maracas."),
    R("Negra 8", "909: charles cerrado (CH). 808: cencerro (CB)."),
    R("Negra 9", "909: charles abierto (OH). 808: plato (CY)."),
    R("Negra 10", "909: crash (CR). 808: charles abierto."),
    R("Negra 11", "909: ride (RD). 808: charles cerrado."),
    P("Los charles y platos de la 909 son muestras, como en la máquina original; todo lo demás, en las dos, es "
      "sintetizado. El charles cerrado corta al abierto."),
    H("Escribir un ritmo"),
    S("1", "Pulsa una tecla negra para elegir pista. Parado, la oyes; sonando, no."),
    S("2", "Pulsa teclas blancas: cada una enciende su paso, y un toque sobre un paso encendido lo apaga."),
    S("3",
      "Para acentos, mantén ENV mientras pulsas las blancas. Un acento hace más fuerte a todos los tambores de "
      "ese paso."),
    P("Un patrón puede tener hasta 64 pasos. Las blancas enseñan 16 cada vez: OCT+ pasa a los 16 siguientes y OCT- "
      "vuelve."),
    H("Silenciar"),
    P("Una pista: mantén HOME y pulsa su tecla negra. Su fila se pone gris. Estos silencios no se guardan. La parte "
      "entera se silencia en HOME."),
    H("Las páginas"),
    R("EDIT", "El sonido de la pista elegida, y después KIT, donde ACCENT fija cuánto sube un paso acentuado."),
    R("SEQ", "LENGTH y RATE de la caja."),
    R("SEL, MORE", "DRIVE y DIST de cada pista."),
    R("LFO, 909 MIX / 808 MIX",
      "Las once pistas a la vez: las negras eligen una y los mandos ponen LEVEL, PAN, REV y "
      "DLY."),
    N("SWING", "Está en HOME, porque afecta a todas las partes. 50 % es recto y 75 % lo máximo."),
};

constexpr Block k303[] = {
    P("Hay dos 303 idénticas. Cada una tiene su sonido y su línea."),
    H("Los pasos"),
    P("Una tecla blanca enciende su paso; otro toque lo apaga. Para editarlo, pulsa SEQ, mantén la tecla del paso y "
      "gira:"),
    R("KNOB 1", "La nota. OCT- y OCT+ la mueven una octava."),
    R("KNOB 2", "REST (silencio), NOTE o TIE (ligadura)."),
    R("KNOB 3", "El acento."),
    R("KNOB 4", "El slide: desliza de la nota de este paso a la del siguiente."),
    P("Una ligadura sujeta la nota anterior durante el paso, sin volver a atacarla. Un paso vacío se enciende al "
      "pulsarlo, así que puedes pulsar, sujetar y poner la nota de una vez."),
    H("El teclado"),
    P("Pulsa REC y las 27 teclas se convierten en un teclado (arriba pone KEYS). Lo que toques se escribe: paso a "
      "paso si está parado, en vivo si suena. Si tocas una tecla sin soltar otra, la 303 desliza entre las dos."),
    R("Acento al grabar", "Mantén ENV mientras pulsas la tecla."),
    R("Slide al grabar", "Mantén LFO mientras pulsas la tecla."),
    R("Silencio", "ENV mantenido y OCT- u OCT+."),
    R("Ligadura", "LFO mantenido y OCT- u OCT+."),
    N("TOCAR SIN ESCRIBIR",
      "SEQ, gira SELECT hasta la página KEYS y pon FREE PLAY en ON. Las teclas tocan la 303 y "
      "nada se escribe hasta que pulses REC."),
    H("El sonido"),
    R("EDIT, TONE", "CUTOFF, RESO, ENVMOD y DECAY."),
    R("EDIT, VOICE", "ACCENT, WAVE (sierra o cuadrada), DRIVE y tiempo de SLIDE."),
    R("SEL, lista", "TUNE, VOLUME, DRIVE TYPE (apagado, suave o RAT) y ACCENT DECAY."),
    R("SEQ", "LENGTH (1 a 64), RATE, DIRECTION (adelante, atrás, ida y vuelta o al azar) y TRANSPOSE."),
    H("TB-3PO: que escriba ella"),
    PANEL(cArp | cOct | cAlgorithm,
          "Elige una 303 con ALGORITHM y pulsa ARP. OCT+ escribe una línea nueva; OCT- muta "
          "la actual."),
    P("Una mutación cambia más o menos un cuarto de los pasos. TB-3PO tiene dos páginas; ARP otra vez, o SELECT, "
      "cambia entre ellas."),
    R("GENERATE",
      "DENS, ACCENT y SLIDE: cuántos pasos suenan, se acentúan y deslizan. OCTS: el rango, de 1 a 3 "
      "octavas."),
    R("SCALE",
      "ROOT, SCALE (menor, frigia, menor armónica, pentatónica menor, dórica o mayor) y OCTAVE. MUTATE la "
      "hace mutar sola cada 1 a 16 compases."),
    P("Estos mandos no cambian la línea que estás oyendo: preparan la siguiente pulsación de OCT+ u OCT-. Tu línea, "
      "retoques a mano incluidos, se queda como está hasta entonces."),
};

constexpr Block kBreak[] = {
    P("El break es un bucle de batería cortado en ocho trozos, y X0X los reordena mientras suena."),
    FLOW("BUCLE>8 TROZOS>GENERADOR>PASOS",
         "El generador decide qué trozo suena en cada paso; tú decides en qué pasos "
         "suena y cuánto se desordena."),
    H("Los pasos"),
    P("Las teclas blancas son los 16 pasos del compás. Pulsa una para que el break suene ahí; otro toque lo quita."),
    H("Tocar trozos a mano"),
    KEYS(black(1) | black(2) | black(3) | black(4) | black(5) | black(6) | black(7) | black(8),
         "Las negras 1 a 8 tocan los ocho trozos. Las tres últimas son efectos."),
    R("Negras 1 a 8", "Mientras las sujetas, suena ese trozo. Al soltar, vuelve el generador."),
    R("Negra 9", "Marcha atrás."),
    R("Negra 10", "Media velocidad."),
    R("Negra 11", "Tartamudeo."),
    R("Blanca + negra 1 a 8",
      "Ese paso tocará siempre ese trozo. Así pones bombo y caja donde quieres y dejas el "
      "resto al generador. Repite para devolverlo."),
    H("El generador"),
    P("En SEQ, páginas GROOVE y RETRIG:"),
    R("COMPLEXITY", "Cada cuánto salta a otro trozo en vez de tocar el siguiente. A 0, el bucle suena tal cual."),
    R("ANCHOR", "Con cuánta fuerza mantiene bombo y caja en los tiempos 1 y 3."),
    R("ROLL", "Cada cuánto repite un trozo o pasa al de al lado."),
    R("FILL", "Cuánto rompe las reglas el último compás de una frase."),
    R("RETRIG 2X a 8X", "La probabilidad, en cada compás, de tartamudear un tiempo a esa velocidad."),
    H("Los bucles"),
    R("EDIT",
      "LOOP A, LOOP B, B CHANCE (la probabilidad de pasar al bucle B en el último compás de la frase) y "
      "PITCH."),
    R("EDIT, otra vez",
      "FILTER y LEVEL. FILTER es un solo mando: apagado en el centro, paso bajo a la izquierda, paso "
      "alto a la derecha."),
    P("Un paso alto se queda con los charles del bucle y deja el bombo y la caja a la 909 o la 808. Los bucles siguen "
      "el tempo tocando más rápido o más lento, así que su tono cambia con él, como en un sampler."),
    H("Tus propios breaks"),
    P("X0X no trae breaks grabados, solo dos bucles tocados por su propia 909. Los tuyos se cargan desde la página de "
      "break loops con la FM-1 conectada: arrastras los archivos y pulsas Upload. Caben unos 21 segundos en total, "
      "alrededor de nueve bucles de un compás. Aparecen como BR1, BR2..."),
};

constexpr Block kPatterns[] = {
    P("Hay 32 patrones. Un patrón guarda los pasos, la longitud y la velocidad de cada parte, los ajustes del break, "
      "el swing y los movimientos. Los sonidos, los efectos y el máster no forman parte del patrón: no cambian con "
      "él."),
    P("Cada parte toca su propio patrón: las 303 pueden ir por el 3 mientras la batería va por el 7."),
    H("Elegir"),
    R("Las cinco partes", "En HOME, una tecla blanca o PRESETS. Desde otra pantalla, HOME + tecla blanca."),
    R("Una parte", "PRESETS en la pantalla de esa parte."),
    R("Una cadena",
      "En HOME, mantén dos teclas blancas a la vez: suenan del primero al segundo, un compás cada uno, y "
      "vuelta a empezar."),
    P("Sonando, el patrón nuevo entra al acabar el compás. Parado, entra al momento."),
    H("Copiar y borrar"),
    R("Copiar",
      "Mantén SAVE (dice P1 COPIED) y pulsa una tecla blanca para pegar ahí. Si el destino tiene notas, "
      "pregunta."),
    R("Borrar una parte", "SAVE mantenido y REC."),
    R("Borrar las cinco", "GLO, CLEAR PATTERN."),
    H("Longitudes"),
    P("Las partes pueden tener longitudes distintas y desfasarse entre sí. La longitud de la 909 marca la del compás, "
      "y su patrón guarda el swing de todas."),
    H("Grabar"),
    PANEL(cRec | cEnv | cLfo, "REC enciende y apaga la grabación. ENV mantenido da acentos; LFO mantenido, slides."),
    R("Batería", "Pulsa teclas negras mientras suena. ENV para golpes acentuados."),
    R("303 en vivo", "Toca sus teclas mientras suena."),
    R("303 paso a paso", "REC con el patrón parado: cada tecla llena el paso siguiente."),
    R("Mandos", "REC, PLAY y gira cualquier mando de sonido."),
    R("La canción", "REC en la pantalla SONG, y PLAY."),
    H("La canción"),
    P("Una canción es una lista de hasta 192 compases. Cada compás dice qué patrón toca cada parte y cuáles están "
      "silenciadas. Se abre pulsando HOME dos veces."),
    R("Escribir compases", "Una tecla blanca: el compás toma ese patrón en las cinco partes y se pasa al siguiente."),
    R("Editar un compás", "KNOB 1 lo elige; los otros mandos ponen el patrón de cada parte."),
    R("Silenciar en un compás", "Teclas negras 1 a 5."),
    R("Repetir", "En la segunda página, REPEAT: hasta 8 veces."),
    R("Tocarla", "PLAY en la pantalla SONG. Empieza en el compás elegido y da vueltas."),
    R("Grabarla", "REC en SONG y PLAY: cambia patrones y silencia partes, y cada compás se escribe al pasar."),
    N("S O P",
      "PLAY desde SONG toca la canción; desde cualquier otra pantalla, los patrones. La línea de arriba dice "
      "cuál: S o P."),
};

constexpr Block kMotion[] = {
    P("Tres formas de dar a un paso un valor propio. Las tres se guardan con el patrón."),
    H("Movimiento de mandos"),
    P("Enciende REC, pulsa PLAY y gira un mando de sonido. X0X graba el mando en los pasos donde lo moviste y "
      "rellena las pausas cortas, para que un barrido lento no tenga huecos. Al reproducir, desliza de paso a paso."),
    B("Un mando con movimiento lleva un punto en la esquina."),
    B("Si giras el mando sin REC, mandas tú durante una vuelta y después vuelve la grabación."),
    B("Para borrar el movimiento de un mando: mantén SAVE y gíralo."),
    B("Al parar, cada mando vuelve a su valor propio."),
    H("Bloqueos por paso"),
    PANEL(cEdit | cKnobs | cKeys,
          "En EDIT, mantén un paso encendido y gira un mando de sonido: ese paso toca el valor "
          "nuevo y los demás conservan el del mando."),
    P("Un bombo más agudo, un filtro abierto en una nota de la 303, el break subido de tono en un paso. Mientras "
      "sujetas el paso, la fila de mandos dice P-LOCK STEP 5 y un punto ámbar marca los bloqueados."),
    R("Quitar un bloqueo", "Mantén el paso, mantén SAVE y gira el mando."),
    R("En SEQ", "Sujetar un paso no bloquea nada; en una 303, los mandos ponen su nota."),
    H("Probabilidad"),
    PANEL(cSelect | cKeys, "Mantén un paso y gira SELECT: la probabilidad de que suene, del 100 % al 5 %."),
    P("Los dados se tiran cada vez que llega el paso. Si sigues girando más allá del 5 % aparecen los ciclos: 1:2, "
      "2:2, 1:3... hasta 8:8. Un paso en 1:4 suena la primera de cada cuatro vueltas; ahí no hay azar."),
    B("En la 909 y la 808, la probabilidad es del paso de la pista elegida: bombo y charles pueden tener la suya en el "
      "mismo paso."),
    B("En una 303, un paso que no sale es un silencio."),
    N("EL LIMITE",
      "Bloqueos y probabilidades se guardan como movimiento. Caben 160 entre todos los patrones; cuando "
      "se llena, arriba pone MOTION FULL."),
};

constexpr Block kMix[] = {
    H("Efectos"),
    P("Hay una reverb y un delay, y cada parte envía a los dos. FX enseña cuánto envía cada una y tiene dos páginas."),
    R("REVERB", "DECAY y TONE. Estéreo y suave."),
    R("DELAY",
      "TIME (de 1/32 a blanca con puntillo), FEEDBACK, TONE y TAPE. Con TAPE a la izquierda es un delay "
      "digital limpio; al subirlo se vuelve de cinta: bambolea, satura y se desgasta."),
    H("Mezclador y máster"),
    P("LFO abre el mezclador, con cuatro páginas."),
    R("PARTS", "Nivel y medidor de cada parte. Las negras 1 a 5 eligen una; los mandos ponen LEVEL, PAN, REV y DLY."),
    R("909 MIX y 808 MIX", "El mezclador de cada caja, pista a pista."),
    R("MASTER", "DRIVE, COMP, PUMP y FILTER sobre toda la mezcla."),
    R("COMP", "El compresor con un solo mando. A 0 está apagado."),
    R("PUMP",
      "Baja toda la mezcla cada vez que suena el bombo y la deja volver: la respiración del big beat. "
      "Funciona incluso con el bombo silenciado."),
    R("FILTER", "Apagado en el centro; paso bajo a la izquierda, paso alto a la derecha."),
    N("MEZCLA LIMPIA",
      "De fábrica viene exagerado a propósito: break delante, 303 por distorsión RAT hacia el delay "
      "de cinta. Para una mezcla limpia, pon COMP y PUMP a 0."),
    P("FILTER y PUMP están también en la página PERFORM de HOME, junto a TEMPO y SWING, para tocar en directo."),
    H("Ajustes"),
    P("GLO, en cinco secciones: MIDI, AUDIO, KEYS AND SCREEN, PROJECT y SYSTEM."),
    R("OUTPUT", "NORMAL o LOUD (6 dB más en auriculares, línea y USB)."),
    R("USB AUDIO", "FOLLOWS KNOB o FULL (a nivel completo aunque bajes MASTER)."),
    R("KEY SOUND", "STOPPED: elegir un tambor es silencioso mientras suena. ALWAYS: siempre se oye."),
    R("AUTOSAVE", "Guarda solo, parado y tras cuatro segundos de calma."),
    R("FACTORY RESET", "Sonidos de fábrica y patrones vacíos. Lo guardado se conserva hasta que guardes encima."),
    R("PERFORMANCE", "Cuánto trabaja la FM-1, parte a parte, con una prueba de carga."),
    R("2ND CORE",
      "X0X usa los dos núcleos del procesador. Si se cuelga con los dos, arranca con uno y lo deja en OFF."),
    H("MIDI y audio USB"),
    P("Por USB aparece como «X0X FM-1», dispositivo MIDI y entrada de audio estéreo a 44,1 kHz. También recibe MIDI "
      "por el minijack. Cada parte escucha en su canal:"),
    R("Canal 10", "La 909, con notas de batería General MIDI."),
    R("Canal 11", "La 808."),
    R("Canales 2 y 3", "303A y 303B. Notas solapadas deslizan; velocidad 100 o más, acento."),
    R("Canal 4", "Las notas 36 a 43 tocan los trozos 1 a 8 del break."),
    P("Sigue un reloj MIDI que entre, sin configurar nada. Puede enviar reloj (CLOCK OUT) y los patrones como notas "
      "(NOTES OUT). Los mandos responden a CC fijos, cada parte en su canal."),
    H("Si algo falla"),
    R("Pantalla roja", "X0X se ha colgado y se reinicia solo. Si lo comunicas, apunta la dirección que muestra."),
    R("SAFE MODE",
      "Dos cuelgues seguidos al arrancar. No suena, pero el instalador web llega: reinstala. PLAY intenta "
      "arrancar normal."),
    R("Un botón hace otra cosa", "Calibra: OCT- y OCT+ mantenidos al encender."),
    R("Modo actualización", "OCT- y OCT+ juntos durante cinco segundos."),
    R("MEMORY FULL", "El proyecto no cabe y no se ha guardado: borra patrones que no uses."),
    H("Chuleta"),
    R("Parte", "ALGORITHM."),
    R("Patrón", "PRESETS, o tecla blanca en HOME."),
    R("Tempo", "HOME + SELECT, o HOME + SEQ a golpes."),
    R("Acento / slide", "ENV / LFO mantenidos."),
    R("Deshacer", "HOME + REC."),
    R("Copiar patrón", "SAVE mantenido y tecla blanca."),
    R("Silenciar parte", "En HOME, negras 1 a 5."),
    R("Línea ácida nueva", "ARP y OCT+."),
    R("Lista completa", "SEL."),
    R("Sí / no", "SEL / HOME."),
};

constexpr Chapter kChapters[] = {
    FMGUIDE_CHAPTER("Qué es X0X", kWhat),
    FMGUIDE_CHAPTER("Instalar, guardar y volver", kInstall),
    FMGUIDE_CHAPTER("El panel y la pantalla", kPanelCh),
    FMGUIDE_CHAPTER("Tu primer ritmo", kFirst),
    FMGUIDE_CHAPTER("La 909 y la 808", kDrums),
    FMGUIDE_CHAPTER("Las 303 y TB-3PO", k303),
    FMGUIDE_CHAPTER("El break", kBreak),
    FMGUIDE_CHAPTER("Patrones, grabar y canción", kPatterns),
    FMGUIDE_CHAPTER("Movimiento y probabilidad", kMotion),
    FMGUIDE_CHAPTER("Mezcla, ajustes y chuleta", kMix),
};

}  // namespace

const Guide kGuideX0x = {
    "X0X",     "Una 909, una 808, dos 303 y breaks, todo a la vez.",       "V 1.2.0",
    kChapters, static_cast<int>(sizeof(kChapters) / sizeof(kChapters[0])),
};

}  // namespace fmguide

#include "GuideData.h"

// SHRUB V1.39, by Steve Duda / Xfer Records (shrub.audio), from its Operation
// Guide 1.0 as read on 27 November 2026.

namespace fmguide {

namespace {

constexpr Block kWhat[] = {
    P("SHRUB convierte la FM-1 en un sintetizador de tablas de ondas: una versión reducida de Serum, el plugin más "
      "usado para diseñar sonido electrónico, hecha por su propio autor, Steve Duda, de Xfer Records. Deja de ser "
      "un sinte FM: suena a Serum, con sus osciladores, sus deformaciones de onda, sus filtros y sus efectos."),
    P("Viene en dos partes. El firmware, que convierte la FM-1 en el sinte. Y un editor en el navegador que enseña "
      "todo a la vez, ordena la biblioteca, importa presets de Serum 1 y hasta toca SHRUB sin la FM-1 conectada."),
    H("La cadena del sonido"),
    FLOW("OSC A>FILTRO>ENV 1>EFECTOS>SALIDA|OSC B>FILTRO|SUB>FILTRO|RUIDO>FILTRO",
         "Dos osciladores de tablas, un suboscilador y ruido entran en un único filtro. Después, la envolvente de "
         "volumen de cada nota y una cadena de hasta siete efectos."),
    H("Lo que trae"),
    B("Dos osciladores de tablas de ondas, con 72 tablas de fábrica, tablas propias y un modo aditivo, y hasta "
      "ocho voces de unísono cada uno."),
    B("Las deformaciones de onda de Serum (sync, bend, PWM, FM, AM, RM, espejo...) y 26 curvas de remapeo."),
    B("Un filtro con 76 tipos: los de Serum 1 y los de Serum 2, escaleras, peines, phasers, filtros dobles."),
    B("Tres envolventes, cuatro LFO que se pueden dibujar, dos generadores de caos y una matriz de 16 conexiones."),
    B("Siete huecos de efectos con once módulos: distorsión, chorus, delay, reverb, compresor con modo OTT, EQ..."),
    B("Escalas y acordes, un arpegiador de 19 patrones y un secuenciador que graba el movimiento de los mandos."),
    B("100 presets de fábrica en siete categorías, y 28 huecos libres para los tuyos."),
    H("Lo que conviene saber antes"),
    R("Hasta 8 notas", "A la vez, sacadas de 16 voces. Un unísono gordo deja menos notas, pero siempre suena una."),
    R("Un solo filtro",
      "Todas las notas pasan por el mismo filtro: un acorde se filtra junto. Una nota sola suena como en "
      "Serum. El manual lo llama parafónico."),
    R("Teclas sin fuerza",
      "Las 27 teclas tocan siempre a fuerza 100. La fuerza y la presión llegan por MIDI desde otro "
      "teclado."),
    R("En beta", "Se actualiza a menudo. Esta guía sigue la V1.39 y su manual 1.0, de noviembre de 2026."),
    H("Datos"),
    R("Autor", "Steve Duda, Xfer Records. Gratis, de código cerrado."),
    R("Web", "shrub.audio: instalador, editor, emulador para probarlo en el navegador y manual."),
    R("Comunidad", "El Discord «Shrub Club», enlazado desde la web."),
    N("NO ES DE M-VAVE",
      "Ni lo hace ni lo mantiene M-VAVE. Se instala bajo tu responsabilidad, aunque el instalador solo "
      "escribe la parte que toca una actualización y siempre se puede volver al firmware original."),
};

constexpr Block kInstall[] = {
    P("Se instala por USB desde el navegador, sin programas ni controladores. Tarda unos dos minutos. Al actualizar "
      "SHRUB a una versión nueva, tus presets y tus tablas de ondas se quedan."),
    H("Lo que necesitas"),
    B("Chrome o Edge en un ordenador. Safari y Firefox no tienen el MIDI web que usa el instalador."),
    B("Un cable USB de datos. Si el instalador no ve la FM-1, prueba otro cable u otro puerto."),
    B("Nada más usando la FM-1: cierra el editor de SHRUB, el actualizador de M-VAVE y cualquier programa de "
      "música. En Windows, el puerto MIDI solo lo puede usar un programa a la vez."),
    H("Instalar"),
    S("1", "Conecta la FM-1 por USB y enciéndela."),
    S("2", "Abre shrub.audio/editor/install en Chrome o Edge."),
    S("3",
      "Elige «Use the SHRUB on this site» para la última versión, o «Choose package (.fwsc)» si tienes el "
      "archivo de una versión concreta."),
    S("4", "Pulsa Install y permite el acceso MIDI cuando el navegador lo pida."),
    S("5",
      "Espera unos dos minutos sin tocar el cable. La FM-1 enseña SHRUB UPDATE mientras trabaja y se reinicia al "
      "acabar."),
    N("VIENES DE OTRO",
      "Si la FM-1 lleva un firmware que el instalador no reconoce, como uno de la comunidad, vuelve "
      "primero al de M-VAVE y luego instala SHRUB."),
    H("Si algo sale mal"),
    R("Se corta a mitad",
      "La FM-1 se queda en modo actualización, aunque la apagues y enciendas. Lanza el instalador otra vez "
      "y continúa."),
    R("No llegó a escribir",
      "Si no se había escrito nada, la FM-1 vuelve sola a su firmware anterior en un minuto, más o "
      "menos."),
    R("No la encuentra", "Cierra los demás programas con MIDI, cambia de cable o de puerto USB."),
    R("Editor y firmware no casan",
      "Si el editor avisa de que no coinciden, actualiza el firmware desde el instalador o recarga la "
      "página del editor."),
    H("Volver al firmware de M-VAVE"),
    P("Descarga el paquete oficial de la web de M-VAVE y pásalo con su actualizador, como cualquier actualización. "
      "Antes, guarda tu biblioteca desde el editor: LIBRARY, Get from FM-1, y luego un archivo .shrubbank."),
    H("Rescate"),
    PANEL(cOct, "OCT- y OCT+ mantenidos al encender: el modo de rescate por USB."),
    S("1", "Conecta el cable USB."),
    S("2", "Mantén OCT- y OCT+ mientras enciendes, y sigue un segundo más."),
    S("3",
      "La pantalla dice KEEP HOLDING OCT- AND OCT+ FOR USB RECOVERY y luego USB RECOVERY. Lanza el instalador "
      "otra vez, o el de M-VAVE."),
    H("Si se cuelga"),
    R("Se queda congelado", "Un vigilante lo reinicia a los 8 segundos. En GLOBAL aparecerá WATCHDOG RESET."),
    R("Pantalla roja con dibujo",
      "Es un informe del fallo. Se reinicia solo a los 10 segundos. Hazle una foto y mándala: es lo que "
      "pide el autor."),
    R("Falla al arrancar",
      "Se defiende solo. Al primer fallo seguido arranca sin tus presets; al segundo, en SAFE START, sin "
      "leer nada guardado; al tercero entra solo en rescate USB."),
    R("CORE 2 FROZE",
      "El segundo núcleo del chip se ha atascado. Pon CPU MHZ a 320 en GLOBAL, página 5: toca un poco "
      "menos de notas y suele arreglarlo."),
    R("GLO al encender",
      "Mantener GLO mientras enciendes apaga el segundo núcleo, o lo vuelve a encender. Es otra salida si "
      "el problema viene de ahí."),
};

constexpr Block kMove[] = {
    P("SHRUB ordena el sonido en secciones, y cada sección en partes con varias páginas de cuatro mandos. Un botón "
      "abre su sección, pulsarlo otra vez pasa a la parte siguiente, SELECT pasa las páginas y los cuatro KNOB "
      "cambian lo que la pantalla enseña."),
    PANEL(cMaster | cPresets | cSelect | cAlgorithm | cKnobs,
          "Los mandos: MASTER, PRESETS, SELECT y ALGORITHM a la izquierda; los cuatro KNOB bajo la pantalla."),
    H("Los mandos de la izquierda"),
    R("MASTER", "El volumen."),
    R("PRESETS", "Recorre los presets. El sonido se carga cuando dejas de girar."),
    R("SELECT", "Pasa las páginas de la parte. En SAVE mueve el cursor del nombre; en SEQ, elige la pista."),
    R("ALGORITHM",
      "El tipo o el modo de la parte: la tabla de un oscilador, el tipo de filtro, el patrón del "
      "arpegio. En HOME salta de categoría."),
    R("KNOB 1 a 4", "Los cuatro valores de la página que se ve."),
    H("Los botones"),
    PANEL(cFx | cSel | cEnv | cLfo | cEdit | cGlo | cHome | cSave | cArp | cSeq | cPlay | cRec,
          "Las dos filas de botones. El que lleva impreso SEL hace de SCL, las escalas."),
    R("EDIT", "Osciladores y filtro: OSC A, OSC B, SUB y FILTER, una por pulsación."),
    R("ENV", "ENV 1, ENV 2, ENV 3 y la MATRIX."),
    R("LFO", "LFO 1 a 4, CHAOS y la MATRIX."),
    R("FX", "El rack de efectos; pulsando otra vez, cada efecto del rack en su orden."),
    R("GLO", "VOICING, las voces, y GLOBAL, los ajustes de toda la FM-1."),
    R("SEL (SCL)", "SCALE: tonalidad, escala y acordes."),
    R("HOME", "La pantalla de inicio con el preset. Otra vez: cambia sus mandos por las cuatro macros."),
    R("SAVE", "Guardar. Otra vez: confirma."),
    R("ARP", "La página del arpegiador. Otra vez: lo enciende o lo apaga."),
    R("SEQ", "El secuenciador de mandos."),
    R("PLAY", "Arranca y para el arpegio, las pistas y el metrónomo. Verde tocando, rojo grabando."),
    R("REC", "Graba en el compás siguiente; si está parado, tras un compás de cuenta."),
    R("OCT- y OCT+", "Octava abajo y arriba. Los dos juntos: retener las notas (manda sustain, CC 64)."),
    N("ATAJO",
      "Mantén EDIT, ENV o LFO y gira SELECT para saltar entre sus partes sin pulsar varias veces el "
      "botón."),
    H("La pantalla"),
    R("Arriba", "El nombre de la parte, en su color de Serum; unos puntos por página; el medidor de salida."),
    R("En medio", "El dibujo de la parte: la tabla en 3D, la curva de la envolvente, el filtro o el efecto."),
    R("Abajo", "Los cuatro mandos, con su nombre y su valor."),
    R("Anillo azul", "Un mando con modulación lleva un anillo azul que marca hasta dónde lo mueve."),
    H("Las luces de las teclas"),
    P("Se encienden las teclas que suenan, las toques tú, lleguen por MIDI o las toque el arpegio. En la página SCALE "
      "enseñan la escala. Con KEY LIGHT, en GLOBAL, las apagadas brillan un poco para tocar a oscuras."),
};

constexpr Block kPresets[] = {
    P("HOME es la pantalla de inicio: el preset que suena, con su nombre y categoría, y cuatro mandos a mano. De ahí "
      "se sale a todo lo demás y ahí se vuelve."),
    PANEL(cHome | cPresets | cAlgorithm | cKnobs,
          "En HOME: PRESETS elige, ALGORITHM salta de categoría, los KNOB "
          "tocan lo básico."),
    H("Los 128 huecos"),
    R("1 a 100", "Los de fábrica, en siete categorías: BASS, LEAD, MISC, PAD, PLUCK, SEQ y SYNTH."),
    R("101 a 128", "INIT: sonidos vacíos donde guardar los tuyos."),
    R("Con una U", "Un hueco de fábrica que ya has sobrescrito lleva una U delante del número."),
    H("Los mandos de HOME"),
    R("KNOB 1 y 2", "CUTOFF y RES: el corte y la resonancia del filtro."),
    R("KNOB 3 y 4", "A POS y B POS: la posición dentro de la tabla de cada oscilador."),
    R("HOME otra vez",
      "Los cuatro KNOB pasan a ser MACRO 1 a 4. Se queda así al cambiar de preset y al apagar, hasta que "
      "vuelvas a pulsar."),
    P("Al girar una macro, la pantalla dice un momento a qué va, por ejemplo «M1: CUTOFF». Una macro sin destino dice "
      "«M3: -»: no hace nada hasta que la conectes en la matriz."),
    H("Favoritos"),
    S("1", "Mantén HOME medio segundo: el preset queda marcado con una estrella, por ejemplo PAD *."),
    S("2", "Con algún favorito, ALGORITHM en HOME pasa también por el grupo FAVORITES."),
    S("3", "Para quitarlo, mantén HOME otra vez."),
    H("No perder lo editado"),
    P("Si has cambiado algo y giras PRESETS, la pantalla dice EDITED / TURN AGAIN y no cambia de sonido. Espera un "
      "momento y gira otra vez para irte, perdiendo los cambios, o pulsa SAVE para quedártelos."),
    N("AL APAGAR",
      "SHRUB guarda solo el sonido en curso y sus pistas un par de segundos después de que dejes de "
      "tocar mandos, y lo recupera al encender. No es lo mismo que guardarlo en un hueco."),
    H("Guardar"),
    PANEL(cSave | cSelect | cAlgorithm | cKnobs, "SAVE: los mandos eligen hueco, cursor, letra y categoría."),
    S("1", "Pulsa SAVE. La pantalla enseña el hueco, el nombre y qué vas a sustituir."),
    S("2",
      "KNOB 1 elige el hueco, de 1 a 128. Si el sonido venía de fábrica, empieza en el primer INIT libre para "
      "que no pises un original sin querer."),
    S("3", "KNOB 2 (o SELECT) mueve el cursor, KNOB 3 (o ALGORITHM) cambia la letra: hasta 16 caracteres."),
    S("4", "KNOB 4 elige una de las siete categorías."),
    S("5", "Pulsa SAVE otra vez para guardar. Cualquier otro botón de sección sale sin guardar."),
    H("Lo que puede contestar"),
    R("SAVED", "Guardado."),
    R("SAVED, THINNED", "Las pistas del secuenciador tenían demasiado detalle y las ha suavizado un poco."),
    R("SAVED, NO LANES", "Las pistas no cabían: el sonido se guarda sin ellas."),
    R("AUX CUT, LFOS CUT, NO LFO 4",
      "Un sonido enorme ha perdido algo para caber: una modulación auxiliar, la forma dibujada de un LFO "
      "o el LFO 4."),
    R("NOT SAVED: STORAGE FULL",
      "La memoria está llena. Libera huecos en LIBRARY, en el editor, o sobrescribe uno viejo."),
    P("Cada hueco tiene alrededor de 1 KB para el sonido y todas sus secuencias. Si incluye pistas, puede pedir «SAVE "
      "AGAIN: SOUND + LANES»."),
    H("Tu primer sonido, desde cero"),
    S("1", "Gira PRESETS hasta un hueco INIT, del 101 en adelante: un sonido de partida, sin nada encima."),
    S("2",
      "Pulsa EDIT: OSC A. Gira ALGORITHM para elegir otra tabla y KNOB 1, WT POS, para recorrerla. Así se "
      "mueve el timbre."),
    S("3", "SELECT, página 2: sube VOICES a 3 o 4 y DETUNE un poco. Es el unísono que engorda el sonido."),
    S("4",
      "EDIT tres veces más hasta FILTER: baja CUTOFF y sube RES. Página 2, ENV 2: cuánto abre el filtro la "
      "envolvente 2."),
    S("5", "Pulsa ENV dos veces, ENV 2: un ATTACK corto, DECAY medio, SUSTAIN bajo. Ahora cada nota hace «uau»."),
    S("6",
      "Pulsa LFO: en LFO 1, KNOB 3, DEST, elige WT POS de OSC A y sube AMOUNT. La tabla se mueve sola: el "
      "anillo azul enseña cuánto."),
    S("7", "Pulsa FX y enciende un REVERB y un CHORUS en el rack."),
    S("8", "SAVE, ponle nombre y categoría, y SAVE otra vez."),
    N("EL DADO",
      "Mantén GLO medio segundo y sale el arbusto de SHRUB. Gira SELECT a la izquierda para un "
      "sonido al azar, o a la derecha para una mezcla de seis presets. Cualquier botón vuelve."),
};

constexpr Block kOsc[] = {
    P("EDIT abre los osciladores. OSC A y OSC B son iguales: leen una tabla de ondas, una fila de formas de onda que "
      "se recorre con WT POS. Mover la posición cambia el timbre poco a poco, que es la gracia de la síntesis por "
      "tablas."),
    PANEL(cEdit | cAlgorithm | cSelect | cKnobs,
          "EDIT pasa de OSC A a OSC B, SUB y FILTER. ALGORITHM elige la tabla; SELECT, la página."),
    H("OSC A y OSC B, página por página"),
    R("1: WT POS, WARP, AMOUNT, LEVEL",
      "La posición en la tabla, el tipo de deformación, cuánto deforma y el volumen del "
      "oscilador."),
    R("2: VOICES, DETUNE, BLEND, WIDTH",
      "Unísono: de 1 a 8 voces, cuánto se desafinan, cuánto pesan las de fuera frente a la "
      "central y cuánto se abren en estéreo."),
    R("3: OCT, SEMI, FINE, RANGE",
      "Afinación: octavas, semitonos, centésimas y el alcance del desafinado del unísono."),
    R("4: PAN, PHASE, RAND", "Panorama, fase de arranque de la onda y cuánto varía esa fase en cada nota."),
    H("Las tablas"),
    P("ALGORITHM elige entre 72 tablas de fábrica, las tuyas y ADDITIVE. Las de fábrica vienen de cinco sitios: las "
      "de Serum 2, un paquete de Serum 1, las que usan los presets de fábrica de Serum 1, unas cuantas que crecen "
      "como un autómata celular (RULE 30, RULE 90...) y ocho, SHRUBBY, leídas del propio dibujo del arbusto."),
    R("ADDITIVE",
      "Una tabla que construyes tú con armónicos: eliges uno y le das volumen y fase. Añade tres páginas, "
      "que cubren también ganancia, modo, deformación, inicio, final, curva y número de fotogramas."),
    R("Tablas propias", "Se cargan desde el editor como archivos WAV. Lo verás en el último capítulo."),
    H("WARP, deformar la onda"),
    P("WARP elige cómo se retuerce la onda y AMOUNT, cuánto. Son las de Serum, y las que usan B (o A, en OSC B) "
      "toman el otro oscilador como modulador."),
    R("SYNC, SYNC WIN, SYNC 1/2 WIN", "Sincronía dura: la onda se reinicia antes de acabar. El clásico chillido."),
    R("BEND+, BEND-, BEND+/-", "Aprieta la onda hacia un lado, el otro o ambos."),
    R("PWM", "Ancho de pulso: estrecha una parte del ciclo."),
    R("ASYM+, ASYM-, ASYM+/-", "Deforma sin simetría: más brillo en un medio ciclo."),
    R("FLIP y MIRROR", "Da la vuelta a una parte del ciclo o lo refleja."),
    R("QUANTIZE", "Escalonado: baja la resolución de la onda, como un sonido de 8 bits."),
    R("PD (B)", "Distorsión de fase, como un Casio CZ, guiada por el otro oscilador."),
    R("FM, AM y RM", "Modulación de frecuencia, de amplitud y en anillo, desde el otro oscilador, el ruido o el sub."),
    R("REMAP", "26 curvas que leen la onda en otro orden, de 8SQR a RAND. AMOUNT acerca la lectura a la curva."),
    H("SUB y ruido"),
    P("Tercera pulsación de EDIT. El suboscilador refuerza los graves por debajo de los osciladores; el ruido da "
      "aire, golpe o textura."),
    R("1: SHAPE, OCT, DIRECT, SUB", "La forma del sub, su octava (de -4 a +4), si salta el filtro y su volumen."),
    R("2: COLOR, PITCH, NOISE", "El color del ruido, su afinación y su volumen."),
    R("Colores", "WHITE, PINK, BROWN, BLUE, CRUSH y RADIO."),
    N("DIRECT",
      "Con DIRECT en ON el sub va directo, sin pasar por el filtro: graves firmes aunque cierres el corte. "
      "La envolvente de volumen lo sigue moldeando."),
};

constexpr Block kFilter[] = {
    P("Cuarta pulsación de EDIT. Un único filtro para todo: los osciladores, el sub y el ruido que le mandes. Es la "
      "pieza de SHRUB que más se nota en un acorde, porque todas las notas se mezclan antes de filtrarse."),
    PANEL(cEdit | cAlgorithm | cKnob1 | cKnob2, "ALGORITHM elige el tipo; KNOB 1 y 2, el corte y la resonancia."),
    H("Las tres páginas"),
    R("1: CUTOFF, RES, DRIVE, MIX",
      "Corte, resonancia, saturación a la entrada y mezcla entre sonido seco y filtrado."),
    R("2: FAT, KEY TRK, ENV 2, STEREO",
      "El segundo mando de cada tipo, cuánto sigue el corte al teclado, cuánto lo abre la ENV 2 y "
      "cuánto se separa entre izquierda y derecha."),
    R("3: A, B, N, S", "Qué pasa por el filtro: OSC A, OSC B, el ruido (N) y el sub (S)."),
    P("FAT cambia de nombre según el tipo: Smooth, Pain, Width, Damp, Boeuf... Lo que esté apagado en la página 3 se "
      "salta el filtro y se une después, todavía con la envolvente de volumen. DIRECT, en el sub, gana siempre."),
    H("Los 76 tipos, por familias"),
    R("SVF", "LP, HP, BP, PEAK y NOTCH en 6, 12 o 24 dB por octava. Los de siempre, limpios."),
    R("MG LOW", "Los paso bajo de Serum 1, con carácter de sinte analógico."),
    R("Escaleras", "MG LADDER, MG DIRTY y ACID LADDER. El último, para bajos ácidos."),
    R("FRENCH LP y BANDREJECT", "Un paso bajo de otro color y un rechazo de banda."),
    R("ALLPASSES y REVERB", "Filtros que mueven la fase o difuminan: más efecto que filtro."),
    R("Peines", "32 tipos. Afinan el sonido a una nota: cuerdas pulsadas, metales, timbres raros."),
    R("Flangers y phasers", "Barridos dentro del propio filtro."),
    R("Dobles", "Dos filtros a la vez, como L+H o L/B/H."),
    H("Un filtro para todas las notas"),
    B("Una nota sola suena exactamente como en Serum."),
    B("En un acorde, todas comparten corte, resonancia y el barrido de ENV 2. KEY TRK sigue a la última nota que "
      "toques."),
    B("Con mucha resonancia, el acorde zumba con la nota que más suena."),
    B("El filtro se reinicia cuando deja de sonar la última nota."),
    N("TRUCO",
      "Para pads de acordes que respiran, filtro suave y modulación lenta en el corte. Para bajos y solos, "
      "pon GLO, VOICING en MONO o LEGATO: con una nota, el filtro único no se nota."),
    N("CON LA REVERB HALL",
      "La reverb HALL comparte memoria con los filtros de peine, ALLPASSES, REVERB y los flanger: con HALL "
      "encendida pueden no sonar. PLATE deja sitio para ambos."),
};

constexpr Block kMod[] = {
    P("Lo que mueve el sonido solo. Tres envolventes, cuatro LFO, dos fuentes de caos y una matriz que conecta cada "
      "fuente con lo que quieras."),
    H("Las envolventes"),
    PANEL(cEnv | cSelect | cKnobs, "ENV pasa de ENV 1 a ENV 2, ENV 3 y la MATRIX."),
    R("ENV 1", "El volumen de cada nota. Es la única por nota, y va después del filtro."),
    R("ENV 2", "Conectada de fábrica al mando ENV 2 del filtro y disponible en la matriz."),
    R("ENV 3", "Libre: solo sale en la matriz. Conéctala tú."),
    R("Página 1", "ATTACK, DECAY, SUSTAIN y RELEASE."),
    R("Página 2", "HOLD, y las curvas del ataque, la caída y la relajación: A CURVE, D CURVE y R CURVE."),
    P("ENV 2 y ENV 3 las comparten todas las notas. Una nota nueva las arranca desde donde estaban, y se relajan "
      "cuando sueltas la última tecla."),
    H("Los LFO"),
    PANEL(cLfo | cAlgorithm | cSelect | cKnobs | cOct,
          "LFO pasa de LFO 1 a 4, CHAOS y MATRIX. Con LFO mantenido, OCT+ y OCT- añaden y quitan puntos."),
    R("1: RATE, SKEW, DEST, AMOUNT", "Velocidad, inclinación de la forma, a dónde va y cuánto."),
    R("2: BPM, MODE, PHASE, BIPOLAR",
      "Sincronía al tempo, modo, fase de arranque y si oscila a los dos lados del valor o a "
      "uno."),
    R("3: START, PEAK, END, RANDOM", "Los tres puntos de la forma y el azar: OFF, S&H (a saltos) o SMOOTH (suave)."),
    R("4: POINT X, POINT Y, SMOOTH, DEPTH", "Mueve un punto de la forma, suaviza y fija la profundidad."),
    R("5: DRAW", "POINT, X, Y y CURVE: dibuja la forma punto a punto, hasta 17 puntos."),
    R("ALGORITHM", "FREE corre siempre; RETRIG empieza de nuevo con cada nota; ENVELOPE recorre una vez por nota."),
    N("DEST, EL ATAJO",
      "DEST en la página del LFO edita la primera conexión de la matriz que sale de ese LFO. Si no tiene "
      "ninguna, toma un hueco libre. Así no hace falta abrir la matriz."),
    P("Para dibujar: mantén LFO y pulsa OCT+ para añadir un punto u OCT- para quitarlo. Los dos extremos no se "
      "borran."),
    R("CHAOS",
      "Dos fuentes que nunca se repiten: CHAOS 1, un atractor de Lorenz, y CHAOS 2, uno de Rössler. Cada una "
      "con RATE y sincronía al tempo. Movimiento orgánico."),
    H("La matriz"),
    P("Dieciséis conexiones de una fuente a un destino. Está al final de ENV y al final de LFO: es la misma."),
    R("1: SLOT, SOURCE, DEST, AMOUNT", "La conexión, de dónde sale, a dónde va y cuánto."),
    R("2: BIPOLAR, CURVE", "Si la modulación va a los dos lados y con qué curva."),
    R("3: AUX, MODE, AUX CURVE",
      "Una segunda fuente que controla la primera: por ejemplo, el LFO solo cuando mueves la "
      "rueda."),
    R("Fuentes",
      "LFO, envolventes, caos, nota, fuerza, azar, rueda de modulación, presión, pitch bend, las cuatro "
      "macros, CC 2, CC 5 y FIXED, un valor fijo."),
    R("LANE 1 a 8",
      "Destinos especiales: encienden o silencian una pista del secuenciador. Con cantidad positiva, la "
      "pista suena con la fuente por encima de la mitad; con negativa, al revés."),
    N("LAS MACROS",
      "MACRO 1 a 4 no hacen nada solas: son fuentes de la matriz. Conéctalas aquí a lo que quieras "
      "mover con los KNOB de HOME, o arrástralas a un mando en el editor."),
};

constexpr Block kFx[] = {
    P("FX abre el rack: siete huecos en fila, cada uno con un módulo. El sonido los recorre de arriba abajo, así que "
      "el orden importa: una distorsión antes de la reverb no suena igual que después."),
    PANEL(cFx | cKnobs | cAlgorithm, "FX: el rack. Otra vez FX: cada efecto. ALGORITHM, su tipo o encendido."),
    H("El rack"),
    R("KNOB 1, SLOT", "El hueco, de 1 a 7."),
    R("KNOB 2, MODULE", "Qué módulo va en ese hueco."),
    R("KNOB 3, MOVE", "Sube o baja el módulo en la fila."),
    R("KNOB 4, ON", "Encendido o apagado."),
    P("Pulsa FX otra vez para entrar en cada efecto, en el orden del rack. Cada uno tiene dos páginas de cuatro "
      "mandos."),
    H("Los once módulos"),
    R("HYPER/DIM", "Unísono y ensanchado. 1: UNISON, RATE, DETUNE, MIX. 2: DIM SIZE, DIM MIX."),
    R("DISTORTION",
      "1: DRIVE, MIX, LEVEL, FILTER (OFF, PRE o POST). 2: FREQ, Q, LP/HP, el filtro que la "
      "acompaña."),
    R("FLANGER", "1: RATE, DEPTH, FEEDBACK, MIX. 2: BPM, WIDTH."),
    R("PHASER", "1: RATE, FREQ, DEPTH, FEEDBACK. 2: BPM, WIDTH, POLES, MIX."),
    R("CHORUS", "1: RATE, DEPTH, FEEDBACK, MIX. 2: DELAY 1, DELAY 2, LPF."),
    R("DELAY", "1: TIME L, TIME R, FEEDBACK, MIX. 2: BPM, LINK, FREQ, BW."),
    R("COMPRESSOR",
      "1: DEPTH, THRESH, UPWARD, TIME. 2: GAIN, MIX, OTT. En una banda, KNOB 1, 3 y 4 son ratio, ataque "
      "y relajación."),
    R("REVERB", "1: SIZE, DECAY, WIDTH, MIX. 2: LOW CUT, HIGH CUT, SPIN."),
    R("EQ", "Dos bandas. 1: FREQ 1, Q 1, GAIN 1, TYPE 1. 2: lo mismo, banda 2."),
    R("FX FILTER", "Un segundo filtro, con todos los tipos del de voz. 1: CUTOFF, RES, DRIVE, MIX. 2: FAT, STEREO."),
    R("PUMP",
      "El bombeo de compresor lateral, sin bombo. RATE (de compás a 1/16, con puntillos y tresillos), DEPTH, "
      "RELEASE, CURVE."),
    H("Los tipos, con ALGORITHM"),
    R("Distorsión",
      "15: TUBE, SOFT CLIP, HARD CLIP, DIODE 1 y 2, LINEAR FOLD, SINE FOLD, ZERO-SQUARE, DOWNSAMPLE, "
      "ASYM, RECTIFY, SINE SHAPER, STOMP BOX, TAPE SAT. y SOFT SAT."),
    R("Delay", "NORMAL, PING-PONG (rebota de lado a lado) y TAP."),
    R("Reverb", "PLATE, más corta y brillante, y HALL, una sala grande."),
    R("Compresor", "Encendido o apagado; OTT, en la página 2, lo vuelve multibanda, el famoso aplastado de Serum."),
    R("EQ", "Banda 1: LO SHELF, PEAK o HP. Banda 2: HI SHELF, PEAK o LP."),
    N("RECETA",
      "Para un bajo moderno: DISTORTION con TUBE o SOFT SAT., luego COMPRESSOR con OTT y poco MIX, y PUMP "
      "a 1/4 para que respire con el tempo."),
};

constexpr Block kPlay[] = {
    P("Cómo se toca: cuántas notas suenan y cómo se enlazan, en qué escala, con qué acordes y con qué arpegio. GLO "
      "abre VOICING, SEL abre SCALE y ARP abre el arpegiador."),
    H("VOICING"),
    PANEL(cGlo | cKnobs | cSelect, "GLO: VOICING. Otra vez GLO: GLOBAL, los ajustes generales."),
    R("MONO", "Una nota. Al soltar la de arriba vuelve a la que sigas pisando."),
    R("LEGATO", "Las notas ligadas no reinician las envolventes. Funciona también tocando acordes."),
    R("GLIDE", "El portamento: cuánto tarda en deslizarse de una nota a otra."),
    R("ALWAYS", "Desliza también entre notas sueltas, no solo entre ligadas."),
    R("Página 2", "RAND CUT, RAND TUN, RAND PAN, RAND VOL: un poco de azar en corte, afinación, panorama y volumen."),
    H("Cuántas notas"),
    P("Hasta ocho notas a la vez, sacadas de un total de 16 voces, y cada nota puede usar hasta ocho voces de unísono "
      "por oscilador. Cuanto más gordo el unísono, menos notas caben; siempre suena al menos una. En POLY la pantalla "
      "dice cuántas caben, por ejemplo «POLY, 7 NOTES». Si una nota nueva no cabe, SHRUB le cede una voz sin "
      "chasquidos."),
    N("MAS NOTAS",
      "QUALITY en DRAFT, en GLOBAL página 5, lee los osciladores con menos detalle y deja tocar más notas "
      "en sonidos polifónicos. Los mono suenan igual."),
    H("SCALE"),
    PANEL(cSel, "El botón que lleva impreso SEL abre SCALE."),
    R("1: KEY, SCALE, CHORD", "La tónica (apagada o de C a B), una de 51 escalas y el modo de acordes."),
    R("2: OUT OF KEY", "CORRECT lleva la nota fuera de escala a la más cercana; MUTE la calla; THRU la deja pasar."),
    R("2: KEYS LIT", "SCALE PAGE ilumina la escala solo en esta página; ALWAYS, siempre, suave."),
    R("2: ARP+SCALE",
      "PRESET: cada preset trae su arpegio, tónica, escala y acorde. GLOBAL: se quedan los tuyos al "
      "cambiar de preset."),
    H("Acordes con una tecla"),
    KEYS(kAllBlack,
         "Con CHORD encendido, las once teclas negras eligen el tipo de acorde, desde el Fa# de abajo, y las "
         "blancas tocan los grados de la escala."),
    P("Los once tipos, en orden: ROOT, 3RD, TRIAD, 7TH, 9TH, SUS2, SUS4, 6TH, POWER, OPEN y OCTAVE. Cada tecla blanca "
      "construye el acorde con las notas de la escala: en Do mayor con TRIAD, la tecla de Re toca Re menor sin que "
      "pienses en ello."),
    H("El arpegiador"),
    PANEL(cArp | cAlgorithm | cKnobs | cPlay, "ARP abre su página y, pulsado otra vez, lo enciende o apaga."),
    R("1: RATE, GATE, OCTAVES, CHANCE",
      "La figura (de un compás a 1/64, con puntillos y tresillos), la duración de cada nota "
      "(1-200 %), cuántas octavas (1-4) y la probabilidad de que suene cada paso."),
    R("2: OFFSET, REPEATS, RETRIG, LATCH",
      "RETRIG reinicia el patrón con cada nota, tiempo, compás o cada pocos compases; LATCH lo "
      "deja sonando al soltar. El manual no detalla OFFSET ni REPEATS: pruébalos."),
    R("3: SWING", "De 50 % (recto) a 75 %: retrasa cada segundo paso."),
    R("Patrones",
      "19, con ALGORITHM: UP, DOWN, UP/DOWN, DOWN/UP, UP+DOWN, DOWN+UP, THUMB UP, THUMB UD, PINKY UP, "
      "PINKY UD, CONVERGE, DIVERGE, CON+DIVERGE, PLAYED, CHORD, RANDOM, RAND NO DUP, RAND DRIFT, RAND ONCE."),
    N("GATE Y LEGATO",
      "Con GATE por encima del 100 % las notas se pisan, y en LEGATO se deslizan unas a otras: el bajo "
      "de acid sale solo."),
    P("Con OCT- y OCT+ a la vez retienes las notas: el arpegio sigue mientras tocas otra cosa. Vuelve a pulsar los "
      "dos para soltar."),
};

constexpr Block kSeq[] = {
    P("El secuenciador de SHRUB es de movimiento, como en Serum: graba lo que haces con los mandos y lo repite en "
      "bucle. También graba notas. Cada sonido tiene 27 secuencias, una por tecla, y cada secuencia ocho pistas."),
    PANEL(cSeq | cRec | cPlay | cSelect | cAlgorithm | cKnobs,
          "SEQ abre las pistas, REC graba, PLAY arranca y para. SELECT elige pista; ALGORITHM, su largo."),
    H("Qué guarda una pista"),
    R("Movimiento",
      "Lo que haces con un mando, que vuelve a sonar en bucle. Los valores continuos se deslizan entre "
      "puntos; los de tipo o modo saltan."),
    R("Notas", "Las teclas que tocas, una nota a la vez por pista."),
    R("Qué se graba",
      "Casi cualquier mando de sonido: osciladores, filtro, envolventes, LFO y efectos. No la elección de "
      "tabla. De GLOBAL, solo LEVEL, GLIDE y las macros."),
    R("REC NOTES",
      "En GLOBAL: MONO pone un acorde en una pista; POLY da una pista a cada nota del acorde; OFF no "
      "graba notas."),
    H("Grabar el movimiento de un mando"),
    S("1",
      "Pulsa REC. Si suena, graba desde el compás siguiente y REC parpadea hasta entonces. Si está parado, "
      "cuenta cuatro clics, de IN 4 a IN 1, y empieza en el compás 1."),
    S("2", "Gira un mando. La toma acaba cuando dejas de girar un momento."),
    S("3", "Pulsa REC otra vez, o PLAY, para terminar."),
    H("Notas paso a paso"),
    S("1", "Para el transporte. Mantén SEQ y pulsa REC: la pantalla dice STEP y enseña el cursor."),
    S("2", "Fija antes el largo de la pista con ALGORITHM."),
    S("3", "Cada tecla pone una nota en el cursor; varias juntas, un acorde. El cursor avanza al soltar."),
    S("4",
      "KNOB 1, CURSOR: adelante deja un silencio, atrás corrige. KNOB 2, STEP: de 1/32 a un compás. KNOB 3, "
      "GATE: de 10 a 100 % del paso. KNOB 4, TEMPO."),
    S("5", "Termina con REC, con PLAY o saliendo de SEQ."),
    GRID("NOTA:x---x---x-x-x---|PASO:xxxxxxxxxxxxxxxx",
         "Con STEP en 1/16: cada tecla ocupa un paso y avanzar el cursor sin tocar deja un silencio."),
    H("Editar una pista"),
    R("SELECT", "Elige la pista, de 1 a 8."),
    R("KNOB 1, POINT", "El punto o la nota."),
    R("KNOB 2, VALUE", "Su valor."),
    R("KNOB 3, TIME", "Su sitio en el tiempo."),
    R("KNOB 4, TEMPO", "En una pista de notas, la duración de la nota."),
    R("ALGORITHM", "El largo, de compás en compás hasta 128, o mitad y doble. Con SEQ mantenido, de 1/16 en 1/16."),
    P("Cada pista da la vuelta con su propio largo. Una de 3 compases contra una de 4 se van desplazando: el sonido "
      "tarda doce compases en repetirse igual."),
    H("Con SEQ mantenido"),
    PANEL(cSeq | cKeys | cOct | cSel | cKnobs, "SEQ mantenido convierte teclas, octavas, SEL y mandos en atajos."),
    R("+ una tecla",
      "Elige la secuencia de esa tecla. Sonando, cambia en el compás siguiente, o en el tiempo o la "
      "semicorchea si KNOB 4 está en BEAT o 1/16."),
    R("+ tecla mantenida y otra vacía", "Copia la secuencia de la primera tecla a la segunda."),
    R("+ OCT-", "Borra el punto."),
    R("+ OCT+", "Vacía la pista."),
    R("+ OCT- y OCT+", "Vacía todas las pistas."),
    R("+ KNOB 1, SWING", "Retrasa las semicorcheas a contratiempo de las pistas de notas. Es el mismo del arpegio."),
    R("+ KNOB 2, OVERLAP", "Apagado, cada nota corta la anterior; encendido, cada una suena entera sobre las demás."),
    R("+ KNOB 3, PARAMS",
      "STAY: los mandos se quedan donde los dejan las pistas. RETURN: vuelven a su valor al parar o "
      "cambiar de secuencia."),
    R("+ KNOB 4, SWITCH", "Cuándo cambia de secuencia: BAR, BEAT o 1/16."),
    R("+ SEL", "TRANSPOSE SEQ: las teclas, o las notas MIDI, transponen la secuencia. ALGORITHM la fija en las notas."),
    N("27 SECUENCIAS",
      "Pulsa SEQ dos veces seguidas: las teclas eligen secuencias directamente, para tocar en directo, "
      "hasta que pulses SEQ dos veces más o un botón de octava."),
    KEYS(kAllWhite | kAllBlack, "Cada una de las 27 teclas guarda una secuencia del sonido."),
    H("Tempo y memoria"),
    B("El tempo lo guarda la FM-1, pero un sonido con secuencia trae el suyo."),
    B("Si llega reloj MIDI, el transporte le sigue. Pulsa PLAY en la FM-1 para recuperar el mando."),
    B("Las 27 secuencias y el sonido comparten alrededor de 1 KB por hueco. Si no caben, SAVE las suaviza o "
      "las deja fuera, y lo dice."),
    B("Desde la matriz, los destinos LANE 1 a 8 silencian pistas: una pista muda sigue corriendo y aparece en "
      "gris."),
};

constexpr Block kMore[] = {
    H("GLOBAL, página por página"),
    PANEL(cGlo | cSelect | cKnobs, "GLO dos veces: GLOBAL. Cinco páginas."),
    R("1: TEMPO, LEVEL, CLICK, ARP+SCALE",
      "Tempo (120 de fábrica); volumen del preset; metrónomo OFF, ON o REC (solo al grabar, "
      "de fábrica); y si arpegio y escala van con el preset o son globales."),
    R("2: VOICES, STEP, PAN, TUNE",
      "Control de voces: de OFF a 8 pasos. Cada nota nueva toma el paso siguiente, con su "
      "panorama y afinación. Abre un pad en estéreo o desafina notas alternas."),
    R("3: MIDI CH, USB, LUZ, BEND",
      "MIDI CH: OMNI, 1-16 o MPE. USB LEVEL: el nivel del audio USB. KEY LIGHT: luz de fondo de las "
      "teclas (OFF, PNO1-5, ALL1-5). BEND RANGE: OFF, 2, 7, 12 o 24."),
    R("4: REC NOTES, MIDI OCT, BT, LINK",
      "REC NOTES: cómo graba notas el secuenciador. MIDI OCT: de -4 a +4. BLUETOOTH, y LINK, el "
      "estado del enlace con otra unidad."),
    R("5: A4 TUNING, QUALITY, CPU MHZ",
      "La afinación del La (415 a 466 Hz, 440 de fábrica), NORMAL o DRAFT, 360 o 320."),
    H("USB LEVEL"),
    R("MASTER", "El audio por USB sigue el mando MASTER."),
    R("FIXED", "El USB va siempre a tope; MASTER solo baja el altavoz y los auriculares."),
    R("SPK OFF", "Calla altavoz y salida para que no lo oigas doble al escuchar por el ordenador."),
    H("Bluetooth"),
    R("OFF", "La radio apagada: no gasta nada."),
    R("ON", "La FM-1 aparece como SHRUB-XXXX y toca las notas que recibe en su canal."),
    R("LEAD",
      "Busca otra FM-1 con SHRUB y le manda reloj, arranque, parada y posición para tocar a tempo las "
      "dos."),
    P("La primera vez que enciendes el Bluetooth, la FM-1 se reinicia una vez. Si dos unidades no se encuentran, LINK "
      "girado a la derecha prueba una calibración genérica en el siguiente arranque; a la izquierda, vuelve a la "
      "suya."),
    H("MIDI"),
    B("Escucha por USB y por la entrada MIDI de minijack, en el canal de MIDI CH."),
    B("Notas, pitch bend, rueda, presión, sustain y cambio de programa, que elige el hueco."),
    B("369 NRPN: cada parámetro automatizable tiene el suyo, para llegar a todo desde un DAW."),
    B("Reloj de entrada y de salida, con arranque, parada y posición."),
    B("MPE: pon el controlador en la zona baja. Cada nota se dobla sola (±48 semitonos); de fábrica, su presión "
      "mueve su volumen y su CC 74, la posición de tabla. Filtro y efectos son comunes, así que el CC 74 no mueve "
      "el corte."),
    H("Los CC de fábrica, los más útiles"),
    R("CC 74 y 71", "Corte y resonancia del filtro."),
    R("CC 16 a 19", "MACRO 1 a 4."),
    R("CC 20 y 21", "WT POS de OSC A y OSC B."),
    R("CC 73, 75, 78, 72", "ENV 1: ataque, caída, sostenimiento y relajación."),
    R("CC 76, 77, 86, 87", "La velocidad de los LFO 1 a 4."),
    R("CC 91, 94, 93", "Mezcla de reverb, delay y chorus."),
    R("CC 5 y 7", "GLIDE y el volumen del preset."),
    R("CC 120, 123, 121", "Apagar todas las notas, y reiniciar controladores."),
    H("El editor"),
    P("shrub.audio/editor, en Chrome o Edge. Sin la FM-1 hace de sinte en el navegador; con Connect FM-1 edita la "
      "tuya por USB y puede enseñar su pantalla en grande. Pestañas: OSC, FX, MATRIX, GLOBAL, SEQ y LIBRARY."),
    R("LIBRARY",
      "Los 128 huecos de la FM-1 en 8 filas de 16, por colores de categoría. Get from FM-1 lo trae todo; "
      "Send to FM-1 manda solo los cambios. Arrastra para ordenar."),
    R("Presets de Serum 1",
      "Importa archivos .fxp. Son un punto de partida: un solo filtro, 8 notas, siete efectos. Lee los "
      "avisos que da al importar."),
    R("A Serum 2",
      "File, Export to Serum2: un .SerumPreset. Arpegio, escala y azar por nota no pasan. Al revés no se "
      "puede."),
    R("Tablas propias",
      "File, User Wavetables: WAV de Serum o con 2048 muestras por fotograma, hasta 64 fotogramas. Tres "
      "bancos de 16."),
    R("Bancos",
      "Un .shrubbank lleva los 128 huecos, sus tablas y el mapa MIDI: tu copia de seguridad. En Bank, "
      "Factory está Juneau, 80 sonidos de polisinte de los 80."),
    R("MIDI Learn", "Clic derecho en cualquier mando. La FM-1 se queda el mapa."),
    N("COMPARTIR",
      "Un enlace para compartir no lleva tus tablas propias: para eso, guarda un archivo .shrub, que lleva "
      "un sonido con sus pistas, o un .shrubbank."),
    H("Si algo no va"),
    R("No suena", "Mira MASTER, LEVEL del preset en GLOBAL y USB LEVEL: SPK OFF calla el altavoz a propósito."),
    R("Notas colgadas", "Manda CC 120 o CC 123, o pulsa el botón de pánico del editor."),
    R("BAT LOW", "Batería por debajo del 30 %. Se quita por encima del 33 %. GLOBAL da el porcentaje exacto."),
    H("Chuleta"),
    R("Botón otra vez", "La parte siguiente de la sección. En FX, el efecto siguiente."),
    R("EDIT, ENV, LFO + SELECT", "Salta entre las partes."),
    R("HOME otra vez", "Los KNOB pasan a ser las macros, o vuelven."),
    R("HOME mantenido", "Favorito, o deja de serlo."),
    R("GLO mantenido", "El arbusto: SELECT da un sonido al azar o una mezcla."),
    R("PRESETS con cambios", "EDITED / TURN AGAIN: gira otra vez para irte."),
    R("SAVE otra vez", "Guarda."),
    R("ARP otra vez", "Enciende o apaga el arpegio."),
    R("OCT- + OCT+", "Retener notas."),
    R("LFO + OCT+ / OCT-", "Añade o quita un punto de la forma dibujada."),
    R("SEQ + REC, parado", "Notas paso a paso."),
    R("SEQ + tecla", "Elige secuencia."),
    R("SEQ + SEL", "Transponer la secuencia."),
    R("SEQ dos veces", "Las teclas eligen secuencias."),
    R("OCT- + OCT+ al encender", "Rescate por USB."),
    R("GLO al encender", "Apaga o enciende el segundo núcleo."),
    R("HOME al encender", "Bloqueo infantil. Fija antes el volumen y guarda lo editado."),
};

constexpr Chapter kChapters[] = {
    FMGUIDE_CHAPTER("Qué es SHRUB", kWhat),
    FMGUIDE_CHAPTER("Instalar, volver y rescate", kInstall),
    FMGUIDE_CHAPTER("Moverse por el panel", kMove),
    FMGUIDE_CHAPTER("Presets y tu primer sonido", kPresets),
    FMGUIDE_CHAPTER("Osciladores, sub y ruido", kOsc),
    FMGUIDE_CHAPTER("El filtro", kFilter),
    FMGUIDE_CHAPTER("Envolventes, LFO y matriz", kMod),
    FMGUIDE_CHAPTER("Los efectos", kFx),
    FMGUIDE_CHAPTER("Voces, escala y arpegio", kPlay),
    FMGUIDE_CHAPTER("El secuenciador de mandos", kSeq),
    FMGUIDE_CHAPTER("Ajustes, editor, chuleta", kMore),
};

}  // namespace

const Guide kGuideShrub = {
    "SHRUB",   "Serum en la FM-1: tablas de ondas, filtros, matriz y efectos.", "V1.39",
    kChapters, static_cast<int>(sizeof(kChapters) / sizeof(kChapters[0])),
};

}  // namespace fmguide

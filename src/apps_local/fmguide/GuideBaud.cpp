#include "GuideData.h"

// FM-1+VA version 102, by Baud Girl (baudgirl.com/work/FM-1+VA), from its
// manual as read on 16 November 2026.

namespace fmguide {

namespace {

constexpr Block kWhat[] = {
    P("FM-1+VA no convierte la FM-1 en otra cosa: la mejora. Sigue siendo el sintetizador FM de seis operadores, con "
      "sus 128 presets, y le añade dos motores más, siete efectos por preset, un secuenciador de verdad y un "
      "generador de acordes y arpegios. Es una modificación del firmware original V15, no un programa nuevo."),
    FLOW("FM>VA>8-BIT",
         "Tres motores. Cada preset es de uno de ellos, y los tres comparten efectos, envolvente, LFO y "
         "secuenciador."),
    R("FM", "Seis operadores y 32 algoritmos, como siempre. 12 notas a la vez. Los 128 presets de fábrica son FM."),
    R("VA",
      "Analógico virtual: una onda, engordada y filtrada. Supersierras, bajos tipo 303, pads. 12 notas con seno "
      "o sierra, 8 con triángulo o cuadrada."),
    R("8-Bit",
      "Una consola entera en un preset: batería, efectos de sonido, bajo y solista, repartidos por el "
      "teclado."),
    H("Lo que añade"),
    B("16 patrones de hasta 64 pasos, con bloqueos de parámetro, probabilidad, redobles y encadenado."),
    B("Siete efectos por preset, en el orden que quieras: filtro, reverb, delay, distorsión, chorus, phaser y "
      "bitcrush."),
    B("Los cuatro KNOB tocan lo que tú elijas, por bancos."),
    B("Note Engine: arpegiador, y acordes o arpegios automáticos en una tonalidad."),
    B("Bluetooth MIDI como dispositivo o como anfitrión, reloj MIDI de entrada y salida, y carga de parches de DX7."),
    B("Device Manager: un editor y biblioteca en el navegador."),
    N("PARA QUIEN",
      "Es el paso más corto desde el firmware original y el menos arriesgado: conserva tus presets y "
      "patrones, y la FM-1 sigue comportándose como la conoces."),
};

constexpr Block kInstall[] = {
    P("Se instala desde la página Install del proyecto, con Chrome o Edge en un ordenador, o con Chrome en un móvil o "
      "tableta Android. Tarda menos de un minuto y conserva los presets y patrones guardados."),
    N("ANTES, V15",
      "FM-1+VA se instala sobre el firmware original V15 o sobre una versión anterior de sí mismo. Con "
      "V13 o V14 el instalador se niega antes de enviar nada: actualiza primero con el programa de "
      "M-VAVE."),
    S("1",
      "Pulsa SAVE en la FM-1: lo que no esté guardado se pierde. Apunta tus ajustes de la pantalla GLOBE; "
      "viniendo del firmware original pueden volver a los de fábrica."),
    S("2", "Abre la página Install y pulsa Next."),
    S("3",
      "Conecta la FM-1 directamente por USB, enciéndela y cierra otros programas que la usen. Next, y luego I "
      "understand."),
    S("4", "Permite MIDI cuando el navegador lo pida. El instalador encuentra la FM-1 y muestra su versión. Next."),
    S("5", "Deja la versión actual o elige otra. Next."),
    S("6",
      "Pulsa el botón Install. La pantalla de la FM-1 se queda oscura unos 30 segundos: es normal. No la apagues "
      "ni cierres la página."),
    S("7", "La FM-1 se reinicia y la página dice Installation done!"),
    N("SI SE CORTA",
      "No apagues con la pantalla oscura salvo que la página diga que no se instaló nada. Recarga la "
      "página Install con la FM-1 conectada: el instalador ofrece terminar."),
    H("Qué versión tengo"),
    P("Sale bajo el logo al encender, y en GLO, About: Version (por ejemplo FM-1_102) y Built, la fecha."),
    H("Paquetes de presets"),
    P("Los motores nuevos traen sus presets aparte, y se instalan desde el Device Manager, en el banco D: el paquete "
      "VA "
      "en los presets 113 a 128 y el paquete 8-Bit en los 097 a 112. Library, elige el paquete, Place all 16 y Send. "
      "Sustituyen a los 16 presets FM que hubiera en esos números."),
    H("Volver al firmware original"),
    S("1", "Descarga el firmware de la FM-1 de la página de M-VAVE."),
    S("2",
      "En la página Install, en el paso de elegir versión, pulsa Install a file from my computer y dale ese "
      "archivo."),
    S("3", "Instala como siempre."),
    P("Tus presets se quedan, pero los de VA y 8-Bit no suenan en el firmware de M-VAVE, que solo entiende FM."),
};

constexpr Block kMove[] = {
    P("Todo FM-1+VA se maneja igual: listas. Un mando recorre las filas, otro cambia el valor, un botón entra y otro "
      "sale."),
    PANEL(cSelect | cAlgorithm | cSel | cHome,
          "Los cuatro controles para moverse: SELECT recorre, ALGORITHM cambia, SEL entra y HOME sale."),
    R("SELECT", "Sube y baja por las filas, una por clic."),
    R("ALGORITHM", "Cambia el valor resaltado. Girado deprisa, salta más."),
    R("SEL", "Entra en un grupo, lanza una acción o responde que sí. En HOME no hace nada."),
    R("HOME", "Un nivel atrás. Desde la pantalla HOME, abre el menú de bancos de mandos."),
    R("PRESETS", "Elige el preset y abre su lista. Con SEL mantenido, elige el patrón."),
    R("MASTER", "El volumen."),
    N("PASOS GRANDES",
      "Con SEL mantenido, ALGORITHM cambia de diez en diez y SELECT salta a la sección siguiente de "
      "la lista."),
    H("Los botones"),
    R("EDIT", "La lista del preset. Otra vez: HOME."),
    R("FX, ENV, LFO", "Las listas de efectos, envolvente y LFO del preset."),
    R("GLO", "La pantalla GLOBE: los ajustes de toda la FM-1."),
    R("SAVE", "Guarda todos los cambios. Desde cualquier pantalla."),
    R("ARP", "Enciende el Note Engine y abre su pantalla. En esa pantalla, otra vez lo apaga."),
    R("SEQ", "La pantalla de patrón. Otra vez: los ajustes del patrón."),
    R("PLAY y REC", "Arrancar y parar; grabar por pasos, o en tiempo real si está sonando."),
    R("OCT- y OCT+", "Una octava abajo o arriba, de -3 a +3. Juntos: octava 0."),
    H("Combinaciones"),
    R("ENV mantenido 1 s", "Enciende o apaga la envolvente del preset."),
    R("PLAY mantenido 1 s", "Enciende o apaga el secuenciador."),
    R("HOME + SELECT", "Pasa al siguiente banco de mandos, en cualquier pantalla."),
    R("HOME + un KNOB", "En el banco Preset, cambia qué parámetro toca ese mando."),
    R("SEL + PRESETS", "El patrón siguiente."),
    R("SEL + un KNOB", "Grabando por pasos: bloquea ese ajuste en el paso."),
    R("REC + teclas", "Alarga esas notas hasta el paso siguiente."),
    R("OCT + ALGORITHM", "Transpone por semitonos, de -12 a +12."),
    H("Las luces"),
    P("FX, LFO, EDIT, GLO y HOME se encienden en su pantalla. ENV queda encendido si el preset tiene la envolvente "
      "activa; ARP, si el Note Engine está en marcha. SEQ parpadea a cada tiempo mientras el secuenciador está "
      "encendido."),
    N("PREGUNTAS",
      "Las acciones que destruyen algo preguntan antes, con Cancel resaltado: un SEL despistado cancela. "
      "Para confirmar, lleva SELECT a la acción y pulsa SEL."),
};

constexpr Block kKnobs[] = {
    P("Los cuatro KNOB tocan cuatro ajustes a la vez, y cuáles son depende del banco elegido. Hay once bancos. La "
      "pantalla HOME muestra una columna por mando y, debajo, el nombre del banco: «Knobs: Preset»."),
    PANEL(cKnobs | cHome | cSelect,
          "HOME en la pantalla HOME abre el menú de bancos. HOME mantenido y SELECT pasa al banco siguiente desde "
          "cualquier pantalla."),
    H("Los once bancos"),
    R("Preset", "Cuatro parámetros del sonido, a tu elección."),
    R("Filter", "Tipo, Cutoff, Resonance y encendido."),
    R("Reverb", "Tipo, Decay, Mix y encendido."),
    R("Delay", "Feedback, Rate, Mix y encendido."),
    R("Distortion", "Gain, Tone, Level y encendido."),
    R("Chorus y Phaser", "Rate, Depth, Mix y encendido."),
    R("Bitcrush", "Bits, Sample Rate, Mix y encendido."),
    R("Global", "Tempo, Arp Rate, Arp Gate y Glide Time."),
    R("Mixer", "FM y VA: nivel del preset y del metrónomo. 8-Bit: niveles de batería, bajo, solista y metrónomo."),
    R("MIDI CC", "Envía los CC 74, 71, 73 y 72 hacia fuera. No cambia nada en la FM-1."),
    N("KNOB 4 ENCIENDE",
      "En los siete bancos de efecto, KNOB 4 es el interruptor del efecto: se enciende y apaga "
      "desde cualquier pantalla."),
    H("El banco Preset"),
    P("Cada preset guarda cuatro elecciones de las ocho de su motor. Para cambiar una: mantén HOME y gira ese KNOB; "
      "un mensaje dice cuál ha quedado. Pulsa SAVE para conservarlo."),
    R("FM", "Brightness, Feedback, Attack, Decay, Release, Vibrato, LFO Speed y Cutoff."),
    R("VA", "Cutoff, Resonance, Filter Env, Filter Decay, Shape, Super, Detune y LFO to Cutoff."),
    R("8-Bit",
      "Drum Decay, Bass Arpeggio, Lead Arpeggio, Lead Decay, Lead Arp Speed, Lead Vibrato, Lead Release y "
      "Bass Decay."),
    P("En un preset FM, los cambios de mando se oyen desde la nota siguiente; en VA y 8-Bit afectan a las notas que "
      "ya suenan. Brightness mueve el nivel de los operadores moduladores, y no hace nada en el algoritmo 32, que no "
      "tiene ninguno."),
    N("UN MANDO MUDO",
      "En el banco Mixer, con un preset FM o VA, KNOB 2 y KNOB 3 no tienen nada asignado. Y si un "
      "mando cambia un efecto en vez del sonido, es que estás en el banco de ese efecto."),
};

constexpr Block kPresets[] = {
    P("Hay 128 presets, del 001 al 128, en cuatro bancos de 32 (A a D). Cada preset guarda los ajustes de su motor, "
      "los siete efectos y su orden, las cuatro elecciones de mando, el nombre y los niveles."),
    H("Elegir"),
    P("PRESETS abre una lista de cinco presets sobre la pantalla y cada uno suena al llegar a él. Girando despacio "
      "carga al momento; girando deprisa muestra número y nombre y carga cuando el mando se queda quieto. HOME, o no "
      "tocar, cierra la lista."),
    H("Guardar"),
    P("SAVE, desde cualquier pantalla, lo guarda todo de una vez: el preset, los ajustes globales, el arpegiador y "
      "los 16 patrones. La pantalla dice Changes saved."),
    N("EL PUNTO",
      "Un punto tras el nombre en HOME significa cambios sin guardar. Si giras PRESETS, se pierden sin "
      "preguntar."),
    H("Poner nombre"),
    S("1", "En EDIT, resalta Name y pulsa SEL."),
    S("2", "SELECT mueve el cursor y ALGORITHM cambia la letra. Hasta diez caracteres."),
    S("3", "SEL confirma, HOME cancela. Después, SAVE."),
    H("Preset nuevo, de otro motor"),
    P("Borrar un preset es la forma de crear uno de VA o de 8-Bit sin instalar los paquetes."),
    S("1", "EDIT, Erase Preset, SEL. Lleva SELECT a Erase y pulsa SEL."),
    S("2", "Elige el motor con SELECT: FM, VA u 8-Bit. SEL."),
    S("3",
      "El preset pasa a llamarse INIT. No queda guardado hasta que pulses SAVE; si giras PRESETS, vuelve el "
      "anterior."),
    H("Copiar y restaurar"),
    R("Copy Preset",
      "En EDIT. ALGORITHM elige el destino; SELECT a Copy y SEL. Pasas a la copia; SAVE la hace "
      "permanente."),
    R("Reset Bank",
      "En GLOBE. Devuelve un banco entero a su copia guardada. No se puede deshacer: haz antes una "
      "copia en el Device Manager. Restaurar el banco D quita los dos paquetes."),
    H("Lo que comparten los tres motores"),
    P("Al final de la lista de cualquier preset están LFO, Envelope (Attack, Decay, Sustain, Release), Effects, Knobs "
      "y Name. La envolvente viene apagada: enciéndela con ENV mantenido. Tocar cualquiera de sus cuatro valores "
      "también la enciende."),
    R("Release en VA", "Mantiene el nivel al soltar y funde de 0,1 a 2 segundos."),
    R("Release en FM", "Solo puede acortar el final propio de los operadores, nunca alargarlo."),
};

constexpr Block kFm[] = {
    H("FM: seis operadores"),
    P("Cada operador es una onda con su propia envolvente. El algoritmo decide cuáles suenan (portadores) y cuáles "
      "modulan a otros (moduladores). Los moduladores cambian el timbre; los portadores, el volumen."),
    FLOW("MODULADOR>PORTADOR>SALIDA",
         "Subir el nivel de un modulador hace el sonido más brillante; subir el de un "
         "portador lo hace más fuerte."),
    R("Algorithm", "1 a 32. El primer giro de ALGORITHM solo enseña el dibujo; los siguientes lo cambian."),
    R("Feedback", "0 a 7. Un operador se modula a sí mismo: aspereza."),
    R("Osc Sync", "Reinicia todos los operadores en cada nota."),
    R("Transpose", "Por octavas."),
    R("Mono", "Una nota cada vez."),
    H("Editar un operador"),
    P("En la pantalla EDIT de un preset FM, algunas teclas abren ajustes en vez de sonar: las marcadas OP1 a OP6 "
      "abren cada operador. Mantener una de ellas apaga ese operador mientras la sujetas: así oyes qué aporta. "
      "Otras abren la envolvente de tono (PIT), el principio de la lista (GLO) y eligen MONO o POLY."),
    R("Tune", "Output Level, Coarse, Fine, Detune y Osc Mode (frecuencia fija)."),
    R("Envelope", "Rate 1 a 4 y Level 1 a 4. Rate 4 y Level 4 son el final al soltar."),
    R("Sensitivity", "Key Velocity y Amp Mod Sens."),
    R("Key Scaling", "Break Point, profundidades y curvas a cada lado, y Rate Scaling."),
    N("FILTRO EN FM",
      "Un preset FM puede tener su propio filtro, con las mismas filas que el de VA. Cuesta notas: con "
      "LP24 y resonancia alta quedan 9 en vez de 12."),
    H("VA: una onda y un filtro"),
    R("Waveform", "Sine, Saw, Tri o Square. También se cambia con ALGORITHM en HOME."),
    R("Super y Detune",
      "Super añade seis copias de la onda; Detune las separa. Los dos arriba sobre Saw: supersierra."),
    R("Drift", "Desafina cada nota un poco. Bajo, da calidez; arriba es inservible a propósito."),
    R("Sub, Noise, PWM", "Una cuadrada una octava abajo, soplido, y anchura de pulso (solo con Square)."),
    R("Filter Type", "LP12, LP24, BP o HP."),
    R("Cutoff y Resonance", "De 20 Hz a 20 kHz, y el énfasis en ese punto."),
    R("Envelope, Decay, Shape", "Cuánto mueve el filtro su envolvente, cuánto dura y qué forma tiene."),
    R("Velocity, Key Tracking", "Tocar fuerte abre el filtro; el filtro sigue al teclado."),
    P("Decay corto con Envelope alto da un punteo. Decay largo con Cutoff bajo da un crescendo. Con Mono activado, "
      "una tecla nueva toma el relevo sin reatacar: así se hacen los deslizamientos."),
    H("8-Bit: cuatro partes"),
    FLOW("BATERIA>EFECTOS>BAJO>SOLISTA",
         "De grave a agudo por el teclado: doce tambores, doce efectos de sonido, tres "
         "octavas de bajo y tres de solista. OCT- y OCT+ te mueven entre ellas."),
    P("En la octava 0 las teclas tocan el bajo, salvo las tres últimas, que ya son solista. En la octava -3, las "
      "doce teclas más graves son la batería. Los efectos de sonido son fijos: moneda, salto, láser, disparo, "
      "explosión y siete más."),
    R("Ondas", "Quince: cuatro pulsos, triángulo, ruido, metal y ocho ondas tipo Game Boy."),
    R("Arpeggio", "31 formas de acorde con una sola tecla, o una dibujada por ti en ocho pasos."),
    R("Key", "Con una tonalidad puesta, bajo y solista tocan acordes de la escala."),
    R("Tone y Tuning", "Raw o Smooth; Exact o Chip, que afina como la consola, con sus notas agudas desafinadas."),
};

constexpr Block kFx[] = {
    H("Siete efectos"),
    P("FX abre la lista de efectos del preset. De arriba abajo es el orden por el que pasa el sonido. Cada efecto se "
      "enciende girando ALGORITHM sobre su fila, y SEL entra en sus ajustes."),
    R("Filter", "Low Pass, Band Pass o High Pass; Cutoff y Resonance."),
    R("Reverb", "Room, Hall o Plate; Decay y Mix."),
    R("Delay", "Feedback, Rate (de 0,8 a 0,1 s entre ecos) y Mix."),
    R("Distortion", "Soft Clip, Hard Clip o Foldback; Gain, Tone y Level."),
    R("Chorus", "Rate, Depth y Mix."),
    R("Phaser", "Rate, Depth y Mix."),
    R("Bitcrush", "Bits (16 a 1), Sample Rate y Mix."),
    N("CAMBIAR EL ORDEN",
      "En la lista de efectos, resalta uno, mantén SEL (la fila parpadea) y gira SELECT: sube o "
      "baja un puesto por clic. Suelta SEL para dejarlo."),
    P("El filtro de efecto actúa sobre el preset ya terminado. Los filtros propios de VA y de FM actúan nota a nota, "
      "antes de los efectos, y pueden funcionar a la vez. Al subir Gain en la distorsión sube también el volumen: "
      "compensa con Level."),
    H("Note Engine"),
    P("ARP enciende el Note Engine y abre su pantalla. Tiene tres modos, que se eligen en su primera fila con "
      "ALGORITHM. Sus ajustes son globales: los mismos para todos los presets."),
    R("Arp", "Arpegia las notas que mantienes."),
    R("Auto-Arp", "Cada tecla toca su propio acorde, nota a nota."),
    R("Auto-Chord", "Cada tecla toca su acorde entero, de golpe."),
    H("Modo Arp"),
    R("Mode",
      "Up, Down, Inclusive (sube y baja repitiendo extremos), Exclusive (sin repetirlos), Random, Order (en el "
      "orden pulsado) y Repeat (todas juntas en cada paso)."),
    R("Octaves", "1 a 4."),
    R("Rate, Gate, Swing", "De 1/1 a 1/32T; de 20 a 100; de 50 a 75."),
    R("Latch", "Sigue sonando al soltar las teclas."),
    R("MIDI In", "Arpegia también las notas de un teclado MIDI externo."),
    P("No hay fila de tempo: el arpegiador va al Tempo global, el primero de GLOBE y KNOB 1 del banco Global."),
    H("Acordes en una tonalidad"),
    P("En Auto-Arp y Auto-Chord, la fila Key fija la tonalidad (de C a B, u Off). Con ella, Scale, Chord y Pattern "
      "deciden qué acorde da cada tecla y en qué orden suena. Sin ella, se usa una de las 31 formas de Arpeggio."),
    R("Chord", "Triad, Seventh, Sixth, Sus 2, Sus 4, Add 9 o Power."),
    R("Pattern", "Up, Down, Up Down, Alberti, Pedal, Bounce, Gallop, Trill y otros."),
    N("CON EL SECUENCIADOR",
      "Grabando en tiempo real se graba lo que toca el Note Engine, no las teclas. Grabando por "
      "pasos, en los modos Auto cada tecla escribe su acorde entero en el paso."),
};

constexpr Block kSeq[] = {
    P("La FM-1 guarda 16 patrones de hasta 64 pasos, que suenan con el preset que tengas elegido. El secuenciador "
      "hay que encenderlo."),
    PANEL(cPlay | cSeq | cRec,
          "PLAY mantenido un segundo enciende o apaga el secuenciador. SEQ muestra el patrón. REC "
          "graba."),
    N("SEQ NO LO APAGA",
      "SEQ solo enseña el patrón. Para encender o apagar el secuenciador, mantén PLAY. Con él "
      "encendido, SEQ parpadea a cada tiempo."),
    H("Dos vistas"),
    R("Lane",
      "La de fábrica. 16 pasos por página y un carril por parte. FM y VA: Notes y Locks. 8-Bit: Lead, Bass, "
      "SFX, Drums y Locks."),
    R("Flow", "Los 64 pasos como puntos, en ocho filas de ocho. SELECT va de paso en paso."),
    P("Se elige en GLOBE, Sequencer View. En Lane, SELECT baja por los carriles, ALGORITHM pasa de página y SEL abre "
      "el carril resaltado. Dentro de un carril, ALGORITHM mueve el cursor paso a paso y SEL abre la lista del paso."),
    H("Grabar por pasos"),
    S("1", "SEQ, lleva SELECT a un carril y ábrelo con SEL."),
    S("2", "REC: aparece un contorno rojo en un paso."),
    S("3", "ALGORITHM lleva el contorno al paso que quieras."),
    S("4", "Toca una tecla o un acorde: se añade, y al soltar el contorno avanza."),
    S("5", "REC para terminar."),
    R("Quitar una nota", "Toca la misma nota en el paso donde empieza. Las teclas siempre añaden, nunca sustituyen."),
    R("Quedarse en el paso", "Mantén SEL mientras tocas."),
    R("Nota larga", "Mantén REC mientras tocas: la nota llega al paso siguiente."),
    R("Batería (8-Bit)",
      "En el carril Drums, cada tecla pone o quita ese tambor en el paso, y el contorno no se mueve."),
    H("Grabar en tiempo real"),
    P("PLAY, y luego REC: el contorno rojo va con el paso que suena. Cada nota cae en el paso más cercano y se oye "
      "desde la vuelta siguiente. Para borrar una nota, mantén SEL y su tecla durante una vuelta entera. El "
      "metrónomo y la cuenta previa están en GLOBE."),
    H("La lista de un paso"),
    P("SEL sobre el paso del cursor abre sus ajustes, parado o sonando:"),
    R("Length", "La figura del paso, de 1/1 a 1/32T."),
    R("Ratchet", "2, 3 o 4 golpes dentro del paso."),
    R("Gate", "Qué parte del paso suena."),
    R("Chance", "En cuántas vueltas suena, de siempre a 5 de cada 100."),
    R("Transpose", "De -24 a +24 semitonos, solo este paso."),
    R("Accent", "Todas sus notas a velocidad máxima."),
    R("Tie & Slide", "Sujeta las notas hasta el paso siguiente; en un preset mono con glide, desliza."),
    R("Copy, Paste, Clear", "Copiar, pegar y vaciar el paso. Clear Step no pregunta."),
    H("Bloqueos"),
    P("Grabando por pasos, con el contorno rojo en un paso, mantén SEL y gira un KNOB: ese ajuste queda fijado en ese "
      "paso (por ejemplo «Lock Feedback 7»). Hasta cuatro por paso. Se pueden bloquear los ocho parámetros del banco "
      "Preset y los tres valores de cada efecto."),
    H("El patrón"),
    P("SEQ dos veces abre sus ajustes: Steps (1 a 64), Timing (Rate, Swing, Gate), Transpose, Chain, Repeats, Preset, "
      "copiar y pegar, Time Multiplier, Copy Repeater y Clear Pattern."),
    R("Chain", "Qué patrón viene después, o Repeat para repetirse. Con Repeats, cuántas veces antes de pasar."),
    R("Preset", "Un preset propio para el patrón: se carga solo al llegar a él."),
    R("Time Multiplier", "Estira al doble o encoge a la mitad, cambiando los pasos o solo la velocidad."),
    R("Copy Repeater", "Escribe el patrón otra vez a continuación."),
    R("Elegir patrón", "SEL + PRESETS, desde cualquier pantalla. Sonando, espera al final del patrón actual."),
};

constexpr Block kGlobal[] = {
    H("GLOBE"),
    P("GLO abre los ajustes de toda la FM-1. SAVE los guarda."),
    R("Tempo y Sync",
      "De 30 a 300. Con Sync activado, el tempo lo marca el reloj MIDI que entra y la fila Tempo se "
      "apaga."),
    R("MIDI", "Canales, Bluetooth, reloj, control local y salida."),
    R("Metronome, Count-in", "Clic al grabar en tiempo real, y un compás de cuenta antes de grabar por pasos."),
    R("Sequencer View", "Lane o Flow."),
    R("Keyboard", "Velocity (las teclas no miden la fuerza: aquí se fija) y rango de bend."),
    R("Glide", "Glide Mode y Glide Time. Solo con el preset en Mono."),
    R("Drive", "0 dB o -6 dB. A -6 dB los presets fuertes y los acordes saturan menos."),
    R("Theme", "Siete colores."),
    R("Reset Bank y About", "Restaurar un banco, y la versión."),
    H("MIDI"),
    P("Entra por USB, por Bluetooth y por el minijack. Sale por USB y por Bluetooth: el minijack es solo de entrada."),
    R("MIDI Channel", "Notas, bend, cambios de programa y controles del panel. All, o del 1 al 16."),
    R("FX Channel", "Los CC 0 a 23 manejan los efectos. De fábrica, el canal 2."),
    R("BT Mode",
      "Off, Device (la FM-1 se anuncia y se empareja desde un móvil u ordenador) o Host (se conecta ella "
      "a un teclado Bluetooth)."),
    R("Clock Out", "Off, Always o Playing."),
    R("Local Control", "En Off, las teclas envían MIDI pero no suenan dentro."),
    N("RUEDA MUDA",
      "Si la rueda de modulación o el pedal de tu teclado no hacen nada, es que envía por el FX Channel. "
      "Cambia uno de los dos canales."),
    H("Parches de DX7"),
    P("Un parche suelto recibido por MIDI sustituye al preset elegido al momento; no queda guardado hasta SAVE. Un "
      "banco de 32 pregunta antes y sustituye el banco entero. Haz copia antes."),
    H("Device Manager"),
    P("Una página web (Chrome o Edge en ordenador) que lee los 128 presets y los 16 patrones y permite ordenarlos, "
      "editarlos, guardar copias e instalar paquetes. Nada cambia en la FM-1 hasta que pulsas Send, y antes de "
      "enviar ofrece guardar una copia."),
    R("Back up everything", "Dos archivos: presets y patrones."),
    R("Studio", "Edita presets de los tres motores y patrones, oyéndolos en el ordenador."),
    R("Archivos", "Abre .syx de FM-1 y de DX7, y guarda o abre patrones como .mid."),
    H("Si algo no responde"),
    R("No cambia el tempo", "Sync está activado."),
    R("Glide Time apagado", "El preset no está en Mono."),
    R("SEQ muestra 64 puntos", "Sequencer View está en Flow."),
    R("PRESETS cambia de patrón", "Tienes SEL mantenido."),
    R("ALGORITHM no hace nada", "En Flow se mueve SELECT; grabando en tiempo real no hay cursor: REC para salir."),
    R("SAVE no hace nada", "Hay un banco de 32 recibido esperando respuesta."),
    R("El móvil no la ve", "BT Mode está en Host. Ponlo en Device."),
    H("Chuleta"),
    R("Moverse", "SELECT recorre, ALGORITHM cambia, SEL entra, HOME sale."),
    R("Guardar todo", "SAVE."),
    R("Banco de mandos", "HOME + SELECT."),
    R("Envolvente sí/no", "ENV mantenido."),
    R("Secuenciador sí/no", "PLAY mantenido."),
    R("Patrón siguiente", "SEL + PRESETS."),
    R("Transponer", "OCT + ALGORITHM."),
};

constexpr Chapter kChapters[] = {
    FMGUIDE_CHAPTER("Qué es FM-1+VA", kWhat),
    FMGUIDE_CHAPTER("Instalar y volver", kInstall),
    FMGUIDE_CHAPTER("Moverse: listas y botones", kMove),
    FMGUIDE_CHAPTER("Los cuatro mandos", kKnobs),
    FMGUIDE_CHAPTER("Presets", kPresets),
    FMGUIDE_CHAPTER("Los tres motores", kFm),
    FMGUIDE_CHAPTER("Efectos y Note Engine", kFx),
    FMGUIDE_CHAPTER("El secuenciador", kSeq),
    FMGUIDE_CHAPTER("Ajustes, MIDI y chuleta", kGlobal),
};

}  // namespace

const Guide kGuideBaud = {
    "FM-1+VA", "La FM-1 de siempre, con dos motores más, efectos y secuenciador.", "V 102",
    kChapters, static_cast<int>(sizeof(kChapters) / sizeof(kChapters[0])),
};

}  // namespace fmguide

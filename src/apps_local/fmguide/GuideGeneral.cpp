#include "GuideData.h"

// ANTES DE NADA: what holds for every community firmware -- the panel, which
// one to try, what to do before installing, moving between them and getting
// back. From each project's own documentation and the community directories,
// as they stood on 16 November 2026.

namespace fmguide {

namespace {

constexpr Block kPanel[] = {
    P("La FM-1 de M-VAVE es un sintetizador FM de bolsillo: seis operadores, 32 algoritmos, 128 presets, altavoz, "
      "batería, USB, entrada MIDI por minijack y Bluetooth. En otoño de 2026 la comunidad aprendió a cargarle su "
      "propio código y hoy hay más de cien firmwares distintos. M-VAVE lo ha aceptado y mantiene un proyecto "
      "abierto, FM-1 Open, para quien quiera programar."),
    P("Todos esos firmwares usan los mismos mandos, pero cada uno les da un trabajo distinto. Por eso conviene "
      "aprenderse el panel por el nombre que lleva impreso: es el nombre que usan todas las guías."),
    PANEL(cAll,
          "El panel, de izquierda a derecha: cuatro mandos y los dos botones de octava; la pantalla; los cuatro "
          "mandos KNOB y dos filas de seis botones; debajo, las 27 teclas."),
    H("Los mandos de la izquierda"),
    R("MASTER", "El volumen. Es lo único que hace lo mismo en todos los firmwares."),
    R("SELECT", "Suele moverse por páginas o listas; en algunos es el tempo."),
    R("PRESETS", "Suele elegir el sonido o el patrón."),
    R("ALGORITHM",
      "En el firmware original cambia el algoritmo FM. En los de la comunidad suele elegir la pista o la "
      "parte, o cambiar el valor resaltado."),
    R("OCT- y OCT+", "Octava abajo y arriba. Casi todos les dan un segundo uso: confirmar, deshacer, pasar de página."),
    H("Los mandos de la derecha"),
    R("KNOB 1 a 4", "Los cuatro valores que la pantalla muestra en ese momento. Cambian con cada página."),
    R("FX, SEL, ENV, LFO, EDIT, GLO", "La fila de arriba. SEL es el segundo botón; el manual de Felucca lo llama SCL."),
    R("HOME, SAVE, ARP, SEQ, PLAY, REC", "La fila de abajo. PLAY y REC arrancan y graban en todos."),
    H("Las teclas"),
    KEYS(kAllWhite | kAllBlack,
         "Dieciséis teclas blancas, de Fa3 a Sol5, y once negras. En las guías se numeran así: blancas 1 a 16 y "
         "negras 1 a 11, siempre de izquierda a derecha."),
    P("Con el firmware original las teclas no responden a la fuerza. Casi todos los de la comunidad dan las notas "
      "fuertes o suaves con un botón mantenido, con un ajuste o por MIDI desde otro teclado. JIANT FM es la excepción: "
      "lee la pastilla que hay bajo cada tecla y saca de ahí la fuerza y la presión."),
    N("TOCAR Y MANTENER",
      "Casi todos los firmwares distinguen entre tocar un botón (abre su página) y mantenerlo (abre una capa: mientras "
      "lo sujetas, teclas y mandos hacen otra cosa). Si algo no responde como esperas, prueba la otra forma."),
};

constexpr Block kChoose[] = {
    P("Estas guías cubren cinco de los firmwares mejor valorados por la comunidad a noviembre de 2026. No compiten: "
      "cada uno convierte la FM-1 en un instrumento distinto, y cambiar de uno a otro lleva unos minutos."),
    H("En una frase"),
    R("SLOOP",
      "Una caja de ritmos para tocar en directo: tres sintes y una batería, grabas tocando y el bucle se "
      "cierra solo. El más inmediato."),
    R("Felucca",
      "Un sinte de cuatro pistas con trece motores y un secuenciador muy completo. Es la base de casi todos "
      "los demás. El más profundo."),
    R("FM-1+VA",
      "La FM-1 de siempre, mejorada: sigue siendo FM y añade un motor analógico virtual, uno de 8 bits, "
      "efectos y un secuenciador serio. El menos arriesgado."),
    R("X0X", "Una 909, una 808, dos 303 y un reproductor de breaks sonando a la vez. Para ácido, techno y big beat."),
    R("JIANT FM",
      "Seis pistas, doce motores, una voz que canta y un generador que escribe temas enteros por género. El más "
      "grande, y el que más propone."),
    H("Cómo se parecen"),
    FLOW("FELUCCA>SLOOP|FELUCCA>X0X|FELUCCA>JIANT FM|ORIGINAL V15>FM-1+VA",
         "SLOOP, X0X y JIANT FM nacen de Felucca: se instalan de forma parecida y comparten combinaciones de rescate y "
         "costumbres. FM-1+VA modifica el firmware original de M-VAVE en lugar de sustituirlo."),
    H("Cuál probar primero"),
    B("Si quieres seguir teniendo tus presets FM y tu forma de tocar: FM-1+VA. Conserva presets y patrones."),
    B("Si quieres hacer un tema entero sin ordenador y sin leer mucho: SLOOP."),
    B("Si quieres diseñar sonido y programar pasos con detalle: Felucca."),
    B("Si quieres bombo, bajo ácido y breaks desde el primer minuto: X0X."),
    B("Si quieres que la FM-1 te proponga un tema entero y luego retocarlo, con seis pistas: JIANT FM."),
    H("Datos de cada uno"),
    R("SLOOP 2.8.5", "Gratis, código abierto (GPL-3.0). Autor: 3dSam. 4 pistas, 12 motores, 153 sonidos."),
    R("Felucca 1.8.2",
      "Gratis, código abierto (GPL-3.0). Autor: Leo Kuroshita, Hügelton Instruments. 4 pistas, 13 "
      "motores, 8 voces."),
    R("FM-1+VA 102", "Gratis, código cerrado. Autora: Baud Girl. 128 presets en tres motores, 16 patrones."),
    R("X0X 1.2.0",
      "Gratis, código abierto (GPL-3.0). Autor: Charles Vestal. 5 partes, 32 patrones, canción de 192 "
      "compases. Se declara en beta."),
    R("JIANT FM 10.7",
      "Gratis, código abierto (GPL-3.0). Autor: Juan Jiant. 6 pistas, 12 motores, 6 proyectos de 4 "
      "secciones."),
    N("VERSIONES",
      "Estos proyectos publican versiones nuevas cada pocos días. Antes de instalar, mira en la página de "
      "cada uno cuál es la actual: algún botón puede haber cambiado de sitio respecto a esta guía."),
};

constexpr Block kBefore[] = {
    P("Instalar un firmware de la comunidad es sencillo y casi siempre sale bien, pero la FM-1 tiene una sola memoria "
      "para el programa y ningún botón de recuperación. Si una instalación se corta, volver puede costar trabajo. Diez "
      "minutos de preparación evitan casi todos los sustos."),
    H("Lo que necesitas"),
    B("Un ordenador con Chrome o Edge. Safari no puede usar MIDI desde la web, y con Firefox el instalador de "
      "FM-1+VA pierde la conexión a mitad. FM-1+VA también se instala desde Chrome en Android."),
    B("Un cable USB de datos, conectado directamente, sin concentrador. Un cable solo de carga no transporta MIDI y "
      "el instalador no verá la FM-1."),
    B("La batería de la FM-1 cargada, y el portátil enchufado."),
    B("Ningún otro programa usando la FM-1: cierra el DAW, el actualizador de M-VAVE y otras pestañas con MIDI."),
    H("Antes de pulsar Instalar"),
    S("1",
      "Actualiza primero el firmware original a V15 o V16 con el actualizador de M-VAVE. FM-1+VA rechaza V13 y "
      "V14."),
    S("2",
      "Guarda lo que tengas. Con el firmware original, haz copia de tus presets; con uno de la comunidad, usa su "
      "copia de seguridad."),
    S("3", "Descarga y guarda a mano el firmware original (FM-1.fwsc, de la web de M-VAVE). Es tu camino de vuelta."),
    S("4", "Apunta tus ajustes globales: canal MIDI, tempo, tema. Algunos instaladores los devuelven a fábrica."),
    S("5",
      "Pulsa Instalar y no toques nada. La pantalla puede quedarse negra medio minuto: es normal. No desenchufes "
      "ni apagues."),
    N("FM-1 NUEVAS",
      "Las unidades vendidas desde otoño de 2026 llevan otro chip de memoria (UC25HQ80). Un firmware que no lo "
      "reconoce pierde lo guardado al apagar y no puede actualizarse. SLOOP 2.8.5, Felucca 1.8.2 y JIANT FM desde la "
      "V10.6z sí lo reconocen: no instales versiones anteriores en una FM-1 reciente."),
    N("POR TU CUENTA",
      "Ninguno de estos proyectos es de M-VAVE. Todos avisan de que se instalan bajo tu "
      "responsabilidad, y X0X se declara en beta."),
};

constexpr Block kSwitch[] = {
    P("Cada firmware guarda sus cosas en la misma memoria de la FM-1, cada uno a su manera. Al instalar otro, lo que "
      "tenía el anterior deja de ser legible, y al volver empiezas con un proyecto vacío. Lo que sí se conserva entre "
      "versiones del mismo firmware son tus proyectos: actualizar no borra nada."),
    H("Guardar antes de cambiar"),
    R("SLOOP", "Editor web, Projects, Backup: un archivo con todo (música, secciones, presets, muestras y ajustes)."),
    R("Felucca",
      "El instalador web hace copia al volver al original; el editor web guarda y restaura todo salvo el "
      "banco de voces FM6, del que hay que conservar el archivo .syx."),
    R("FM-1+VA", "Device Manager, Library, Back up everything: dos archivos, presets y patrones."),
    R("X0X",
      "Página de backup: un archivo .x0b con sonidos, patrones, canción y ajustes. Los breaks propios no van "
      "dentro: guarda sus WAV."),
    R("JIANT FM",
      "Editor web, Projects, Save backup: un archivo con todo salvo el banco de voces FM6. Su instalador hace "
      "además una copia al volver al original."),
    H("El orden seguro"),
    FLOW("FIRMWARE A>COPIA>ORIGINAL V15>FIRMWARE B",
         "Mucha gente pasa por el firmware original entre dos firmwares de la comunidad. No es obligatorio, pero es "
         "la costumbre más extendida y la que menos sorpresas da."),
    P("Ninguna documentación promete que se pueda instalar un firmware de la comunidad directamente encima de otro. "
      "A veces funciona, pero si algo se comporta raro tras el cambio, vuelve al original y reinstala desde ahí."),
    N("FM-1+VA",
      "Solo se instala sobre el V15 original o sobre una versión anterior de sí mismo. Para llegar a él "
      "desde otro firmware de la comunidad, vuelve primero al original."),
    N("JIANT FM",
      "Solo se instala sobre el V16 o el V15 original, o sobre sí mismo. Su manual avisa de que escribirlo "
      "directamente encima de otro firmware de la comunidad ya ha dejado alguna FM-1 sin arrancar."),
    P("Al volver al firmware original después de usar otro, puede arrancar con ajustes extraños o la pantalla "
      "revuelta: está leyendo datos que dejó el otro. No hay nada roto. Un restablecimiento de fábrica desde el propio "
      "firmware de M-VAVE lo arregla."),
};

constexpr Block kRescue[] = {
    H("Volver al firmware original"),
    R("Con M-UPGRADE",
      "El actualizador de M-VAVE (m-vave.com, Downloads) con el archivo FM-1.fwsc, V16 o V15. Es el "
      "camino que recomienda SLOOP. Cierra antes cualquier otro programa con MIDI."),
    R("Desde el instalador",
      "Felucca, X0X y JIANT FM tienen en su página web un botón para volver al original; en FM-1+VA se "
      "elige «Install a file from my computer» y se le da el archivo de M-VAVE."),
    H("Combinaciones de rescate"),
    P("SLOOP, X0X y JIANT FM documentan estas combinaciones. Felucca comparte la del modo actualización y tiene la "
      "calibración en su menú (HOME mantenido, SYSTEM, CALIBRATION). FM-1+VA no las usa."),
    PANEL(cOct, "Los dos botones de octava, a la izquierda, son la puerta de emergencia."),
    R("OCT- y OCT+ juntos, 5 s",
      "Con la música parada: modo actualización. Sale una cuenta atrás; suelta para "
      "cancelar. Desde ahí el instalador web puede volver a escribir."),
    R("OCT- y OCT+ al encender",
      "Calibración: la pantalla te pide pulsar cada botón y girar cada mando. Úsala si un "
      "botón hace lo que no debe."),
    R("OCT- solo al encender", "En SLOOP, rescate por USB: arranca lo justo para reinstalar desde el navegador."),
    H("Si X0X se cuelga"),
    P("X0X muestra una pantalla roja y se reinicia solo. Si se cuelga dos veces seguidas al arrancar, entra en modo "
      "seguro: no suena, pero el USB funciona y el instalador puede reinstalar o devolver el firmware original."),
    H("Si no enciende"),
    S("1",
      "Prueba otro cable de datos, cierra todo lo que use MIDI y lanza de nuevo el instalador. Una instalación "
      "cortada suele dejar la FM-1 esperando en modo actualización."),
    S("2",
      "Mira si el ordenador la ve como un dispositivo USB llamado WL80UBOOT. Es el modo de arranque del propio "
      "chip: desde ahí todavía se puede recuperar."),
    S("3",
      "Para ese caso la comunidad tiene herramientas no oficiales (MvaveFM1Unbricker, jl-uboot-tool, M-Flasher) "
      "que reescriben la memoria por USB. Lee sus avisos antes de usarlas."),
    N("PIDE AYUDA",
      "El foro r/MVaveFM1 es donde se publican las recuperaciones y donde responden los autores. Di qué "
      "firmware y versión tenías, y qué hiciste justo antes."),
};

constexpr Chapter kChapters[] = {
    FMGUIDE_CHAPTER("La FM-1 por fuera", kPanel),        FMGUIDE_CHAPTER("Qué firmware elegir", kChoose),
    FMGUIDE_CHAPTER("Antes de instalar", kBefore),       FMGUIDE_CHAPTER("Cambiar de uno a otro", kSwitch),
    FMGUIDE_CHAPTER("Volver atrás y rescates", kRescue),
};

}  // namespace

const Guide kGuideGeneral = {
    "ANTES DE NADA", "El panel, cuál elegir, cómo instalar sin sustos y cómo volver.", "NOV 2026",
    kChapters,       static_cast<int>(sizeof(kChapters) / sizeof(kChapters[0])),
};

}  // namespace fmguide

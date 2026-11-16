#include "MysteryCore.h"

// The cases. Prose is Spanish in UTF-8 and stays inside Latin-1, which is what
// the reading serif carries; names, roles and titles go on buttons set in the
// display face, so they are ASCII capitals. host-tests/mystery holds every
// text to the length its screen can show.

namespace mystery {

namespace {

constexpr Case kCaseList[kCases] = {
    {
        "LA CATA FINAL",
        "Bodega Ferrer, en el Penedès. La cena de fin de vendimia acabó a las diez. A las once, el capataz encontró "
        "a Ramón Ferrer, el dueño, muerto en la sala de barricas: un golpe en la cabeza y la puerta cerrada con "
        "llave. Nadie ha salido de la finca. Cuatro personas tenían algo pendiente con él esta noche.",
        {
            {"ELVIRA FERRER",
             "HERMANA Y SOCIA",
             "Hermana de Ramón y dueña de media bodega. Lleva las cuentas. Seca, cansada, con las manos manchadas "
             "de harina.",
             "Paquita, la cocinera, jura que Elvira no salió de la cocina entre las diez y las once. Clara las vio "
             "a las dos por la ventana. Y Elvira no tiene llave de la sala.",
             {{"¿Dónde estuvo después de la cena?",
               "En la cocina, con Paquita, preparando las cocas de mañana. No salí hasta que oí los gritos de Tomás. "
               "Pregúntele a ella: no me quitó ojo, dice que amaso mal.",
               -1, -1},
              {"¿Se llevaba bien con su hermano?",
               "Regular. Yo quería vender: hay una oferta buena y estoy harta de deudas. Ramón decía que antes "
               "muerto. Ya ve qué frase. Pero una no mata a su hermano por una firma.",
               -1, 5},
              {"¿Quién entraba en la sala de barricas?",
               "Ramón, a todas horas. Bruno, para sus muestras. Yo no bajo nunca: la humedad me destroza la espalda. "
               "Ni llave tengo.",
               -1, -1}}},
            {"BRUNO SALA",
             "ENOLOGO",
             "Enólogo de la casa desde hace seis años. Sus vinos han ganado medallas. Habla despacio y no deja de "
             "limpiarse las gafas.",
             "",
             {{"¿Dónde estuvo después de la cena?",
               "En el laboratorio, de diez a once, con los análisis del mosto. Es la semana más importante del año. "
               "No vi a nadie y nadie me vio, supongo: estaba trabajando.",
               -1, -1},
              {"¿Qué pasa con el lote 14?",
               "¿Ha leído ese informe? Es un error del laboratorio de fuera, pasa a menudo. Ramón se alarmó por "
               "nada. Íbamos a hablarlo mañana, con calma. No esta noche. Mañana.",
               0, -1},
              {"A las diez y media su laboratorio estaba a oscuras.",
               "Estaría mirando por el microscopio; a veces apago para ver mejor. ¿Quién dice eso, la compradora? "
               "Esa mujer no distingue un laboratorio de un almacén.",
               2, -1}}},
            {"CLARA MIR",
             "COMPRADORA",
             "Compradora de una distribuidora de Barcelona. Ha venido a cerrar un contrato grande. Elegante, con "
             "prisa, el móvil siempre en la mano.",
             "El registro de su móvil muestra una llamada de cuarenta minutos desde el aparcamiento. No tiene llave "
             "de la sala, y con Ramón muerto pierde su contrato.",
             {{"¿Dónde estuvo después de la cena?",
               "En mi coche, en el aparcamiento, hablando con mi jefe. Cuarenta minutos de llamada; mire el "
               "registro si quiere. Volví al oír el alboroto.",
               -1, -1},
              {"¿Qué vio desde el aparcamiento?",
               "Poca cosa. La cocina encendida, con dos mujeres dentro. El patio lleno de gente cantando. Y el "
               "laboratorio a oscuras: me fijé a las diez y media, pensé que el enólogo ya se habría ido a dormir.",
               -1, 2},
              {"¿Qué negocio tenía con Ramón?",
               "Quería treinta mil botellas de su reserva. En la cena me dijo que quizá no podía venderme ni una, "
               "que antes tenía que «aclarar una cosa del vino». Estaba furioso, y no conmigo.",
               -1, -1}}},
            {"TOMAS ROIG",
             "CAPATAZ",
             "Capataz. Treinta vendimias en la casa. Fue quien encontró el cuerpo. Todavía le tiemblan las manos.",
             "Veinte vendimiadores cantaron con él en el patio hasta las once. Su llave abre la sala, pero él no se "
             "movió de allí.",
             {{"¿Dónde estuvo después de la cena?",
               "En el patio, con la cuadrilla. Veinte personas cantando y yo en medio con la guitarra. A las once "
               "bajé a buscar a Ramón para el brindis y me lo encontré así.",
               -1, -1},
              {"¿Quién podía entrar en la sala?",
               "La cerré yo a las diez, como siempre. Solo hay tres llaves: la de Ramón, que la llevaba encima; la "
               "mía, que no ha salido de mi bolsillo; y la de Bruno, para sus muestras. Estaba cerrada cuando bajé.",
               4, 1},
              {"¿Cómo estaba Ramón estos días?",
               "Raro. Bajaba solo a probar las barricas, una por una, y apuntaba cosas. Ayer me preguntó si en "
               "primavera había entrado algún camión cisterna de noche. Le dije que yo no había visto ninguno.",
               -1, -1}}},
        },
        {
            {"SALA DE BARRICAS",
             "Fresco y silencio. Ramón cayó junto a la barrica 14, que tiene el tapón quitado. En el suelo, una "
             "pipeta de cata rota y un charco de vino todavía húmedo. Sacaba una muestra cuando lo golpearon por "
             "detrás. No hay ventanas: solo la puerta.",
             4},
            {"DESPACHO",
             "Sobre la mesa, una carpeta abierta. Dentro, el informe de un laboratorio de Vilafranca: «El lote 14 "
             "no corresponde a la añada ni a la variedad declaradas». Al margen, con letra de Ramón: «Hablar con "
             "B. esta noche».",
             0},
            {"LABORATORIO",
             "Todo en orden, quizá demasiado. El cuaderno de análisis se detiene a las 21:40, antes de la cena. Las "
             "probetas de hoy están sin etiquetar. En el perchero, una bata con el puño húmedo; huele a vino tinto.",
             3},
        },
        {"Un informe dice que el lote 14 no es el vino que pone la etiqueta. Ramón iba a hablar con «B.» esta noche.",
         "Solo hay tres llaves de la sala: la de Ramón, la de Tomás y la de Bruno.",
         "A las diez y media el laboratorio estaba a oscuras.",
         "El cuaderno del laboratorio se detiene a las 21:40. Hay una bata con el puño mojado de vino.",
         "Ramón sacaba una muestra de la barrica 14 cuando lo golpearon por detrás.",
         "Elvira quería vender la bodega y Ramón se negaba."},
        6,
        1,
        0,
        1,
        2,
        "Bruno llevaba años rellenando la reserva con vino comprado a granel. Ramón lo descubrió con el informe y "
        "bajó a sacar una muestra de la barrica 14. Bruno, que decía estar en un laboratorio que estaba a oscuras, "
        "abrió con su llave, discutieron y lo golpeó. Al salir cerró la puerta, como siempre. La costumbre lo delató.",
    },
    {
        "MAREA BAJA",
        "Club Náutico de Cala Roja. Al amanecer, con la marea baja, un pescador ha visto un cuerpo junto al "
        "pantalán B: Octavio Rius, presidente del club, con un golpe en la nuca. Su velero, el Garbí, está amarrado "
        "a dos pasos. Hoy había asamblea de socios. Anoche, cuatro personas tuvieron algo que decirle.",
        {
            {"MARC SOLER",
             "PATRON DE REGATAS",
             "Patrón del Tramuntana y eterno segundo en las regatas del club. Moreno, impaciente, con la voz ronca "
             "de gritar a su tripulación.",
             "La puerta registró su salida a las 22:25 y Pau apuntó su moto. Octavio llamó a Nuria a las 22:40: "
             "seguía vivo. El dueño del bar confirma que Marc estuvo allí hasta el cierre.",
             {{"¿Dónde estuvo anoche?",
               "Fui al Garbí sobre las diez y diez a hablar con Octavio. Un cuarto de hora, no más. Luego cogí la "
               "moto y me fui al bar de la plaza; allí estuve hasta que cerraron.",
               -1, -1},
              {"¿De qué hablaron?",
               "De la regata del domingo. Me descalificó por una boya que no toqué. Le dije cuatro cosas y él me "
               "dijo otras cuatro. Cuando me fui estaba vivo y de mal humor, que era su estado natural.",
               -1, 3},
              {"¿Vio a alguien al salir?",
               "A Pau en la garita. Y un coche gris en el aparcamiento de la junta, que a esas horas suele estar "
               "vacío. No me fijé en más.",
               -1, -1}}},
            {"NURIA PONS",
             "DUENA DE LA NAUTICA",
             "Dueña de la náutica del puerto y amiga de Octavio de toda la vida. Fue vicepresidenta hasta que "
             "discutieron. Fuma en la puerta, mirando al mar.",
             "No tiene tarjeta del pantalán B desde que dimitió, y el registro no muestra ninguna otra entrada. Fue "
             "ella quien avisó de que las cuentas no cuadraban.",
             {{"¿Dónde estuvo anoche?",
               "En casa, encima de la tienda. Sola. Ya sé que como coartada no es gran cosa, pero es lo que hay.", -1,
               -1},
              {"¿Habló con Octavio?",
               "Me llamó él, a las once menos veinte. Estaba excitado. Me dijo: «Ya sé quién ha sido, Nuria. Mañana "
               "lo cuento en la asamblea». Le pregunté quién y me colgó. Así era él.",
               -1, 5},
              {"¿Por qué dejaron de hablarse?",
               "Porque hace un año le dije que las cuentas del club no cuadraban y me llamó exagerada. Dimití. "
               "Anoche fue la primera vez que me llamaba desde entonces. Sería su manera de darme la razón.",
               -1, -1}}},
            {"PAU FERRAN",
             "VIGILANTE DE NOCHE",
             "Marinero del club y vigilante de noche. Veintitrés años; todo el pueblo lo conoce. Apunta cada ronda "
             "en el libro de guardia con letra de colegio.",
             "Sus rondas están apuntadas hora a hora y coinciden con el registro de la puerta. No ganaba nada con "
             "la muerte de Octavio, que fue quien le dio el trabajo.",
             {{"¿Dónde estuvo anoche?",
               "En la garita y haciendo rondas, como cada noche. A las once pasé por el aparcamiento y a las doce "
               "por los pantalanes. No vi nada raro en el agua, pero la marea estaba alta.",
               -1, -1},
              {"¿Qué apuntó en el libro de guardia?",
               "Lo de siempre. A las diez y veinticinco, sale la moto del señor Soler. A las once, luz en la "
               "oficina y el coche gris de la señora Valls en su plaza. A las doce el coche seguía allí. Me "
               "extrañó, pero no es cosa mía.",
               -1, 2},
              {"¿Quién puede entrar en el pantalán B?",
               "Los socios con amarre allí, con su tarjeta. Y la junta. La puerta guarda la hora de cada tarjeta; "
               "el registro está en el armario de portería, si sabe usted leerlo.",
               -1, -1}}},
            {"IRENE VALLS",
             "TESORERA",
             "Tesorera del club desde hace ocho años. Gestora de profesión. Amable, ordenada, con una carpeta para "
             "cada cosa.",
             "",
             {{"¿Dónde estuvo anoche?",
               "Cerré la oficina a las nueve y me fui a Girona, a casa. Tenía que repasar las cuentas para la "
               "asamblea. No volví al club hasta que me han llamado esta mañana.",
               -1, -1},
              {"¿Qué es Valls Consulting?",
               "Mi gestoría. El club me encarga trabajos y me los paga, todo con factura. Octavio lo sabía de "
               "sobra. Si había marcado algo en rojo sería para preguntármelo, nada más.",
               0, -1},
              {"Su tarjeta abrió el pantalán a las 23:05.",
               "Eso es imposible. Bueno: la tarjeta de tesorería está siempre en el cajón de la oficina. Cualquiera "
               "pudo cogerla. Yo estaba a cuarenta kilómetros.",
               1, -1}}},
        },
        {
            {"CABINA DEL GARBI",
             "La cabina está revuelta. En la mesa de cartas, extractos del banco del club con una docena de pagos "
             "subrayados en rojo, todos a «Valls Consulting». Suman más de noventa mil euros. Encima, una nota: "
             "«Asamblea: enseñar esto».",
             0},
            {"PORTERIA",
             "El registro de la puerta del pantalán B. Después de las nueve solo hay tres tarjetas: la de Octavio a "
             "las 21:50; la de Marc Soler a las 22:10, con salida a las 22:25; y la tarjeta 007, «Tesorería», a "
             "las 23:05.",
             1},
            {"PANTALAN B",
             "Con la marea baja se ve el fondo. Entre las algas, bajo la popa del Garbí, brilla una manivela de "
             "winche. En cubierta falta una. Es pesada, cabe en una mano, y alguien la tiró al agua creyendo que "
             "no volvería a verse.",
             4},
        },
        {"Octavio había marcado noventa mil euros en pagos del club a «Valls Consulting» e iba a enseñarlo en la "
         "asamblea.",
         "La tarjeta de Tesorería abrió el pantalán B a las 23:05.",
         "A las once y a las doce, el coche de Irene Valls seguía en el club.",
         "Marc discutió con Octavio por la regata y se fue a las 22:25.",
         "El arma es una manivela de winche del Garbí, tirada al agua.",
         "A las 22:40 Octavio seguía vivo: llamó a Nuria y dijo que ya sabía quién había sido."},
        6,
        3,
        0,
        1,
        2,
        "Irene llevaba años cobrando del club trabajos que no existían. Octavio lo descubrió y pensaba contarlo en "
        "la asamblea. Ella no se fue a Girona: esperó en la oficina, entró en el pantalán con la tarjeta de "
        "tesorería a las 23:05 y lo golpeó con la manivela del propio Garbí. La tiró al agua con la marea alta. "
        "La marea baja la devolvió.",
    },
    {
        "NOTA FALSA",
        "Auditorio de Vic, ensayo general. Tras el descanso, Aurelio Mas, director de la orquesta de cámara, no "
        "volvió al podio. Lo encontraron en su camerino, desplomado sobre el tocador. Tomaba gotas para el corazón; "
        "alguien le ha dado muchas más de la cuenta. El descanso duró veinte minutos. Cuatro personas pudieron "
        "acercarse a él.",
        {
            {"HUGO MARIN",
             "GERENTE",
             "Gerente de la orquesta. Lleva los contratos, los viajes y los disgustos. Traje arrugado, tres "
             "teléfonos.",
             "El encargado del auditorio confirma que discutió con Hugo en taquilla los veinte minutos. Y el termo "
             "que preparó a las seis estaba bien: Aurelio bebió de él toda la primera parte.",
             {{"¿Dónde estuvo en el descanso?",
               "En taquilla, discutiendo con el encargado del auditorio por la factura de la sala. Veinte minutos "
               "enteros. Él se lo confirmará, por desgracia para los dos.",
               -1, -1},
              {"¿Quién preparó el termo?",
               "Yo, a las seis, en la cafetería: té con limón, como cada ensayo. Aurelio bebió toda la primera "
               "parte y dirigió como un toro. Lo que fuera, se lo echaron después. El termo se quedó en el "
               "camerino durante el descanso.",
               -1, 4},
              {"¿Tenía enemigos?",
               "Tenía treinta músicos. Pero en serio: esta semana estaba cambiando cosas. Me pidió un contrato de "
               "concertino nuevo, para un chico de Viena. Me hizo jurar que no diría nada hasta después del "
               "concierto.",
               -1, -1}}},
            {"LIDIA SANZ",
             "PIANISTA INVITADA",
             "Pianista invitada; toca el concierto de mañana. Joven, brillante, con fama de discutir todos los "
             "tempi.",
             "Simón no se apartó del piano y jura que Lidia no se levantó de la banqueta. En la grabación se la oye "
             "probar notas durante todo el descanso.",
             {{"¿Dónde estuvo en el descanso?",
               "Al piano, en el escenario. No me levanté: el afinador estaba retocando los agudos y yo le iba "
               "diciendo. Pregúntele a él.",
               -1, -1},
              {"¿Discutió con Aurelio?",
               "Claro. Quería el adagio lentísimo, como un funeral. Mire por dónde. Le dije que así no lo tocaba y "
               "me contestó que buscaría a otra. Pero una no envenena a un director por un tempo. Se le sufre, y ya.",
               -1, -1},
              {"¿Qué oyó desde el piano?",
               "Los violines charlando. Alguien que salió por el lado de los camerinos. Y luego algo raro al "
               "afinar, no sabría decirle qué. Aquí lo graban todo: escúchelo usted.",
               -1, -1}}},
            {"BERTA COLL",
             "CONCERTINO",
             "Concertino. Veinte años en el primer atril, los mismos que Aurelio en el podio. Recta como su arco. "
             "No ha soltado el violín.",
             "",
             {{"¿Dónde estuvo en el descanso?",
               "En el escenario, con mi sección. Repasamos un pasaje del segundo movimiento y afinamos. Di el la, "
               "como siempre. Cualquiera de los violines se lo dirá, si es que se fijan en algo que no sean ellos.",
               -1, -1},
              {"¿Sabía que iban a sustituirla?",
               "¿Sustituirme? Veinte años. Veinte. Le he salvado más entradas de las que él ha dado bien. No, no "
               "sabía nada. Y de saberlo se lo habría dicho a la cara, que es como digo yo las cosas.",
               0, -1},
              {"En la grabación, el la lo da el oboe.",
               "Pues lo daría el oboe. Saldría un momento al baño, no llevo la cuenta. ¿Ahora es delito ir al "
               "baño? Me está usted ofendiendo, y a la memoria de Aurelio también.",
               2, -1}}},
            {"SIMON PRAT",
             "AFINADOR",
             "Afinador de pianos del auditorio. Sesenta años, oído absoluto y pocas palabras. Lo ve todo desde "
             "detrás del piano.",
             "Lidia estuvo a su lado todo el descanso, y en la grabación se oye su llave de afinar sin pausa. No se "
             "movió del piano.",
             {{"¿Dónde estuvo en el descanso?",
               "Con el piano, en el escenario, junto a la puerta del pasillo de camerinos. La señorita Sanz no se "
               "movió de la banqueta. Yo tampoco.",
               -1, -1},
              {"¿Quién pasó hacia los camerinos?",
               "Una sola persona en todo el descanso. Pasó a mi espalda, deprisa. No le vi la cara: tenía la cabeza "
               "dentro del piano. Vi el estuche. De violín, forrado de azul. Volvió a los cinco minutos.",
               3, 1},
              {"¿Cómo era Aurelio?",
               "Afinado. Y cruel con quien no lo estaba. Hoy paró tres veces a los primeros violines. A la tercera "
               "dijo: «Señora Coll, esto ya no es lo que era». Lo oyó toda la orquesta.",
               -1, -1}}},
        },
        {
            {"CAMERINO",
             "El termo está sobre el tocador, casi vacío; el té huele amargo. El frasco de las gotas no está en el "
             "neceser. Aparece en la papelera del pasillo, vacío y sin huellas. Al camerino solo se llega por ese "
             "pasillo, que sale del escenario.",
             3},
            {"PODIO",
             "La partitura sigue abierta en el atril. Entre dos páginas, una carta aún sin firmar: «Estimada Berta: "
             "tras veinte años, la orquesta necesita un nuevo concertino. Este será tu último concierto». La "
             "esquina está doblada: alguien la ha leído ya.",
             0},
            {"CABINA DE GRABACION",
             "Los ensayos se graban enteros. En el descanso se oye charla, sillas, el piano nota a nota. Luego, un "
             "la largo para afinar: es un oboe. El primer atril de los violines no suena en doce minutos.",
             2},
        },
        {"Aurelio iba a despedir a Berta tras veinte años. La carta estaba en su partitura y alguien la había leído.",
         "Una sola persona fue hacia los camerinos en el descanso, con un estuche de violín forrado de azul.",
         "En la grabación del descanso el la lo da el oboe. El primer atril calla doce minutos.",
         "Las gotas se echaron en el termo. El frasco apareció vacío en el pasillo de camerinos.",
         "El termo estaba bien en la primera parte: fue durante el descanso.", ""},
        5,
        2,
        0,
        1,
        2,
        "Berta vio la carta al dejar sus arcos en el podio. Veinte años, y un papel sin firmar. En el descanso "
        "cruzó el pasillo con su estuche azul, vació el frasco en el termo y volvió al atril. Dijo que había dado "
        "el la, pero la grabación no miente: lo dio el oboe. Aurelio bebió su té antes de salir. No llegó al "
        "segundo movimiento.",
    },
    {
        "EL PORTULANO",
        "Archivo de Santa Clara, en Tortosa. A las siete, al acabar su conferencia, la conservadora Irene Castell "
        "abre la vitrina de la sala noble: el portulano de 1439, la pieza mayor de la casa, es ahora una copia. A "
        "las cinco aún estaba allí el auténtico. Mañana viene la prensa. Cuatro personas pasaron hoy junto a la "
        "vitrina.",
        {
            {"IRENE CASTELL",
             "CONSERVADORA",
             "Conservadora del archivo. Encontró el portulano hace cuatro años en un legajo mal catalogado y habla "
             "de él como de un hijo. Pálida y furiosa.",
             "Dio su conferencia de seis a siete en el claustro, delante de sesenta personas, sin moverse del "
             "atril. Si el portulano no aparece, pierde el cargo y el hallazgo de su vida.",
             {{"¿Cuándo vio el portulano por última vez?",
               "A las cinco. Hugo le quitó una mancha de humedad en la mesa de la sala, lo miramos juntos con la "
               "lupa y lo devolví yo a la vitrina. De seis a siete estuve en el claustro, hablando de él.",
               -1, -1},
              {"¿Quién ha tocado hoy la pieza?",
               "Tres personas. Yo. Hugo, que la limpió. Y Marta, la jefa de sala, que cerró la sala noble con llave "
               "cuando bajé al claustro. Nadie más se acerca: lo tengo prohibido.",
               -1, 5},
              {"¿Qué le pasa con Sergi Oms?",
               "Que cree que el hallazgo era suyo, porque el legajo estaba en su sección. Ayer dijo delante de "
               "todos que cualquiera encuentra lo que otro ha ordenado. Pero estuvo en primera fila toda la tarde.",
               -1, 4}}},
            {"HUGO BALLESTER",
             "RESTAURADOR",
             "Restaurador de papel y pergamino, con taller en el barrio viejo. Trabaja para medio obispado. Manos "
             "finas, voz baja, una sonrisa que llega tarde.",
             "",
             {{"¿Qué hizo usted esta tarde?",
               "Vine a las cinco, limpié la mancha, cobré y me fui. A las cinco y media ya estaba en la calle. Tengo "
               "el taller lleno de trabajo, no me quedo a escuchar conferencias.",
               -1, -1},
              {"¿Usa usted un tubo doble de planos?",
               "Como cualquier restaurador. Llevo pergaminos de un lado a otro todos los días. Hoy traía un "
               "facsímil para enseñárselo a un historiador de la casa. ¿A cuál? No recuerdo el nombre.",
               3, -1},
              {"Su taller tiene un aviso de embargo.",
               "Una mala racha, la tiene todo el mundo. ¿Y por eso voy a robar una carta que no se puede vender? "
               "Aunque... siempre hay quien compra sin preguntar. Eso lo sabe cualquiera del oficio.",
               1, -1}}},
            {"MARTA LLOP",
             "JEFA DE SALA",
             "Jefa de sala del archivo. Lleva las llaves, los horarios y la hoja de firmas de la entrada. No se le "
             "escapa un minuto.",
             "Cerró la sala noble a las seis y pasó la hora en la mesa de entrada del claustro, repartiendo "
             "auriculares de traducción. Dos bedeles estuvieron con ella.",
             {{"¿Quién tiene llave de la sala noble?",
               "Yo, y hay una copia en conserjería. La cerré a las seis en punto, cuando Irene bajó. Pero antes "
               "estuvo abierta una hora larga, con gente entrando y saliendo.",
               -1, -1},
              {"¿A qué hora salió Hugo del edificio?",
               "Aquí lo tengo: firmó la salida a las 18:25. No a las cinco y media. Y salió por la puerta del "
               "huerto, que no es por donde entró. Me extrañó, pero iba cargado.",
               3, 2},
              {"¿Notó algo raro en la sala?",
               "Al cerrar, la vitrina tenía el paño de terciopelo mal puesto, con una esquina por fuera. Pensé que "
               "lo habría dejado así el restaurador. No la abrí: no es mi trabajo.",
               -1, -1}}},
            {"SERGI OMS",
             "HISTORIADOR",
             "Historiador del archivo, veinte años en la sección de mapas. El legajo del portulano era de los "
             "suyos. Tamborilea con los dedos sobre la rodilla.",
             "Escuchó la conferencia entera en primera fila, a la vista de la conferenciante y de toda la sala. En "
             "el turno de preguntas hizo dos, y no precisamente amables.",
             {{"¿Dónde estuvo de seis a siete?",
               "En primera fila, tomando notas de todo lo que Irene contó mal. Pregunte a cualquiera. Hasta levanté "
               "la mano dos veces.",
               -1, -1},
              {"¿Envidia usted ese portulano?",
               "Envidio el hallazgo, no el pergamino. Un portulano robado no se puede enseñar: lo conoce todo el "
               "mundo. Para un historiador no vale nada. Para otra clase de gente, sí.",
               -1, -1},
              {"¿Le enseñó Hugo algún facsímil hoy?",
               "¿A mí? No. Ni a mí ni a nadie de la sección, que yo sepa. Lo vi cruzar el pasillo a las seis y "
               "cuarto, con prisa y un tubo grande al hombro. Ni saludó.",
               3, -1}}},
        },
        {
            {"SALA NOBLE",
             "La vitrina, abierta. Dentro, un facsímil: pergamino nuevo, envejecido a mano con un tinte pardo para "
             "imitar el original. De lejos engaña; con lupa, la tinta no ha cuarteado. La cerradura de la sala no "
             "está forzada.",
             0},
            {"PUERTA DEL HUERTO",
             "La cámara del huerto grabó a las 18:20 a un hombre de espaldas saliendo con un tubo doble de planos, "
             "de los que llevan dos rollos. La puerta se abre desde dentro sin llave.",
             3},
            {"TALLER DE HUGO",
             "Pergaminos tensados y olor a cola. En el banco, un frasco de tinte pardo recién usado. En el cajón, "
             "un aviso de embargo por dos meses de alquiler y un número de teléfono de Ginebra apuntado.",
             1},
        },
        {"Lo que hay en la vitrina es un facsímil envejecido a mano: trabajo de un profesional.",
         "El taller de Hugo tiene un aviso de embargo, y un frasco de tinte pardo recién usado.",
         "Hugo dijo que se fue a las cinco y media, pero firmó la salida a las 18:25, por el huerto.",
         "A las 18:20 un hombre salió por el huerto con un tubo doble de planos.",
         "Sergi cree que el hallazgo del portulano era suyo.",
         "Solo tres personas tocaron hoy el portulano: Irene, Hugo y Marta."},
        6,
        1,
        1,
        3,
        2,
        "Hugo trajo en su tubo doble un facsímil que él mismo había envejecido. Mientras limpiaba el portulano hizo "
        "el cambio y esperó en el edificio a que empezara la conferencia. A las 18:20 salió por el huerto con el "
        "auténtico, camino de un comprador de Ginebra que pagaría el embargo. La hoja de firmas dice a qué hora.",
    },
    {
        "VENTISCA",
        "Refugio de Colomers, Pirineo. Dos días de ventisca: nadie ha subido ni bajado. Al amanecer, la guarda ha "
        "encontrado a Gaspar Novell, guía veterano, muerto en el cobertizo de la leña, con un golpe en la sien. La "
        "nieve de la puerta de atrás está pisada. Quedan cuatro personas en el refugio.",
        {
            {"BERTA ARNAU",
             "GUARDA DEL REFUGIO",
             "Guarda del refugio desde hace doce inviernos. Duerme junto a la cocina, con un oído en la estufa. "
             "Pocas palabras, todas exactas.",
             "Su cuarto da al comedor, y Mireia, que no podía dormir, estuvo allí leyendo de cuatro a seis: Berta no "
             "pasó. Fue ella quien dio la alarma por radio.",
             {{"¿Qué oyó esta noche?",
               "El viento, sobre todo. Pero a las cinco y diez sonó el cerrojo de la puerta de atrás: chirría. Una "
               "vez, y al rato otra. Pensé que alguien iba a por leña y me volví a dormir.",
               -1, 0},
              {"¿Cómo se llevaba Gaspar con los demás?",
               "Con Oriol, mal desde el martes; cenaban sin mirarse. Con la doctora discutió por dinero de un viaje "
               "que se canceló. Y al señor Codina lo trataba con mucho cuidado, como quien debe algo.",
               -1, 5},
              {"¿De quién es el frontal del cobertizo?",
               "Del refugio no es: los nuestros son rojos. Ese es de los buenos, de guía. Oriol llevaba uno igual "
               "anoche en la cena, colgado del cuello.",
               3, -1}}},
            {"JULIAN CODINA",
             "CLIENTE",
             "Cliente de Gaspar: quería subir al Montardo. Ingeniero jubilado, de Lleida. Educado, despacio, con una "
             "tristeza vieja en la cara.",
             "Tiene una rodilla operada y no baja la escalera sin ayuda: Mireia tuvo que acompañarlo al comedor a "
             "las seis. Su saco estaba caliente cuando ella lo despertó.",
             {{"¿Qué hizo usted esta noche?",
               "Dormir mal, como siempre en un refugio. Me acosté a las diez. No me moví de la litera hasta que la "
               "doctora vino a buscarme a las seis, con la noticia.",
               -1, -1},
              {"¿Vio a alguien levantarse a las cinco?",
               "Ver, no. Pero me desperté con el cerrojo y miré el reloj: las cinco y diez. La litera de enfrente, "
               "la de Oriol, estaba vacía. Toqué el saco al ir a por agua: frío.",
               0, 2},
              {"Su hermano murió en una expedición de Gaspar.",
               "Hace nueve años, sí. Vine a que me contara cómo fue, no a otra cosa. Anteanoche me lo contó, y me "
               "pidió perdón. Llevaba nueve años esperando eso. No iba a matarlo ahora.",
               4, -1}}},
            {"ORIOL PRAT",
             "GUIA AYUDANTE",
             "Guía joven, tres temporadas con Gaspar. Fuerte, rápido, orgulloso de serlo. Se frota las manos como si "
             "aún tuviera frío.",
             "",
             {{"¿Qué hizo usted esta noche?",
               "Dormir de un tirón, de diez a seis. Estaba reventado de abrir huella ayer. Me despertó el grito de "
               "Berta. No oí nada, ni cerrojos ni nada.",
               -1, -1},
              {"¿Qué pasó el martes en la cresta?",
               "Nada. Un tramo fácil, lo pasamos sin cuerda para ir más rápido, venía mal tiempo. No se cayó nadie. "
               "Gaspar hizo un drama: decía que eso no se le hace a un cliente. Es de otra época.",
               1, -1},
              {"Su litera estaba vacía a las cinco.",
               "Fui al baño. O a por agua, yo qué sé. ¿Quién lleva la cuenta a esas horas? Mi frontal lo perdí "
               "ayer en la huella, así que si han encontrado uno, no prueba nada.",
               2, -1}}},
            {"MIREIA SANS",
             "MEDICA",
             "Médica de urgencias, de vacaciones. Venía a hacer una travesía que Gaspar canceló por el tiempo. "
             "Práctica, cansada, con ojeras de guardia.",
             "Estuvo de cuatro a seis en el comedor, leyendo junto a la estufa: desde allí se ve la puerta del "
             "cuarto de Berta, pero no la de atrás. Sus botas estaban secas.",
             {{"¿Qué puede decir del cuerpo?",
               "Un solo golpe, con algo pesado y romo: un tronco, diría. Murió entre las cinco y las seis. No se "
               "cayó: estaba de espaldas a la puerta, agachado sobre la leña.",
               -1, -1},
              {"¿Qué discutió con Gaspar?",
               "Seiscientos euros. Pagué la travesía por adelantado y la canceló. Me los iba a devolver al bajar; "
               "lo dijo delante de todos. Por seiscientos euros no se mata a nadie, y menos a quien te los debe.",
               5, -1},
              {"¿Vio pasar a alguien por el comedor?",
               "A nadie. Pero la escalera de las literas baja directa al pasillo de atrás, sin pasar por el comedor. "
               "Oí crujir los peldaños poco después de las cinco. Pasos ligeros, de alguien joven.",
               -1, -1}}},
        },
        {
            {"COBERTIZO",
             "Leña apilada y un tronco con sangre. Debajo de la pila, medio escondido, un frontal de guía con las "
             "iniciales O. P. rotuladas en la cinta. En la nieve de la puerta, un solo par de huellas de ida y otro "
             "de vuelta.",
             3},
            {"MOCHILA DE GASPAR",
             "Mapas, una cuerda, un cuaderno. En el cuaderno, el borrador de una carta a la federación: «El martes "
             "mi ayudante Oriol Prat dejó sin cuerda a cuatro clientes en la cresta. No debe guiar». Sin firmar aún.",
             1},
            {"LIBRO DE REGISTRO",
             "El libro del refugio guarda las firmas de veinte años. Alguien ha dejado una hoja doblada en una "
             "página de hace nueve: expedición guiada por G. Novell, cinco nombres. Uno, Andreu Codina, está "
             "subrayado a lápiz.",
             4},
        },
        {"A las cinco y diez sonó dos veces el cerrojo de la puerta de atrás.",
         "Gaspar iba a denunciar a Oriol a la federación por dejar sin cuerda a unos clientes.",
         "A las cinco y diez la litera de Oriol estaba vacía y el saco, frío.",
         "Bajo la leña del cobertizo hay un frontal de guía con las iniciales O. P.",
         "El hermano de Julián murió hace nueve años en una expedición de Gaspar.",
         "Gaspar le debía a Mireia el dinero de una travesía cancelada."},
        6,
        2,
        1,
        3,
        2,
        "Gaspar iba a enviar la carta al bajar: Oriol perdería la licencia. A las cinco, Oriol lo siguió al "
        "cobertizo para convencerlo; Gaspar ni se volvió, y él cogió un tronco. Con las prisas se le cayó el "
        "frontal entre la leña. Dijo que durmió de un tirón, pero su saco estaba frío y el cerrojo sonó dos veces.",
    },
    {
        "MENU CERRADO",
        "Restaurante Can Llop, Cadaqués. En el segundo postre, el crítico Fermín Aguado se ha desplomado sobre la "
        "mesa siete. Está en el hospital y saldrá de esta: alguien le dio un sedante fuerte. Mañana debía publicar "
        "su crítica de la casa. Cuatro personas estuvieron lo bastante cerca de él esta noche.",
        {
            {"CARME LLOP",
             "CHEF Y DUENA",
             "Chef y dueña. Es su segundo restaurante; el primero cerró hace seis años. Brazos cruzados, delantal "
             "impecable, la mandíbula apretada.",
             "No salió de la cocina en todo el servicio: el pase está abierto a la sala y veinte comensales la "
             "vieron emplatar. Los platos de Fermín los llevó el camarero, y dieron limpio.",
             {{"¿Dónde estuvo durante la cena?",
               "En mi cocina, de ocho a once, como cada noche. El pase está abierto: me ve toda la sala. No me "
               "acerqué a la mesa siete; a un crítico no se le hace la pelota.",
               -1, -1},
              {"¿Conocía usted a Fermín de antes?",
               "Me cerró el primer restaurante con una página. «Cocina de catálogo», escribió. Tardé seis años en "
               "levantar este. Hoy quería que se comiera sus palabras, no que se durmiera encima.",
               -1, 4},
              {"¿Quién se acercó a su mesa?",
               "El sumiller, a servir. El otro crítico, que cenaba con él. Y la sobrina, que llegó pronto, dejó "
               "algo en el guardarropa y luego rondó por la barra hasta los postres.",
               -1, -1}}},
            {"ADRIA FONT",
             "SUMILLER",
             "Sumiller de la casa. Sirvió los seis vinos del menú en la mesa siete. Joven, minucioso, con la "
             "servilleta siempre doblada en el brazo.",
             "Descorchó cada botella delante de la mesa y Víctor bebió de las mismas. El laboratorio no ha "
             "encontrado nada en las botellas ni en las copas.",
             {{"¿Qué sirvió usted en la mesa siete?",
               "Seis vinos, seis botellas abiertas allí mismo. El señor Reig bebió de todas y está perfectamente. "
               "Lo único que tomó el señor Aguado y nadie más fue su pastilla, con el agua, antes del postre.",
               -1, -1},
              {"¿Cómo estaban los dos críticos?",
               "Tirantes. Se disputan la misma columna en el diario, dicen. El señor Reig no dejó de pincharle: que "
               "si estaba mayor, que si ya no distinguía un tinto. Pero no se levantó de la silla.",
               -1, 5},
              {"¿A qué hora llegó la sobrina?",
               "Pronto, antes que los entrantes. La vi en el guardarropa con el abrigo de su tío en las manos. "
               "Luego dijo a todo el mundo que acababa de llegar. Me pareció raro y no dije nada.",
               3, -1}}},
            {"LOLA AGUADO",
             "SOBRINA Y AGENTE",
             "Sobrina de Fermín y su agente: le lleva la agenda, los contratos y las medicinas. Simpática, rápida, "
             "mira el móvil cada medio minuto.",
             "",
             {{"¿Cuándo llegó usted al restaurante?",
               "A los postres. Venía a recoger a mi tío: ya no conduce de noche. Entré, lo vi caerse y llamé yo a "
               "la ambulancia. Estoy destrozada, entiéndalo.",
               -1, -1},
              {"¿Quién prepara el pastillero de su tío?",
               "Yo, cada lunes. ¿Y qué? Tiene setenta años y se lía con las cápsulas. Cualquiera pudo abrirlo esta "
               "noche: lo deja siempre en el bolsillo del abrigo.",
               0, 1},
              {"¿Qué son los tres mil euros de Can Tosca?",
               "Una... asesoría. Les ayudé con la carta. Mi tío lo malinterpretó, se ponía imposible con esas "
               "cosas. Íbamos a aclararlo mañana. No hacía falta ningún abogado.",
               2, -1}}},
            {"VICTOR REIG",
             "CRITICO RIVAL",
             "Crítico del mismo diario, veinte años más joven. Cenaba con Fermín por orden del director. Irónico, "
             "bien vestido, algo pálido ahora.",
             "No se levantó de la mesa en toda la cena; lo confirman el sumiller y la mesa de al lado. Nunca tuvo "
             "el pastillero en la mano: Fermín lo sacó del abrigo él mismo.",
             {{"¿Qué pasó en la mesa?",
               "Cenamos, discutimos, lo normal. Antes del segundo postre pidió su abrigo, sacó el pastillero y se "
               "tomó la cápsula de la noche. Diez minutos después se le cerraban los ojos.",
               -1, -1},
              {"¿Es verdad que quiere su columna?",
               "Claro que la quiero. Y la tendré cuando se jubile, que será pronto. No necesito dormirlo para eso: "
               "le basta con seguir escribiendo como escribe.",
               5, -1},
              {"¿Le habló Fermín de su sobrina?",
               "Esta noche, sí. Dijo que alguien vendía críticas con su nombre y que ya sabía quién. «La sangre no "
               "lo tapa todo», dijo. Mañana iba a ver a un abogado, antes de publicar.",
               -1, -1}}},
        },
        {
            {"MESA SIETE",
             "Copas, migas, una servilleta en el suelo. Junto al plato, el pastillero de Fermín, con los días de la "
             "semana. En el hueco de hoy queda media cápsula abierta: el polvo de dentro no es del color de las "
             "otras.",
             0},
            {"MOVIL DE FERMIN",
             "En borradores, un mensaje sin enviar: «Lola: sé lo de los tres mil euros de Can Tosca por una crítica "
             "que yo no he escrito. Mañana a las diez, en el abogado. No me llames».",
             2},
            {"GUARDARROPA",
             "El abrigo de Fermín cuelga en la percha 12. La encargada apunta las horas: la ficha 13, a nombre de L. "
             "Aguado, se dio a las 20:40. Los postres salieron a las 22:30.",
             3},
        },
        {"El sedante estaba en una cápsula del pastillero de Fermín, no en el vino ni en la comida.",
         "Lola rellena el pastillero de su tío cada lunes.",
         "Fermín sabía que Lola cobró tres mil euros por una crítica falsa e iba a ir al abogado.",
         "Lola dijo que llegó a los postres, pero el guardarropa le dio ficha a las 20:40.",
         "Fermín cerró el primer restaurante de Carme con una crítica.",
         "Víctor y Fermín se disputan la misma columna."},
        6,
        2,
        2,
        1,
        3,
        "Lola vendía críticas con la firma de su tío. Él lo descubrió y al día siguiente iba al abogado. Ella llegó "
        "a las 20:40, cambió la cápsula de la noche en el pastillero del abrigo y esperó en la barra: con Fermín "
        "dormido, tendría un día para borrar su rastro. Luego fingió que acababa de llegar. La ficha la desmiente.",
    },
};

}  // namespace

const Case& caseAt(const int index) { return kCaseList[index < 0 ? 0 : (index >= kCases ? kCases - 1 : index)]; }

}  // namespace mystery

#pragma once

// PARLEM, the part with no hardware in it: three hundred phrases for somebody
// starting out in Catalan, the ones a day in Barcelona asks for, each with
// its Spanish and a tip on a word, a sound or a rule. They are sorted into
// sections, and the reader can keep to one.

#include <cstdint>

#include "../dita/DitaCore.h"

namespace parlem {

using dita::dayNumber;
using dita::stepStyle;
using dita::Style;

// Where a phrase is said. Section 0 is all of them; section n is kTopics[n - 1].
inline constexpr const char* kTopics[] = {"SALUDOS",   "CORTESÍA",        "PRESENTARSE",     "BÁSICOS",
                                          "EN EL BAR", "DE COMPRAS",      "POR LA CALLE",    "EL TIEMPO",
                                          "EN CASA",   "SENTIR Y QUEDAR", "TRABAJO Y CLASE", "SALUD"};
constexpr int kTopicCount = sizeof(kTopics) / sizeof(kTopics[0]);
constexpr int kSectionCount = kTopicCount + 1;
constexpr const char* sectionName(const int section) { return section == 0 ? "TODAS" : kTopics[section - 1]; }

struct Phrase {
  uint8_t topic;  // into kTopics
  const char* ca;
  const char* es;
  const char* tip;
};

// One from each topic in turn, so a week of all of them is not seven
// greetings. Straight apostrophes: not every cut on the device has the curly
// ones.
inline constexpr Phrase kPhrases[] = {
    {0, "Bon dia!", "¡Buenos días!", "Vale hasta la hora de comer. Es el saludo estrella."},
    {1, "Si us plau", "Por favor", "Se pronuncia casi «sisplau»."},
    {2, "Em dic Marta", "Me llamo Marta", "«Dic» es «digo»: literalmente, «me digo Marta»."},
    {3, "I tant!", "¡Y tanto! ¡Claro que sí!", "Un «sí» con entusiasmo. Muy catalán."},
    {4, "Un cafè amb llet, si us plau", "Un café con leche, por favor", "«Llet» = leche."},
    {5, "Només estic mirant", "Solo estoy mirando", "«Només» = solo."},
    {6, "On és el metro?", "¿Dónde está el metro?", "«On és...?» sirve para preguntar por cualquier sitio."},
    {7, "Quina hora és?", "¿Qué hora es?", "«Quina» concuerda con «hora». Con un masculino: «quin dia és?»."},
    {8, "Tinc son", "Tengo sueño", "«Son» son las ganas de dormir; «somni», lo que sueñas."},
    {9, "M'agrada molt", "Me gusta mucho", "«Agradar» funciona igual que «gustar»."},
    {10, "Tinc classe de català", "Tengo clase de catalán", "«Classe», con dos eses. «Fer classe» = dar clase."},
    {11, "No em trobo bé", "No me encuentro bien", "«Trobar-se» = encontrarse."},
    {0, "Bona tarda!", "¡Buenas tardes!", "«Tarda» = tarde. Se usa desde después de comer hasta que oscurece."},
    {1, "Gràcies!", "¡Gracias!", "También oirás «merci», muy habitual."},
    {2, "Com et dius?", "¿Cómo te llamas?", "De usted: «com es diu?»."},
    {3, "D'acord", "De acuerdo, vale", "También oirás «vale», pero «d'acord» es lo propio."},
    {4, "Un tallat", "Un cortado", "De «tallar», cortar. Es el cortado de siempre."},
    {5, "Quant val això?", "¿Cuánto vale esto?", "«Quant val?» y «quant costa?» significan lo mismo."},
    {6, "Com puc anar a la plaça?", "¿Cómo puedo ir a la plaza?", "La ç suena como s: plaça, Barça, feliç."},
    {7, "Són les tres", "Son las tres", "A la una: «és la una». Las demás: «són les...»."},
    {8, "Estic cansada", "Estoy cansada", "Un hombre diría «estic cansat»."},
    {9, "No m'agrada gens", "No me gusta nada", "«Gens» = nada de nada."},
    {10, "No he fet els deures", "No he hecho los deberes", "«Deures» = deberes. «Fet» = hecho."},
    {11, "Em fa mal el cap", "Me duele la cabeza", "«Fer mal» = doler: «em fa mal» + lo que duele."},
    {0, "Bona nit!", "¡Buenas noches!", "Sirve para saludar de noche y también para despedirse al ir a dormir."},
    {1, "Moltes gràcies!", "¡Muchas gracias!", "«Molt» concuerda: molt, molta, molts, moltes."},
    {2, "Molt de gust", "Mucho gusto", "Al presentarte también vale «encantada»; un hombre diría «encantat»."},
    {3, "És clar!", "¡Claro!", "«És» (del verbo ser) lleva acento; «es» sin acento es el pronombre «se»."},
    {4, "Una canya", "Una caña", "«ny» es la ñ catalana: canya, any, Catalunya."},
    {5, "És massa car", "Es demasiado caro", "«Massa» = demasiado."},
    {6, "És lluny?", "¿Está lejos?", "«Lluny» = lejos."},
    {7, "Avui fa bon dia", "Hoy hace buen día", "«Avui» = hoy."},
    {8, "Me'n vaig a dormir", "Me voy a dormir", "«Me'n vaig» = me voy."},
    {9, "T'estimo", "Te quiero", "Ojo: «et vull» significa «te deseo»."},
    {10, "Com es pronuncia?", "¿Cómo se pronuncia?", "La pregunta más útil de toda la clase."},
    {11, "Em fa mal la panxa", "Me duele la barriga", "«Panxa» = barriga."},
    {0, "Hola, què tal?", "Hola, ¿qué tal?", "En catalán el signo de interrogación solo va al final."},
    {1, "De res", "De nada", "«Res» = nada."},
    {2, "Soc de Madrid", "Soy de Madrid", "«Soc» ya no lleva acento; antes era «sóc»."},
    {3, "Potser", "Quizás", "Se escribe junto, en una sola palabra. Suena «putsé»."},
    {4, "Una aigua amb gas", "Un agua con gas", "«Aigua» es femenino: «l'aigua freda»."},
    {5, "Teniu una talla més gran?", "¿Tenéis una talla más grande?", "«Gran» = grande; «petit» = pequeño."},
    {6, "És aquí a prop", "Está aquí cerca", "«A prop» = cerca."},
    {7, "Demà plourà", "Mañana lloverá", "«Demà» es el día de mañana; «matí», la mañana."},
    {8, "Què fem per sopar?", "¿Qué hacemos para cenar?", "Esmorzar, dinar, sopar: desayunar, comer, cenar."},
    {9, "Et trobo a faltar", "Te echo de menos", "Literalmente, «te encuentro a faltar»."},
    {10, "Què significa aquesta paraula?", "¿Qué significa esta palabra?", "«Paraula» = palabra."},
    {11, "Tinc febre", "Tengo fiebre", "«Febre» = fiebre."},
    {0, "Com estàs?", "¿Cómo estás?", "De usted: «com està?». A varios: «com esteu?»."},
    {1, "Perdona", "Perdona", "De usted: «perdoni»."},
    {2, "D'on ets?", "¿De dónde eres?", "«On» = dónde."},
    {3, "No ho sé", "No lo sé", "«Ho» = lo. Va delante del verbo: «no ho sé», «no ho entenc»."},
    {4, "Què em recomana?", "¿Qué me recomienda?", "Forma de usted. De tú: «què em recomanes?»."},
    {5, "M'ho puc emprovar?", "¿Me lo puedo probar?", "«Emprovar» es para ropa; «tastar», para comida."},
    {6, "Tot recte", "Todo recto", "«Tot» = todo. «Recte» = recto."},
    {7, "Ahir vaig anar al cinema", "Ayer fui al cine", "El pasado se hace con «vaig» + infinitivo. ¡No es futuro!"},
    {8, "Ja he dinat", "Ya he comido", "«Dinar» es la comida del mediodía."},
    {9, "Estic contenta", "Estoy contenta", "Un hombre diría «estic content»."},
    {10, "Ho pots escriure?", "¿Lo puedes escribir?", "«Escriure» = escribir."},
    {11, "Estic refredada", "Estoy resfriada", "Un hombre diría «refredat». También «constipada»."},
    {0, "Molt bé, gràcies. I tu?", "Muy bien, gracias. ¿Y tú?", "«I» es la «y» castellana."},
    {1, "Ho sento", "Lo siento", "«Ho» es el «lo» neutro: esto, eso."},
    {2, "Visc a Barcelona", "Vivo en Barcelona", "«Visc», de «viure». Suena «visk»."},
    {3, "Tens raó", "Tienes razón", "Tinc, tens, té: tengo, tienes, tiene."},
    {4, "Tenen menú del dia?", "¿Tienen menú del día?", "«Tenen» = tienen (ustedes). De vosotros: «teniu»."},
    {5, "M'ho quedo", "Me lo quedo", "«M'ho» = me lo. Dos pronombres unidos con apóstrofo."},
    {6, "A la dreta", "A la derecha", "«Dreta» = derecha. «Gira a la dreta»: gira a la derecha."},
    {7, "Fa fred", "Hace frío", "El tiempo «se hace»: fa fred, fa calor, fa sol, fa vent."},
    {8, "On són les claus?", "¿Dónde están las llaves?", "«Clau» = llave."},
    {9, "Quina sort!", "¡Qué suerte!", "«Quin» y «quina» también sirven para exclamar: quina sort!, quin fred!"},
    {10, "Tinc un dubte", "Tengo una duda", "«Dubte» es masculino: «un dubte»."},
    {11, "Tinc tos", "Tengo tos", "Igual que en castellano."},
    {0, "Vaig fent", "Voy tirando", "La respuesta más catalana a «com estàs?»."},
    {1, "No passa res", "No pasa nada", "La respuesta natural a «perdona» o a «ho sento»."},
    {2, "Estic aprenent català", "Estoy aprendiendo catalán",
     "«Estic» + gerundio, igual que en castellano: «estic aprenent», «estic mirant»."},
    {3, "Què és això?", "¿Qué es esto?", "«Això» = esto o eso. «Allò» = aquello."},
    {4, "Una taula per a dos", "Una mesa para dos", "«Taula» = mesa."},
    {5, "Vull una barra de pa", "Quiero una barra de pan", "«Vull» = quiero."},
    {6, "A l'esquerra", "A la izquierda", "«Esquerra» = izquierda. «La» se apostrofa ante vocal: l'esquerra."},
    {7, "Fa molta calor", "Hace mucho calor", "«Calor» es femenino: «molta calor»."},
    {8, "Tanca la porta", "Cierra la puerta", "«Tancar» = cerrar."},
    {9, "Quina llàstima!", "¡Qué lástima!", "También se dice «quina pena!»."},
    {10, "Ho he entès", "Lo he entendido", "«Entès» es el participio de «entendre»."},
    {11, "Estic malalta", "Estoy enferma", "«Malalt», enfermo; «malalta», enferma."},
    {0, "Adéu!", "¡Adiós!", "Más cariñoso: «adeu-siau»."},
    {1, "Amb molt de gust", "Con mucho gusto", "«Amb» = con. La b no suena: «am»."},
    {2, "Parlo una mica de català", "Hablo un poco de catalán", "«Una mica» = un poco."},
    {3, "On és el lavabo?", "¿Dónde está el baño?", "«Lavabo» es el baño entero, no solo la pila."},
    {4, "Jo vull el peix", "Yo quiero el pescado", "«Peix» suena «pesh»."},
    {5, "Mig quilo de tomàquets", "Medio kilo de tomates", "«Mig» suena «mich»."},
    {6, "A la cantonada", "En la esquina", "«Cantonada» = esquina."},
    {7, "Plou", "Llueve", "De «ploure», llover. «Està plovent» = está lloviendo."},
    {8, "Obre la finestra", "Abre la ventana", "«Obrir» = abrir."},
    {9, "Tant me fa", "Me da igual", "Literalmente, «tanto me hace»."},
    {10, "Està bé així?", "¿Está bien así?", "«Així» = así. La x suena «sh»."},
    {11, "Necessito un metge", "Necesito un médico", "«Metge», médico; «metgessa», médica."},
    {0, "Fins després!", "¡Hasta luego!", "«Fins» = hasta."},
    {1, "Bon profit!", "¡Buen provecho!", "Se dice al empezar a comer, y también a quien ves comiendo."},
    {2, "Ho entenc, però em costa parlar", "Lo entiendo, pero me cuesta hablar",
     "«Però» = pero, con el golpe de voz en la o."},
    {3, "Quan arribes?", "¿Cuándo llegas?", "«Quan» = cuándo; «quant» = cuánto."},
    {4, "Per a mi, l'amanida", "Para mí, la ensalada", "«Amanida» = ensalada."},
    {5, "Una dotzena d'ous", "Una docena de huevos", "«Ou» = huevo."},
    {6, "M'he perdut", "Me he perdido", "«He» + participio, como en castellano: m'he perdut, he arribat."},
    {7, "Aquest matí", "Esta mañana", "«Matí» es masculino: «aquest matí»."},
    {8, "Tinc feina", "Tengo trabajo", "«Feina» = trabajo."},
    {9, "Tinc ganes de veure't", "Tengo ganas de verte", "«Tenir ganes de» = tener ganas de. «Veure't» = verte."},
    {10, "M'he equivocat", "Me he equivocado",
     "Una mujer también dice «m'he equivocat»: aquí el participio no cambia."},
    {11, "Tinc hora amb el metge", "Tengo cita con el médico", "«Tenir hora» = tener cita."},
    {0, "Fins demà!", "¡Hasta mañana!",
     "«Demà» = mañana. Con «fins» te despides hasta cuando quieras: «fins dilluns»."},
    {1, "Salut!", "¡Salud!", "Para brindar. Al estornudar: «Jesús!» o «salut!»."},
    {2, "Pots parlar més a poc a poc?", "¿Puedes hablar más despacio?", "«A poc a poc» = despacio."},
    {3, "Per què?", "¿Por qué?", "Separado pregunta; junto, «perquè», responde."},
    {4, "Pa amb tomàquet", "Pan con tomate", "El básico de cualquier mesa catalana."},
    {5, "Tens canvi?", "¿Tienes cambio?", "«Canvi» = cambio. La v suena como b."},
    {6, "Un bitllet, si us plau", "Un billete, por favor", "«tll» suena como una ll: «billet»."},
    {7, "La setmana que ve", "La semana que viene", "«Que ve» = que viene. También «l'any que ve»."},
    {8, "Truca'm després", "Llámame luego", "«Trucar» = llamar por teléfono."},
    {9, "Què fas aquest vespre?", "¿Qué haces esta tarde?", "«Vespre» es el rato entre la tarde y la noche."},
    {10, "Torna-ho a dir", "Dilo otra vez", "«Tornar a» + infinitivo = volver a hacer algo."},
    {11, "Vull demanar hora", "Quiero pedir cita", "«Demanar» = pedir."},
    {0, "Fins aviat!", "¡Hasta pronto!", "«Aviat» = pronto."},
    {1, "Felicitats!", "¡Felicidades!",
     "Vale para un cumpleaños y para cualquier buena noticia. También «enhorabona»."},
    {2, "Ho pots repetir, si us plau?", "¿Lo puedes repetir, por favor?", "«Pots» = puedes. «Puc» = puedo."},
    {3, "Hi ha algú?", "¿Hay alguien?", "«Hi ha» = hay. Suena «iá»."},
    {4, "Sense ceba, si us plau", "Sin cebolla, por favor", "«Sense» = sin."},
    {5, "Una bossa, si us plau", "Una bolsa, por favor", "«Bossa» = bolsa. La ss suena como la s castellana."},
    {6, "Quina parada és aquesta?", "¿Qué parada es esta?", "«Quin» y «quina» = qué, delante de un nombre."},
    {7, "Dilluns tinc classe", "El lunes tengo clase", "Dilluns, dimarts, dimecres, dijous, divendres."},
    {8, "Dorm bé!", "¡Duerme bien!", "De «dormir». «Bé» = bien; «bo» = bueno."},
    {9, "Quedem demà?", "¿Quedamos mañana?", "«Quedar» se usa igual que en castellano para citarse."},
    {10, "Tinc una reunió", "Tengo una reunión", "Las palabras en -ió son femeninas: la reunió, la cançó."},
    {11, "On és la farmàcia?", "¿Dónde está la farmacia?", "«Farmàcia», con acento abierto en la a."},
    {0, "Que vagi bé!", "¡Que vaya bien!", "La despedida de siempre al salir de una tienda."},
    {1, "Per molts anys!", "¡Feliz cumpleaños!", "Literalmente, «por muchos años»."},
    {2, "No ho entenc", "No lo entiendo", "«Entenc» = entiendo. La c final suena como k."},
    {3, "No hi ha res", "No hay nada", "«Hi ha» = hay. «No hi ha» = no hay. «Res» = nada."},
    {4, "Està boníssim!", "¡Está buenísimo!", "«-íssim» es el «-ísimo» catalán: boníssim, moltíssim."},
    {5, "Qui és l'últim?", "¿Quién es el último?", "Imprescindible en el mercado y en la panadería."},
    {6, "Baixes a la propera?", "¿Bajas en la próxima?", "Lo oirás en el metro y en el bus."},
    {7, "Ara mateix", "Ahora mismo", "«Mateix» = mismo. La x suena «sh»: «matesh»."},
    {8, "Som-hi!", "¡Vamos allá!", "Sirve para arrancar cualquier cosa."},
    {9, "Em ve de gust un gelat", "Me apetece un helado", "«Venir de gust» = apetecer."},
    {10, "Avui treballo des de casa", "Hoy trabajo desde casa", "«Des de» = desde, en dos palabras."},
    {11, "Tinc al·lèrgia", "Tengo alergia", "El punto volado (l·l) marca una l larga. Solo existe en catalán."},
    {0, "Bon cap de setmana!", "¡Buen fin de semana!", "«Cap» es cabeza: la cabeza de la semana."},
    {1, "Perdoni, una pregunta", "Perdone, una pregunta",
     "Forma de usted. Buen arranque para parar a alguien por la calle."},
    {2, "Com es diu això en català?", "¿Cómo se dice esto en catalán?", "«Això» = esto."},
    {3, "Tinc una pregunta", "Tengo una pregunta", "«Tinc» sirve para casi todo: tinc gana, tinc son, tinc pressa."},
    {4, "Tinc gana", "Tengo hambre", "«Gana» = hambre."},
    {5, "Obriu els diumenges?", "¿Abrís los domingos?", "«Diumenge» = domingo. «Obrir» = abrir."},
    {6, "On és la sortida?", "¿Dónde está la salida?", "«Sortida» está en todos los carteles."},
    {7, "Encara no", "Todavía no", "«Encara» = todavía."},
    {8, "M'he llevat d'hora", "Me he levantado temprano", "«Llevar-se» = levantarse. «D'hora» = temprano."},
    {9, "Molta sort!", "¡Mucha suerte!", "«Sort» es femenino: «molta sort»."},
    {10, "A quina hora plegues?", "¿A qué hora sales de trabajar?", "«Plegar» = acabar la jornada. Muy de aquí."},
    {11, "Em marejo", "Me mareo", "«Marejar-se» = marearse."},
    {0, "Benvinguda!", "¡Bienvenida!", "En masculino, «benvingut»."},
    {1, "Passa, passa", "Pasa, pasa", "Para ceder el paso en una puerta."},
    {2, "Què vol dir?", "¿Qué significa?", "Literalmente, «¿qué quiere decir?»."},
    {3, "Em pots ajudar?", "¿Me puedes ayudar?", "«Ajudar» = ayudar. La j catalana es suave, como la francesa."},
    {4, "Tinc set", "Tengo sed", "Ojo: «set» también es siete."},
    {5, "A quina hora tanqueu?", "¿A qué hora cerráis?", "«Tancar» = cerrar. En las puertas verás «tancat», cerrado."},
    {6, "Vaig a peu", "Voy andando", "«A peu» = a pie."},
    {7, "Arribo tard", "Llego tarde", "También se dice «faig tard»."},
    {8, "Vaig a dutxar-me", "Voy a ducharme", "«Dutxa» = ducha. La tx suena como la ch castellana."},
    {9, "Bon viatge!", "¡Buen viaje!", "«tg» suena como la j inglesa de «jeans»: «viadje»."},
    {10, "Plego a les sis", "Salgo a las seis", "«A les sis» = a las seis."},
    {11, "He dormit malament", "He dormido mal", "«Malament» = mal; «bé» = bien."},
    {0, "Quant de temps!", "¡Cuánto tiempo!",
     "Tras «quant» y «molt» suele ir un «de»: «quant de temps», «molt de gust»."},
    {1, "Em sap greu", "Lo siento mucho", "Más sentido que «ho sento». Literalmente, «me sabe grave»."},
    {2, "Parla'm en català, si us plau", "Háblame en catalán, por favor",
     "La frase clave para que no te cambien al castellano."},
    {3, "Un moment, si us plau", "Un momento, por favor", "También «un segon», un segundo."},
    {4, "Vols res més?", "¿Quieres algo más?", "En las preguntas, «res» significa «algo»."},
    {5, "Busco unes sabates", "Busco unos zapatos", "«Sabata» = zapato. Es femenino: «unes sabates»."},
    {6, "Aquest bus va al centre?", "¿Este bus va al centro?",
     "«Aquest» = este. Delante de consonante la s no suena: «aquet»."},
    {7, "Quin dia és avui?", "¿Qué día es hoy?", "«Avui» = hoy."},
    {8, "Esmorzem?", "¿Desayunamos?", "«Esmorzar» = desayunar."},
    {9, "Que et milloris!", "¡Que te mejores!", "Para quien está enfermo. De usted: «que es millori»."},
    {10, "Estic enfeinada", "Estoy muy liada", "De «feina», trabajo. Un hombre diría «enfeinat»."},
    {11, "Estic millor", "Estoy mejor", "«Millor» = mejor; «pitjor» = peor."},
    {0, "Bon dia a tothom!", "¡Buenos días a todos!", "«Tothom» = todo el mundo. Va siempre en singular."},
    {1, "Cap problema", "Ningún problema", "«Cap» delante de un nombre = ningún, ninguna."},
    {2, "Tinc trenta anys", "Tengo treinta años", "«Tinc» = tengo. «Any» = año; la ny es la ñ."},
    {3, "Ara vinc", "Ahora vengo", "«Ara» = ahora."},
    {4, "Res més, gràcies", "Nada más, gracias", "Te preguntarán «alguna cosa més?». «Res més» = nada más."},
    {5, "En tens de més barats?", "¿Tienes más baratos?", "«Barat» = barato; «car» = caro."},
    {6, "On he de baixar?", "¿Dónde tengo que bajar?", "«He de» + infinitivo = tengo que."},
    {7, "Avui és dimarts", "Hoy es martes", "Ojo: «dimarts» es martes y «dimecres», miércoles."},
    {8, "Poso la rentadora", "Pongo la lavadora", "«Rentar» = lavar."},
    {9, "Tinc por", "Tengo miedo", "«Por» = miedo."},
    {10, "T'envio un correu", "Te envío un correo", "«Correu» = correo."},
    {11, "Ja em trobo bé", "Ya me encuentro bien", "«Ja» = ya."},
    {0, "Com va tot?", "¿Cómo va todo?", "Otra forma de preguntar «què tal?»."},
    {1, "Gràcies per tot", "Gracias por todo", "«Per» = por."},
    {2, "Treballo en una oficina", "Trabajo en una oficina",
     "Los verbos en -ar hacen el «yo» en -o: treballo, parlo. Esa o suena «u»."},
    {3, "Ja està", "Ya está", "«Ja» = ya. Se usa igual que en castellano para decir que algo está hecho."},
    {4, "El compte, si us plau", "La cuenta, por favor", "En catalán es masculino: «el compte»."},
    {5, "De quin color?", "¿De qué color?", "«Quin» delante de un nombre: quin color, quina talla."},
    {6, "A quina hora surt el tren?", "¿A qué hora sale el tren?", "«Sortir» = salir."},
    {7, "Dissabte i diumenge", "Sábado y domingo", "El «cap de setmana»."},
    {8, "Paro la taula", "Pongo la mesa", "«Parar taula» = poner la mesa. ¡No es pararla!"},
    {9, "Estic nerviosa", "Estoy nerviosa", "Un hombre diría «estic nerviós»."},
    {10, "Em passes el fitxer?", "¿Me pasas el archivo?", "«Fitxer» = archivo."},
    {11, "Cuida't!", "¡Cuídate!", "Una despedida cariñosa."},
    {0, "Tot bé, i tu?", "Todo bien, ¿y tú?", "«Bé» = bien. Con acento cerrado: suena como la e castellana."},
    {1, "Igualment!", "¡Igualmente!", "La respuesta a «bon cap de setmana» o a «bon profit»."},
    {2, "Aquest és el meu xicot", "Este es mi novio", "«Xicot», novio; «xicota», novia; «parella», pareja."},
    {3, "Oi que sí?", "¿A que sí?", "«Oi?» al final pide confirmación, como «¿verdad?»."},
    {4, "Puc pagar amb targeta?", "¿Puedo pagar con tarjeta?", "«Puc» = puedo."},
    {5, "M'agrada el vermell", "Me gusta el rojo", "Vermell, blau, verd, groc: rojo, azul, verde, amarillo."},
    {6, "Quant triga?", "¿Cuánto tarda?", "«Trigar» = tardar."},
    {7, "És la una", "Es la una", "Solo la una va en singular."},
    {8, "Rento els plats", "Lavo los platos", "«Plat» = plato."},
    {9, "Estic farta", "Estoy harta", "Un hombre diría «estic fart»."},
    {10, "Demà faig festa", "Mañana libro", "«Fer festa» = no trabajar ese día."},
    {11, "Descansa", "Descansa", "Igual que en castellano."},
    {0, "Fins ara!", "¡Hasta ahora!", "Para cuando os vais a ver enseguida."},
    {1, "Puc passar?", "¿Puedo pasar?", "«Puc» = puedo. Sirve para pedir paso o permiso."},
    {2, "Fa poc que visc aquí", "Hace poco que vivo aquí", "«Fa» + tiempo = hace: «fa un any», «fa poc»."},
    {3, "Què passa?", "¿Qué pasa?", "«Què» interrogativo lleva acento abierto."},
    {4, "Fem un vermut?", "¿Tomamos un vermut?", "Las bebidas «se hacen»: fer un cafè, fer un vermut."},
    {5, "Em va petit", "Me queda pequeño", "«Em va gran» = me queda grande."},
    {6, "És al costat del mercat", "Está al lado del mercado", "«Al costat de» = al lado de."},
    {7, "Són dos quarts de vuit", "Son las siete y media", "La hora mira a la siguiente: «dos cuartos de ocho»."},
    {8, "Apaga el llum", "Apaga la luz", "«El llum» es la lámpara o la luz encendida; «la llum», la luz en general."},
    {9, "Quina vergonya!", "¡Qué vergüenza!", "La ny es la ñ: «vergoña»."},
    {10, "Estic de vacances", "Estoy de vacaciones", "«Vacances», siempre en plural."},
    {11, "He caigut", "Me he caído", "En catalán «caure» va sin pronombre: «he caigut»."},
    {0, "Records a la família!", "¡Recuerdos a la familia!", "«Records» = recuerdos, saludos."},
    {1, "Bones festes!", "¡Felices fiestas!", "En Navidad, también «Bon Nadal!»."},
    {2, "Tinc un germà i una germana", "Tengo un hermano y una hermana",
     "La g ante e, i es suave, como la j catalana."},
    {3, "Què vols?", "¿Qué quieres?", "Vull, vols, vol: quiero, quieres, quiere."},
    {4, "Què hi ha de postres?", "¿Qué hay de postre?", "«Postres» va siempre en plural en catalán."},
    {5, "És per regalar", "Es para regalar", "Dilo y te lo envolverán."},
    {6, "Davant de l'església", "Delante de la iglesia", "«Davant» = delante; «darrere» = detrás."},
    {7, "Al matí i a la tarda", "Por la mañana y por la tarde", "Con «al» y «a la», no con «per»."},
    {8, "On és el comandament?", "¿Dónde está el mando?", "«Comandament» = el mando de la tele."},
    {9, "M'encanta!", "¡Me encanta!", "Funciona como «m'agrada»: m'encanta, m'encanten."},
    {10, "Fem una pausa?", "¿Hacemos una pausa?", "«Fem» = hacemos; también sirve para proponer."},
    {11, "M'he fet mal al peu", "Me he hecho daño en el pie", "«Peu» = pie."},
    {2, "A què et dediques?", "¿A qué te dedicas?", "De usted: «a què es dedica?»."},
    {3, "Vull això", "Quiero esto", "Señalar y decir «això» te saca de cualquier apuro."},
    {4, "Què vols prendre?", "¿Qué quieres tomar?", "«Prendre» = tomar."},
    {5, "Puc tornar-ho?", "¿Puedo devolverlo?", "«Tornar» = devolver, y también volver."},
    {6, "Creua el carrer", "Cruza la calle", "«Carrer» = calle, y es masculino: «el carrer»."},
    {7, "D'aquí a una estona", "Dentro de un rato", "«Una estona» = un rato. «D'aquí a» = dentro de."},
    {8, "Baixo les escombraries", "Bajo la basura", "«Escombraries» = basura, siempre en plural."},
    {9, "Tant de bo!", "¡Ojalá!", "Literalmente, «tanto de bueno»."},
    {10, "Bona feina!", "¡Buen trabajo!", "Para felicitar a alguien por lo que ha hecho."},
    {11, "Truca a una ambulància", "Llama a una ambulancia", "Para emergencias, el 112."},
    {2, "Soc mestra", "Soy maestra", "Las profesiones van sin artículo, como en castellano."},
    {3, "No en tinc", "No tengo", "«En» sustituye a lo ya dicho: «tens canvi?» «No en tinc»."},
    {4, "Un entrepà de formatge", "Un bocadillo de queso", "«Entrepà»: entre pan. «Formatge» = queso."},
    {5, "Quatre pomes, si us plau", "Cuatro manzanas, por favor", "«Poma» = manzana."},
    {6, "Agafo el metro", "Cojo el metro", "«Agafar» = coger."},
    {7, "Abans de dinar", "Antes de comer", "«Abans» = antes; «després» = después."},
    {8, "Qui cuina avui?", "¿Quién cocina hoy?", "«Cuinar» = cocinar; «la cuina», la cocina."},
    {9, "Anem a fer un volt?", "¿Vamos a dar una vuelta?", "«Fer un volt» = dar una vuelta."},
    {10, "Ho acabo demà", "Lo acabo mañana", "«Ho» va delante del verbo."},
    {11, "Ajuda!", "¡Ayuda!", "Para pedir auxilio también vale «socors!»."},
    {2, "Parles castellà?", "¿Hablas castellano?", "«Castellà» es como se llama aquí al español."},
    {3, "Hi vaig", "Voy (allí)", "«Hi» = allí. En catalán hay que decirlo: «hi vaig»."},
    {4, "Un got d'aigua, si us plau", "Un vaso de agua, por favor", "«Got» = vaso; «copa» = copa."},
    {5, "Això és tot", "Eso es todo", "Lo que dices cuando ya no quieres nada más."},
    {6, "Hi ha un caixer per aquí?", "¿Hay un cajero por aquí?", "«Per aquí» = por aquí."},
    {7, "Fa vent", "Hace viento", "«Vent» = viento. La t final casi no suena."},
    {8, "El sopar és a taula", "La cena está en la mesa", "«Sopar» es cenar y también la cena."},
    {9, "A quina hora quedem?", "¿A qué hora quedamos?", "«A quina hora» = a qué hora."},
    {10, "No arribo a temps", "No llego a tiempo", "«Arribar» = llegar."},
    {11, "Prenc aquest medicament", "Tomo este medicamento", "«Prenc», de «prendre»: tomo."},
    {2, "Encara m'equivoco molt", "Todavía me equivoco mucho",
     "Nadie se ríe: aquí se agradece mucho el esfuerzo de hablarlo."},
    {3, "Tinc pressa", "Tengo prisa", "«Pressa» = prisa."},
    {4, "Una copa de vi negre", "Una copa de vino tinto",
     "El tinto es «negre»; el blanco, «blanc»; el rosado, «rosat»."},
    {5, "On és la caixa?", "¿Dónde está la caja?", "La x suena «sh»: «casha»."},
    {6, "Busco aquesta adreça", "Busco esta dirección", "«Adreça» = la dirección de una casa."},
    {7, "Està núvol", "Está nublado", "«Núvol» es la nube y también «nublado»."},
    {8, "Vine aquí", "Ven aquí", "«Vine» es «ven»; «vinc» es «vengo»."},
    {9, "Vinc de seguida", "Vengo enseguida", "«De seguida» = enseguida."},
    {10, "Necessito ajuda", "Necesito ayuda", "«Necessito», con c y dos eses."},
    {11, "Tinc mal de queixal", "Me duele una muela", "«Queixal» = muela."},
    {2, "Com s'escriu?", "¿Cómo se escribe?", "Útil para nombres y calles."},
    {3, "Espera'm", "Espérame", "El pronombre se une al verbo con apóstrofo."},
    {4, "Un suc de taronja", "Un zumo de naranja", "«Suc» = zumo. «Taronja» = naranja."},
    {5, "Em cobres?", "¿Me cobras?", "La forma habitual de pedir la cuenta en una tienda o un bar."},
    {6, "Estic buscant una farmàcia", "Estoy buscando una farmacia", "Las farmacias se reconocen por la cruz verde."},
    {7, "Quin fred que fa!", "¡Qué frío hace!", "La fórmula: «quin» + nombre + «que fa»."},
    {8, "Estic a casa", "Estoy en casa", "«A casa» vale por «en casa» y por «a casa». Sin artículo."},
    {9, "M'ho he passat molt bé", "Me lo he pasado muy bien", "Para despedirte después de una buena tarde."},
    {10, "Molts ànims!", "¡Muchos ánimos!", "Para animar a quien tiene un examen o un mal día."},
    {11, "Vaig al gimnàs", "Voy al gimnasio", "«Gimnàs», con acento en la a."},
    {3, "Ja vinc!", "¡Ya voy!", "Ojo: hacia quien te llama, en catalán «vienes»: «ja vinc»."},
    {4, "Soc vegetariana", "Soy vegetariana", "Un hombre diría «vegetarià»."},
    {3, "Mira!", "¡Mira!", "Igual que en castellano. Mira, escolta: mira, escucha."},
    {4, "Per emportar", "Para llevar", "De «emportar-se», llevarse."},
    {3, "De debò?", "¿De verdad?", "También «de veritat?»."},
    {4, "Convido jo", "Invito yo", "«Convidar» = invitar."},
    {3, "Més o menys", "Más o menos", "«Més» lleva acento: más. «Mes» sin acento es el mes."},
    {4, "Paguem a mitges?", "¿Pagamos a medias?", "«A mitges» = a medias."},
    {3, "Una mica més", "Un poco más", "«Una mica» = un poco. «Gens» = nada."},
    {4, "Estava tot molt bo", "Estaba todo muy bueno", "Buena frase para despedirte del camarero."},
};
constexpr int kCount = sizeof(kPhrases) / sizeof(kPhrases[0]);

constexpr bool inSection(const int index, const int section) {
  return section == 0 || kPhrases[index].topic == section - 1;
}

constexpr int countIn(const int section) {
  int n = 0;
  for (int i = 0; i < kCount; ++i) n += inSection(i, section);
  return n;
}

// The nth phrase of a section, counting from 0.
constexpr int nth(const int section, int n) {
  for (int i = 0; i < kCount; ++i) {
    if (inSection(i, section) && n-- == 0) return i;
  }
  return 0;
}

// Which of its section a phrase is, counting from 0.
constexpr int placeIn(const int index, const int section) {
  int n = 0;
  for (int i = 0; i < index; ++i) n += inSection(i, section);
  return n;
}

// The next phrase of the section in a direction, going round.
constexpr int stepIn(int index, const int section, const int by) {
  const int dir = by < 0 ? -1 : 1;
  for (int tried = 0; tried < kCount; ++tried) {
    index = ((index + dir) % kCount + kCount) % kCount;
    if (inSection(index, section)) return index;
  }
  return index;
}

// The phrase of a section that belongs to a day.
constexpr int phraseFor(const int section, const long day) {
  const int count = countIn(section);
  return nth(section, static_cast<int>((day % count + count) % count));
}

struct State {
  Style style = Style::Editorial;
  int section = 0;
  int index = 0;
  long lastDay = -1;
};

// The phrase to open on: the day's on a new day, the one left on the page
// otherwise, or when the clock has never been set (today < 0).
constexpr void open(State& state, const long today) {
  if (today < 0 || today == state.lastDay) return;
  state.lastDay = today;
  state.index = phraseFor(state.section, today);
}

// Keeping to a section: the page stays if it is in it, and goes to the
// section's first phrase if not.
constexpr void choose(State& state, const int section) {
  state.section = section;
  if (!inSection(state.index, section)) state.index = nth(section, 0);
}

constexpr int kStateBytes = 9;
inline void packState(const State& state, uint8_t* out) {
  out[0] = 2;  // version
  out[1] = static_cast<uint8_t>(state.style);
  out[2] = static_cast<uint8_t>(state.section);
  out[3] = static_cast<uint8_t>(state.index & 0xFF);
  out[4] = static_cast<uint8_t>(state.index >> 8);
  const uint32_t day = static_cast<uint32_t>(state.lastDay);
  for (int i = 0; i < 4; ++i) out[5 + i] = static_cast<uint8_t>(day >> (8 * i));
}
inline bool unpackState(const uint8_t* in, const int size, State& state) {
  state = State{};
  if (size < kStateBytes || in[0] != 2 || in[1] >= dita::kStyleCount || in[2] >= kSectionCount) return false;
  const int index = in[3] | (in[4] << 8);
  if (index >= kCount || !inSection(index, in[2])) return false;
  state.style = static_cast<Style>(in[1]);
  state.section = in[2];
  state.index = index;
  uint32_t day = 0;
  for (int i = 0; i < 4; ++i) day |= static_cast<uint32_t>(in[5 + i]) << (8 * i);
  state.lastDay = static_cast<long>(static_cast<int32_t>(day));
  return true;
}

}  // namespace parlem

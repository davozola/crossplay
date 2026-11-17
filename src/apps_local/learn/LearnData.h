#pragma once

// The lessons. Forty to begin with: ten each of maths, economics, electronics
// and AI, in Spanish, written for this app. A lesson is an idea in two or
// three short paragraphs, one worked example and one question.
//
// Only what the reading serif can set: Latin-1. No arrows, no Greek, no euro
// sign. host-tests/learn checks every byte and every length.

#include "LearnCore.h"

namespace learn {

constexpr Lesson kLessons[] = {
    // --- Matemáticas ---------------------------------------------------------
    {Subject::Maths,
     "Interés compuesto y la regla del 72",
     "Con interés compuesto, cada año ganas intereses también sobre los intereses de los años anteriores. Por eso el "
     "dinero no crece en línea recta: crece cada vez más deprisa.\n"
     "Un atajo útil: divide 72 entre el tipo de interés anual y obtienes, aproximadamente, los años que tarda una "
     "cantidad en doblarse.",
     "Al 6 % anual, 72 / 6 = 12 años para doblar. Al 3 %, 24 años. Y al revés: una inflación del 4 % reduce a la mitad "
     "el poder de compra de un billete en unos 18 años.",
     "¿Cuánto tarda en doblarse una inversión al 9 % anual?",
     {"Unos 8 años", "Unos 11 años", "Unos 18 años"},
     0,
     "72 / 9 = 8. El cálculo exacto da 8,04 años: la regla acierta de sobra para decidir de cabeza."},
    {Subject::Maths,
     "Un logaritmo cuenta multiplicaciones",
     "El logaritmo responde a una pregunta sencilla: ¿cuántas veces hay que multiplicar la base por sí misma para "
     "llegar "
     "a un número? En base 10, el logaritmo de 1.000 es 3, porque 10 × 10 × 10 = 1.000.\n"
     "Por eso los logaritmos convierten multiplicaciones en sumas, y por eso se usan para medir cosas que abarcan "
     "muchos "
     "órdenes de magnitud: el sonido, los terremotos, la acidez.",
     "Cada 10 decibelios más multiplican la potencia del sonido por 10. De 60 dB a 90 dB hay 30 dB: la potencia se "
     "multiplica por 10 tres veces, es decir, por 1.000.",
     "Si el logaritmo en base 2 de un número es 10, ¿cuál es el número?",
     {"20", "100", "1.024"},
     2,
     "Es 2 multiplicado por sí mismo diez veces: 1.024. Por eso un kilobyte clásico tiene 1.024 bytes."},
    {Subject::Maths,
     "Bayes: cuánto cambia una prueba",
     "Una prueba positiva no significa lo que parece si lo que buscas es raro. Hay que combinar dos cosas: lo fiable "
     "que es la prueba y lo frecuente que es aquello que detecta.\n"
     "El error típico es mirar solo la fiabilidad de la prueba y olvidar el punto de partida.",
     "Una enfermedad afecta a 1 de cada 1.000 personas. La prueba detecta todos los casos y da un 5 % de falsos "
     "positivos. De 1.000 personas, 1 está enferma y da positivo; de las 999 sanas, unas 50 dan positivo también. "
     "Total: 51 positivos, y solo 1 enfermo.",
     "En ese ejemplo, si das positivo, ¿qué probabilidad hay de que estés enfermo?",
     {"Un 95 %", "Alrededor de un 2 %", "Un 50 %"},
     1,
     "1 enfermo entre 51 positivos: un 2 %. La prueba es buena, pero la enfermedad es tan rara que casi todos los "
     "positivos son falsos."},
    {Subject::Maths,
     "La esperanza: lo que ganas de media",
     "La esperanza matemática es el resultado medio de repetir una apuesta muchísimas veces. Se calcula multiplicando "
     "cada resultado posible por su probabilidad y sumando.\n"
     "No dice qué pasará la próxima vez: dice si, a la larga, el juego te favorece o te cuesta dinero.",
     "Una rifa cuesta 2 euros y hay 1 posibilidad entre 100 de ganar 100 euros. Esperanza del premio: 100 × 0,01 = "
     "1 euro. Pagas 2 y recibes de media 1: pierdes 1 euro por boleto.",
     "Un juego te paga 6 euros si sale un 6 en el dado y nada si no. ¿Cuál es el precio justo por jugar?",
     {"1 euro", "3 euros", "6 euros"},
     0,
     "6 euros × 1/6 = 1 euro. Por encima de ese precio pierdes de media; por debajo, ganas."},
    {Subject::Maths,
     "La desviación típica mide lo disperso",
     "La media dice dónde está el centro de unos datos; la desviación típica, cuánto se apartan de ese centro. Dos "
     "grupos pueden tener la misma media y ser completamente distintos.\n"
     "En muchos fenómenos naturales, unos dos tercios de los datos caen a menos de una desviación de la media, y un "
     "95 % a menos de dos.",
     "Dos clases sacan un 6 de media. En una, todos entre 5 y 7; en la otra, la mitad saca un 2 y la otra mitad un 10. "
     "La media es la misma; la desviación típica de la segunda es cuatro veces mayor.",
     "Las alturas de un grupo tienen media 170 cm y desviación típica 10 cm. ¿Entre qué valores está más o menos el "
     "95 %?",
     {"Entre 160 y 180 cm", "Entre 150 y 190 cm", "Entre 165 y 175 cm"},
     1,
     "Dos desviaciones a cada lado de la media: 170 - 20 y 170 + 20."},
    {Subject::Maths,
     "La ley de los grandes números",
     "Con pocas repeticiones, el azar hace lo que quiere. Con muchas, la proporción observada se acerca a la "
     "probabilidad real. Por eso un casino no teme a una noche de suerte de un cliente, pero vive tranquilo con "
     "millones de apuestas.\n"
     "Ojo: la ley no dice que el azar compense. Tras diez caras seguidas, la siguiente tirada sigue siendo al 50 %.",
     "Tiras una moneda 10 veces y salen 7 caras: un 70 %, nada raro. La tiras 10.000 veces y sale un 70 % de caras: "
     "eso es prácticamente imposible con una moneda normal.",
     "Sale rojo cinco veces seguidas en una ruleta. ¿Qué es más probable en la siguiente tirada?",
     {"Negro, porque toca", "Rojo, porque está en racha", "Lo mismo que siempre"},
     2,
     "La ruleta no tiene memoria. Creer que toca el negro es la falacia del jugador."},
    {Subject::Maths,
     "La derivada es una pendiente",
     "La derivada mide a qué ritmo cambia una cosa respecto a otra en un instante concreto. En una gráfica, es la "
     "inclinación de la curva en ese punto.\n"
     "Si la derivada es positiva, la función sube; si es negativa, baja; si es cero, estás en un llano: quizá un "
     "máximo o un mínimo.",
     "Si la posición de un coche es la función, su derivada es la velocidad, y la derivada de la velocidad es la "
     "aceleración. El velocímetro te está enseñando una derivada.",
     "El beneficio de una empresa en función del precio alcanza su máximo. ¿Cuánto vale ahí la derivada?",
     {"Es máxima", "Cero", "Negativa"},
     1,
     "En la cima la curva no sube ni baja: pendiente cero. Así se buscan los óptimos."},
    {Subject::Maths,
     "Primos: los átomos de los números",
     "Un número primo solo se puede dividir entre 1 y entre sí mismo. Todo número entero se descompone de una única "
     "forma en producto de primos, igual que una molécula en átomos.\n"
     "Multiplicar dos primos enormes es fácil; averiguar cuáles eran a partir del resultado es dificilísimo. Buena "
     "parte de la criptografía se apoya en esa diferencia.",
     "60 = 2 × 2 × 3 × 5. No hay otra combinación de primos que dé 60. En cambio, saber de qué dos primos de "
     "trescientas cifras sale un número de seiscientas llevaría más que la edad del universo.",
     "¿Cuál de estos números es primo?",
     {"51", "57", "59"},
     2,
     "51 = 3 × 17 y 57 = 3 × 19. El 59 no se puede dividir entre nada más."},
    {Subject::Maths,
     "Exponencial contra lineal",
     "Crecer de forma lineal es sumar siempre lo mismo; crecer de forma exponencial es multiplicar siempre por lo "
     "mismo. Al principio lo exponencial parece lento, y luego se dispara.\n"
     "La intuición humana es lineal, y por eso subestimamos las epidemias, las deudas y el interés compuesto.",
     "Un nenúfar dobla su superficie cada día y tarda 30 días en cubrir el estanque. El día 29 cubre la mitad. El día "
     "25, apenas un 3 %: casi nadie habría dado la alarma.",
     "Doblas una hoja de papel de 0,1 mm por la mitad 20 veces. ¿Qué grosor tendría?",
     {"Unos 2 milímetros", "Unos 2 metros", "Unos 100 metros"},
     2,
     "0,1 mm × 2 veinte veces son 0,1 mm × 1.048.576: unos 105 metros."},
    {Subject::Maths,
     "Correlación no es causa",
     "Que dos cosas suban y bajen juntas no significa que una provoque la otra. Puede haber una tercera causa común, "
     "puede ser al revés, o puede ser casualidad.\n"
     "Para hablar de causa hace falta un experimento, o al menos descartar las demás explicaciones.",
     "Los meses con más helados vendidos son también los de más ahogamientos. El helado no ahoga a nadie: el calor "
     "hace que se vendan más helados y que más gente se bañe.",
     "Un estudio ve que quienes desayunan sacan mejores notas. ¿Qué se puede concluir?",
     {"Que desayunar sube las notas", "Que hay relación, sin saber la causa", "Que las notas dan hambre"},
     1,
     "Quizá los hogares con más tiempo y recursos desayunan más y además estudian mejor. Con ese dato solo, no se "
     "sabe."},

    // --- Economía ------------------------------------------------------------
    {Subject::Economics,
     "El coste de oportunidad",
     "Lo que cuesta de verdad una decisión no es solo lo que pagas: es lo mejor a lo que renuncias al tomarla. El "
     "tiempo y el dinero que usas en una cosa ya no están para otra.\n"
     "Por eso algo gratis rara vez lo es: te cuesta lo que habrías hecho en su lugar.",
     "Tienes un local propio y montas tu tienda en él. No pagas alquiler, pero podrías alquilarlo por 1.000 euros al "
     "mes. Ese es el coste de usarlo tú: si la tienda no los gana, estás perdiendo dinero.",
     "Dejas un trabajo de 30.000 euros al año para estudiar un máster que cuesta 10.000. ¿Cuál es su coste real?",
     {"10.000 euros", "30.000 euros", "40.000 euros"},
     2,
     "La matrícula más el sueldo al que renuncias. Lo que dejas de ganar también cuenta."},
    {Subject::Economics,
     "Oferta y demanda",
     "El precio es el punto en que lo que la gente quiere comprar coincide con lo que otros quieren vender. Si sube la "
     "demanda o escasea la oferta, el precio sube; al revés, baja.\n"
     "El precio hace de señal: uno alto avisa a los productores de que conviene producir más y a los compradores de "
     "que conviene buscar alternativas.",
     "Una helada destruye media cosecha de naranjas. Hay menos naranjas y las mismas ganas de comprarlas: el precio "
     "sube hasta que parte de los compradores se pasa a otra fruta.",
     "Sale un estudio que dice que el aguacate es muy sano y nada más cambia. ¿Qué pasa con su precio?",
     {"Sube", "Baja", "No cambia"},
     0,
     "Aumenta la demanda con la misma oferta. Más adelante, el precio alto animará a plantar más."},
    {Subject::Economics,
     "Elasticidad: cuánto reacciona la gente",
     "La elasticidad mide cuánto cambia la cantidad que se compra cuando cambia el precio. Si la gente compra casi lo "
     "mismo cueste lo que cueste, la demanda es inelástica; si deja de comprar enseguida, es elástica.\n"
     "Depende sobre todo de si hay sustitutos y de si el producto es imprescindible.",
     "Si la insulina sube un 20 %, quien la necesita la compra igual: inelástica. Si una marca de refresco sube un "
     "20 %, muchos se pasan a otra: elástica.",
     "Un vendedor sube un 10 % el precio y vende un 30 % menos. ¿Qué le pasa a sus ingresos?",
     {"Suben", "Bajan", "Se quedan igual"},
     1,
     "Cobra más por unidad pero pierde muchas más ventas: 1,10 × 0,70 = 0,77. Ingresa un 23 % menos."},
    {Subject::Economics,
     "La inflación",
     "La inflación es la subida general de los precios: con el mismo dinero compras menos. No es que una cosa se "
     "encarezca, sino que casi todo lo hace a la vez.\n"
     "Perjudica a quien tiene ahorros en efectivo o ingresos fijos y favorece a quien debe dinero a tipo fijo, porque "
     "devolverá billetes que valen menos.",
     "Con un 3 % de inflación anual, 100 euros de hoy compran dentro de un año lo que hoy cuestan unos 97. Si tu "
     "cuenta te da un 1 %, en realidad pierdes alrededor de un 2 %.",
     "Tu sueldo sube un 2 % y los precios un 5 %. ¿Qué le ha pasado a tu sueldo real?",
     {"Ha subido un 2 %", "Ha bajado alrededor de un 3 %", "No ha cambiado"},
     1,
     "Lo que importa es lo que puedes comprar. Cobras más euros, pero compran menos."},
    {Subject::Economics,
     "Tipos de interés y bonos",
     "Un bono es una promesa de pagos fijos. Si después de comprarlo suben los tipos de interés, los bonos nuevos "
     "pagan más y el tuyo resulta menos atractivo: su precio baja. Si los tipos bajan, tu bono vale más.\n"
     "Tipos y precio de los bonos se mueven como un balancín.",
     "Compras por 1.000 euros un bono que paga 20 al año, un 2 %. Los tipos suben al 4 %: los bonos nuevos pagan 40. "
     "Nadie te dará 1.000 por el tuyo; habrá que rebajarlo hasta que su rentabilidad se parezca.",
     "El banco central baja los tipos de interés. ¿Qué pasa con el precio de los bonos ya emitidos?",
     {"Sube", "Baja", "Nada"},
     0,
     "Pagan más que los nuevos, así que valen más. Cuanto más largo el plazo, más se mueve su precio."},
    {Subject::Economics,
     "Un euro hoy vale más que mañana",
     "El dinero de hoy se puede invertir y crecer, así que cobrar una cantidad dentro de unos años vale menos que "
     "cobrarla ahora. Traer un cobro futuro a su valor de hoy se llama descontar.\n"
     "Es la base para valorar una empresa, una hipoteca o cualquier inversión: sumar lo que dará, cada pago traído a "
     "hoy.",
     "Con un tipo del 5 %, 100 euros de dentro de un año valen hoy 100 / 1,05 = 95,24 euros. Dentro de diez años, "
     "100 / 1,05 diez veces: unos 61 euros.",
     "Te ofrecen 1.000 euros hoy o 1.050 dentro de un año, y puedes invertir sin riesgo al 8 %. ¿Qué eliges?",
     {"Los 1.000 de hoy", "Los 1.050", "Da igual"},
     0,
     "1.000 euros al 8 % son 1.080 dentro de un año: más que 1.050."},
    {Subject::Economics,
     "La ventaja comparativa",
     "Aunque alguien sea mejor que otro en todo, a los dos les conviene especializarse e intercambiar. Lo que cuenta "
     "no es quién es mejor, sino a qué renuncia cada uno al hacer cada cosa.\n"
     "Cada cual debe dedicarse a aquello en lo que su coste de oportunidad es menor.",
     "Una abogada escribe a máquina más rápido que su asistente, pero cada hora que pasa tecleando deja de facturar "
     "200 euros. Le conviene pagar 20 euros la hora por mecanografía, aunque ella lo haga mejor.",
     "Un país produce mejor que su vecino tanto vino como ordenadores. ¿Le interesa comerciar con él?",
     {"No, lo hace todo mejor", "Sí, si se centra en lo que hace comparativamente mejor", "Solo si es más barato"},
     1,
     "Especializándose donde su ventaja es mayor y comprando el resto, los dos acaban con más de todo."},
    {Subject::Economics,
     "Externalidades",
     "Una externalidad es un coste o un beneficio que recae en alguien que no participó en la decisión. Como quien "
     "decide no lo paga ni lo cobra, el mercado produce demasiado de lo que perjudica y demasiado poco de lo que "
     "beneficia.\n"
     "Los impuestos, las multas y las subvenciones intentan que quien decide note ese efecto en su bolsillo.",
     "Una fábrica vierte al río. Fabricar le sale barato porque la limpieza la pagan los de río abajo. Si se le cobra "
     "por contaminar, el precio de su producto pasa a incluir el daño.",
     "Vacunarte protege también a quienes te rodean. Eso es...",
     {"Una externalidad negativa", "Una externalidad positiva", "Un coste de oportunidad"},
     1,
     "Otros se benefician sin pagar. Por eso muchas vacunas se subvencionan: si no, habría menos de las que "
     "convienen."},
    {Subject::Economics,
     "Pensar en el margen",
     "Las buenas decisiones no comparan totales ni medias, sino la siguiente unidad: ¿qué gano y qué me cuesta "
     "producir una más, trabajar una hora más?\n"
     "Lo ya gastado y que no se recupera es un coste hundido y no debería influir en lo que hagas a partir de ahora.",
     "Un avión va a despegar con asientos vacíos. Llevar un pasajero más cuesta 20 euros en combustible y comida. "
     "Vender ese asiento por 80 conviene, aunque el coste medio por pasajero sea de 150.",
     "Llevas gastados 50 euros en una entrada y te encuentras mal. ¿Qué debería decidir si vas?",
     {"Que ya pagaste", "Si disfrutarás más yendo o quedándote", "El precio de la entrada"},
     1,
     "Los 50 euros están perdidos vayas o no. Lo único que puedes elegir es la tarde que te espera."},
    {Subject::Economics,
     "Diversificar",
     "No poner todos los huevos en la misma cesta reduce el riesgo sin reducir en la misma medida lo que esperas "
     "ganar. Funciona porque las inversiones no suben y bajan todas a la vez.\n"
     "Lo que no se puede diversificar es el riesgo de que todo el mercado caiga junto.",
     "Una fábrica de paraguas gana cuando llueve y otra de crema solar cuando hace sol. Con acciones de las dos, tus "
     "resultados dependen mucho menos del tiempo que con una sola.",
     "¿Cuál de estas carteras está mejor diversificada?",
     {"Diez bancos del mismo país", "Empresas de sectores y países distintos", "Una sola empresa muy sólida"},
     1,
     "Los diez bancos caerán juntos en una crisis financiera. Diversificar es mezclar cosas que no se mueven igual."},

    // --- Electrónica ---------------------------------------------------------
    {Subject::Electronics,
     "La ley de Ohm",
     "La tensión empuja, la resistencia frena y la corriente es lo que consigue pasar. La ley de Ohm las une: tensión "
     "igual a corriente por resistencia, V = I × R.\n"
     "Piensa en agua: la tensión es la presión, la corriente el caudal y la resistencia lo estrecho de la tubería.",
     "Una resistencia de 100 ohmios conectada a 5 voltios deja pasar 5 / 100 = 0,05 amperios, es decir, 50 mA. Con "
     "1.000 ohmios pasarían 5 mA.",
     "Por una resistencia de 220 ohmios pasan 10 mA. ¿Qué tensión hay entre sus extremos?",
     {"2,2 voltios", "22 voltios", "0,22 voltios"},
     0,
     "V = I × R = 0,010 × 220 = 2,2 voltios. Conviene pasar siempre los miliamperios a amperios antes de multiplicar."},
    {Subject::Electronics,
     "Potencia: lo que se calienta",
     "La potencia es la energía por segundo que consume o disipa un componente. Se calcula multiplicando tensión por "
     "corriente: P = V × I, en vatios.\n"
     "En una resistencia toda esa potencia se convierte en calor. Si supera lo que el componente aguanta, se quema.",
     "Una resistencia con 5 voltios y 50 mA disipa 5 × 0,05 = 0,25 vatios. Justo el límite de las resistencias "
     "corrientes de un cuarto de vatio: se calentará bastante.",
     "Un cargador entrega 5 voltios y 2 amperios. ¿Qué potencia da?",
     {"2,5 vatios", "7 vatios", "10 vatios"},
     2,
     "5 × 2 = 10 vatios. Los cargadores rápidos suben la tensión para dar más potencia con la misma corriente."},
    {Subject::Electronics,
     "El divisor de tensión",
     "Dos resistencias en serie se reparten la tensión en proporción a su valor. Tomando la señal del punto medio "
     "obtienes una fracción de la tensión de entrada.\n"
     "Sirve para adaptar niveles y para leer sensores cuya resistencia cambia, pero no para alimentar nada: en cuanto "
     "conectas una carga, el reparto cambia.",
     "Con 10 kiloohmios arriba y 10 kiloohmios abajo, el punto medio está a la mitad: de 5 voltios salen 2,5. Con "
     "10 k arriba y 20 k abajo, salen dos tercios: 3,33 voltios.",
     "Tienes 9 voltios, 20 k arriba y 10 k abajo. ¿Qué tensión hay en el punto medio?",
     {"3 voltios", "4,5 voltios", "6 voltios"},
     0,
     "La de abajo es un tercio del total: 9 × 10 / 30 = 3 voltios."},
    {Subject::Electronics,
     "El condensador",
     "Un condensador guarda carga, como un pequeño depósito. Se llena deprisa al principio y cada vez más despacio a "
     "medida que su tensión se acerca a la de la fuente.\n"
     "Con una resistencia en serie, el tiempo de carga lo marca el producto R × C, la constante de tiempo: tras una "
     "constante está al 63 %, y tras cinco, prácticamente lleno.",
     "Con 10 kiloohmios y 100 µF, la constante es 10.000 × 0,0001 = 1 segundo. En 1 segundo llega al 63 % y en unos 5 "
     "segundos está cargado.",
     "Con 1 kiloohmio y 1.000 µF, ¿cuánto tarda en cargarse casi del todo?",
     {"1 segundo", "Unos 5 segundos", "Unos 50 segundos"},
     1,
     "La constante es 1.000 × 0,001 = 1 segundo, y hacen falta unas cinco."},
    {Subject::Electronics,
     "Un LED necesita su resistencia",
     "Un diodo deja pasar la corriente en un solo sentido. Un LED es un diodo que da luz, y cuando conduce mantiene "
     "entre sus patas una tensión casi fija: unos 2 voltios si es rojo, unos 3 si es azul o blanco.\n"
     "Como no limita la corriente por sí mismo, necesita una resistencia en serie que lo haga, o se quema.",
     "LED rojo a 5 voltios y 10 mA: en la resistencia caen 5 - 2 = 3 voltios, y 3 / 0,010 = 300 ohmios. Se monta la "
     "normalizada de 330.",
     "LED azul de 3 voltios, alimentado a 5 voltios y con 20 mA. ¿Qué resistencia le toca?",
     {"100 ohmios", "250 ohmios", "1.000 ohmios"},
     0,
     "Sobran 2 voltios: 2 / 0,020 = 100 ohmios."},
    {Subject::Electronics,
     "El transistor como interruptor",
     "Un transistor permite que una corriente pequeña controle una grande. Usado como interruptor, una señal débil de "
     "un microcontrolador enciende y apaga un motor, un relé o una tira de LED.\n"
     "En un NPN, una pequeña corriente en la base deja pasar una mucho mayor del colector al emisor. Un MOSFET hace lo "
     "mismo gobernado por tensión, casi sin consumir.",
     "Un pin de un microcontrolador da como mucho 20 mA y un motor pide 500 mA. El pin gobierna el transistor y el "
     "transistor lleva la corriente del motor, que viene de la alimentación y no del pin.",
     "¿Por qué no se conecta un motor directamente a un pin del microcontrolador?",
     {"Porque el pin no puede dar tanta corriente", "Porque el motor iría al revés", "Porque sobra tensión"},
     0,
     "El pin se dañaría. Con cargas con bobina hay que añadir además un diodo que absorba el pico al apagar."},
    {Subject::Electronics,
     "PWM: regular a base de pulsos",
     "Encender y apagar muy rápido equivale, para muchas cargas, a dar una tensión intermedia. La modulación por "
     "ancho de pulso varía cuánto tiempo de cada ciclo está encendida la salida: el ciclo de trabajo.\n"
     "Así se regula el brillo de un LED o la velocidad de un motor con una salida que solo sabe estar a cero o a tope, "
     "y sin gastar energía en calor.",
     "Una salida de 5 voltios con un ciclo de trabajo del 25 % está encendida la cuarta parte del tiempo: de media, "
     "1,25 voltios. El LED se ve a un cuarto de su brillo.",
     "Con 12 voltios y un ciclo de trabajo del 50 %, ¿qué tensión media recibe el motor?",
     {"3 voltios", "6 voltios", "12 voltios"},
     1,
     "La mitad del tiempo encendido: la mitad de la tensión, de media."},
    {Subject::Electronics,
     "Resistencias de pull-up",
     "Una entrada digital que no está conectada a nada no vale ni cero ni uno: flota y recoge ruido. Una resistencia "
     "de pull-up la une a la alimentación para que, en reposo, lea un uno seguro.\n"
     "El botón conecta entonces la entrada a masa: pulsado se lee cero, suelto se lee uno. Un pull-down hace lo mismo "
     "al revés.",
     "Botón entre el pin y masa, 10 kiloohmios entre el pin y 3,3 voltios. Suelto: el pin ve 3,3 voltios. Pulsado: el "
     "pin ve 0 voltios y por la resistencia pasan solo 0,33 mA.",
     "Con un pull-up y el botón a masa, ¿qué lee el pin al pulsar?",
     {"Un uno", "Un cero", "Un valor al azar"},
     1,
     "El botón lo lleva a masa. Por eso tantos botones funcionan con lógica invertida."},
    {Subject::Electronics,
     "Del mundo real a números: el ADC",
     "Un conversor analógico-digital mide una tensión y la convierte en un número. Su resolución en bits dice en "
     "cuántos escalones divide el margen: con 10 bits, 1.024; con 12 bits, 4.096.\n"
     "Cuantos más bits, más fino el escalón, aunque el ruido del circuito suele limitar antes que los bits.",
     "Un ADC de 10 bits con referencia de 5 voltios tiene escalones de 5 / 1.024, unos 4,9 mV. Si lee 512, la tensión "
     "es más o menos la mitad: 2,5 voltios.",
     "Un ADC de 12 bits con referencia de 3,3 voltios devuelve 1.024. ¿Qué tensión hay?",
     {"Unos 0,8 voltios", "Unos 1,65 voltios", "Unos 3,3 voltios"},
     0,
     "1.024 es la cuarta parte de 4.096: una cuarta parte de 3,3 voltios."},
    {Subject::Electronics,
     "El filtro paso bajo",
     "Una resistencia seguida de un condensador a masa deja pasar las señales lentas y atenúa las rápidas: el "
     "condensador no tiene tiempo de seguirlas.\n"
     "La frontera es la frecuencia de corte, 1 dividido entre 2 × pi × R × C. Por encima de ella la señal sale cada "
     "vez más pequeña.",
     "Con 1,6 kiloohmios y 100 nF, la frecuencia de corte ronda 1 kHz. Una señal de 100 Hz pasa casi intacta; una de "
     "10 kHz sale diez veces menor. Así se convierte una señal PWM en una tensión casi continua.",
     "Quieres bajar la frecuencia de corte de un filtro RC. ¿Qué haces?",
     {"Aumentar R o C", "Disminuir R o C", "Subir la tensión"},
     0,
     "La frecuencia de corte es inversa a R × C: cuanto mayores, más lento el filtro."},

    // --- IA ------------------------------------------------------------------
    {Subject::Ai,
     "Qué significa que un modelo aprenda",
     "Un modelo es una función con muchos números ajustables, los parámetros. Aprender consiste en ir cambiándolos "
     "para que, con los ejemplos de entrenamiento, las salidas se parezcan cada vez más a las respuestas correctas.\n"
     "Nadie programa las reglas: se muestra el resultado deseado y se deja que el ajuste las encuentre.",
     "Para predecir el precio de un piso según sus metros, el modelo más simple es una recta con dos parámetros: la "
     "pendiente y el punto de partida. Aprender es encontrar la recta que mejor pasa entre los pisos conocidos.",
     "¿Qué se modifica durante el entrenamiento de una red neuronal?",
     {"Los datos de entrenamiento", "Los parámetros del modelo", "El código del programa"},
     1,
     "Los datos y el código no cambian. Lo que se mueve son los pesos, millones o miles de millones de números."},
    {Subject::Ai,
     "El descenso de gradiente",
     "Para saber hacia dónde mover cada parámetro se mide el error y se calcula su gradiente: la dirección en que el "
     "error sube más deprisa. Se da un paso pequeño en la dirección contraria y se repite.\n"
     "Es como bajar una montaña con niebla tanteando la pendiente bajo los pies. El tamaño del paso es la tasa de "
     "aprendizaje.",
     "Con una tasa demasiado grande, el modelo salta de una ladera a otra y no baja nunca. Con una demasiado pequeña, "
     "baja, pero tarda una eternidad. Elegirla bien es buena parte del oficio.",
     "El error de un modelo sube y baja sin control durante el entrenamiento. ¿Qué es lo primero que se prueba?",
     {"Subir la tasa de aprendizaje", "Bajar la tasa de aprendizaje", "Añadir más capas"},
     1,
     "Los pasos son demasiado largos y se pasa del valle una y otra vez."},
    {Subject::Ai,
     "Sobreajuste",
     "Un modelo con capacidad de sobra puede memorizar los ejemplos en lugar de aprender la regla que hay detrás. "
     "Acierta todo lo que ya ha visto y falla con lo nuevo.\n"
     "Se combate con más datos, con modelos más sencillos o con técnicas que le impiden memorizar. Y se detecta "
     "midiendo siempre con datos que no ha visto.",
     "Un alumno que se aprende de memoria las respuestas de los exámenes de otros años saca un diez en esos y "
     "suspende en cuanto cambian las preguntas.",
     "Un modelo acierta el 99 % en entrenamiento y el 70 % con datos nuevos. ¿Qué le pasa?",
     {"Le falta entrenamiento", "Está sobreajustado", "Los datos nuevos están mal"},
     1,
     "La distancia entre las dos cifras es la señal: ha memorizado en lugar de generalizar."},
    {Subject::Ai,
     "Entrenar, validar y probar",
     "Los datos se reparten en tres partes. Con la de entrenamiento se ajusta el modelo. Con la de validación se "
     "eligen las decisiones de diseño. La de prueba se guarda sin tocar hasta el final.\n"
     "Si usas la prueba para tomar decisiones, deja de ser una medida honrada: sin querer, has ajustado el modelo "
     "también a ella.",
     "Pruebas veinte configuraciones y te quedas con la que mejor nota saca en validación. Esa nota es optimista: "
     "ganó en parte por suerte. La nota real es la que saque en la prueba, que no ha visto nunca.",
     "¿Para qué sirve el conjunto de prueba?",
     {"Para ajustar los parámetros", "Para elegir la mejor configuración", "Para medir el resultado final una vez"},
     2,
     "Es la única cifra que no ha influido en nada, y por eso es la única de fiar."},
    {Subject::Ai,
     "Tokens",
     "Un modelo de lenguaje no lee letras ni palabras, sino tokens: trozos de texto que pueden ser una palabra corta, "
     "parte de una larga o un signo. El texto se trocea, cada trozo se convierte en un número y con eso trabaja.\n"
     "Los límites de contexto y los precios se cuentan en tokens. En español, una palabra viene a ser entre uno y dos "
     "tokens.",
     "Una palabra frecuente como casa suele ser un solo token. Una rara o muy larga se parte en varios, como si se "
     "deletreara por sílabas. Por eso a los modelos les cuesta contar las letras de una palabra.",
     "¿Por qué un modelo puede fallar al contar las letras de una palabra?",
     {"Porque no sabe sumar", "Porque ve tokens y no letras", "Porque la palabra no está en sus datos"},
     1,
     "Recibe trozos ya codificados y las letras sueltas no están a la vista."},
    {Subject::Ai,
     "Embeddings: el significado como posición",
     "Un embedding convierte un texto en una lista de números de modo que los textos de significado parecido quedan "
     "cerca unos de otros. Es un mapa en el que la distancia mide el parecido.\n"
     "Sirve para buscar por sentido y no por palabras exactas, para agrupar documentos y para recomendar.",
     "Las frases el perro ladra y un can aúlla no comparten ninguna palabra, pero sus embeddings quedan cerca. La "
     "frase la bolsa cae queda lejos de ambas.",
     "Quieres encontrar documentos que hablen de lo mismo que una pregunta aunque usen otras palabras. ¿Qué usas?",
     {"Buscar palabras exactas", "Comparar embeddings", "Ordenar por fecha"},
     1,
     "Se convierte la pregunta en un embedding y se buscan los documentos más cercanos."},
    {Subject::Ai,
     "Atención",
     "El mecanismo de atención permite que, al procesar cada palabra, el modelo mire todas las demás y decida cuáles "
     "le importan. Así resuelve a qué se refiere un pronombre o qué adjetivo va con qué nombre.\n"
     "Es la pieza central de los transformers, la arquitectura de los modelos de lenguaje actuales, y permite "
     "procesar todo el texto a la vez en lugar de palabra por palabra.",
     "En la frase el trofeo no cabía en la maleta porque era demasiado grande, para entender qué era grande hay que "
     "mirar al trofeo y a la maleta. La atención es lo que conecta esas palabras.",
     "¿Qué aporta la atención a un modelo de lenguaje?",
     {"Memoria permanente", "Relacionar cada palabra con las relevantes del contexto", "Conexión a internet"},
     1,
     "Cada palabra se interpreta a la luz de las que más le afectan, estén cerca o lejos."},
    {Subject::Ai,
     "Temperatura",
     "Un modelo de lenguaje no elige una palabra: calcula una probabilidad para cada continuación posible y sortea "
     "una. La temperatura ajusta ese sorteo.\n"
     "Baja, elige casi siempre la más probable: respuestas previsibles y repetibles. Alta, da oportunidades a las "
     "menos probables: más variedad y también más disparates.",
     "Para extraer datos de una factura conviene una temperatura cercana a cero: quieres siempre la misma respuesta. "
     "Para proponer nombres de una marca, una más alta da ideas más variadas.",
     "Le haces la misma pregunta dos veces y responde distinto. ¿Por qué?",
     {"Porque ha aprendido entre medias", "Porque hay un sorteo al elegir cada palabra", "Porque se ha equivocado"},
     1,
     "El modelo no ha cambiado. Con temperatura por encima de cero, cada respuesta es una tirada distinta."},
    {Subject::Ai,
     "Alucinaciones",
     "Un modelo de lenguaje genera el texto más plausible, no necesariamente el verdadero. Cuando no sabe algo, puede "
     "producir una respuesta con la misma seguridad y la misma buena redacción que cuando sí lo sabe.\n"
     "No miente a propósito: no distingue por dentro entre recordar y completar con lo que suena bien.",
     "Si le pides la referencia de un artículo científico sobre un tema poco tratado, puede inventar un título "
     "verosímil, autores reales del campo y una revista que existe, de un artículo que nadie escribió.",
     "¿Cuál es la mejor defensa contra las alucinaciones?",
     {"Pedirle que no invente", "Darle las fuentes y comprobar lo que afirma", "Usar más temperatura"},
     1,
     "Con el texto delante inventa mucho menos, y lo que importa hay que verificarlo de todos modos."},
    {Subject::Ai,
     "RAG: buscar antes de responder",
     "La generación aumentada con recuperación consiste en buscar primero los documentos relevantes y pasárselos al "
     "modelo junto con la pregunta. El modelo responde a partir de ese texto y no solo de lo que recuerda.\n"
     "Permite usar información reciente o privada sin reentrenar, y citar de dónde sale cada cosa.",
     "Un asistente de una empresa recibe una pregunta sobre vacaciones. Busca en el manual interno los tres párrafos "
     "más parecidos, los añade a la consulta y redacta la respuesta citando el apartado.",
     "¿Qué problema resuelve sobre todo el RAG?",
     {"Que el modelo sea lento", "Que el modelo no conozca tus documentos", "Que el modelo gaste mucha energía"},
     1,
     "El modelo no se entrenó con ellos. Con RAG se le dan en el momento de responder."},
};

constexpr int kLessonCount = sizeof(kLessons) / sizeof(kLessons[0]);

}  // namespace learn

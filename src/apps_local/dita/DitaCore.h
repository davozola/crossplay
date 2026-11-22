#pragma once

// DITA, the part with no hardware in it: a hundred and fifty Catalan sayings,
// each with what it means in Spanish and, where it is worth having, what it
// says word for word, which is usually the funny half. One a day, and a tap
// for the next.

#include <cstdint>

namespace dita {

// What the third line of a saying is, when it has one.
enum class Extra : uint8_t { None, Literal, Note };

struct Saying {
  const char* ca;  // the saying
  const char* es;  // what it means, in Spanish
  Extra kind;
  const char* extra;  // the literal reading, or a note on the word
};

// In a fixed shuffle, so one kind does not follow itself for a week. Straight
// apostrophes and no ellipsis sign: not every cut on the device has the
// curly ones.
inline constexpr Saying kSayings[] = {
    {"Somiar truites", "Hacerse ilusiones imposibles", Extra::Literal, "Soñar tortillas"},
    {"Salut i força al canut!", "¡Salud y pesetas!", Extra::Literal, "¡Salud y fuerza al monedero!"},
    {"Qui oli remena, els dits se n'unta", "Quien maneja dinero, algo se le pega", Extra::Literal,
     "Quien remueve aceite, se pringa los dedos"},
    {"Val més boig conegut que savi per conèixer", "Más vale malo conocido que bueno por conocer", Extra::Literal,
     "Más vale loco conocido que sabio por conocer"},
    {"Molt soroll per no res", "Mucho ruido y pocas nueces", Extra::Literal, "Mucho ruido para nada"},
    {"La mar, com més té, més brama", "Quien más tiene, más quiere", Extra::Literal,
     "El mar, cuanto más tiene, más brama"},
    {"Setze jutges d'un jutjat mengen fetge d'un penjat", "Dieciséis jueces de un juzgado comen hígado de un ahorcado",
     Extra::Note, "El trabalenguas que delata a quien no es de aquí"},
    {"Fer un ressopó", "Picar algo a medianoche", Extra::Note, "La cena después de la cena tiene nombre propio"},
    {"Qui es lleva de matí, pixa allà on vol", "Quien madruga elige sitio", Extra::Literal,
     "Quien se levanta temprano, mea donde quiere"},
    {"No diguis blat fins que no sigui al sac i ben lligat", "No cantes victoria antes de tiempo", Extra::Literal,
     "No digas trigo hasta que esté en el saco y bien atado"},
    {"Gat escaldat, amb aigua tèbia en té prou", "El gato escaldado, del agua fría huye", Extra::Literal,
     "Gato escaldado, con agua tibia tiene bastante"},
    {"Dormir com un soc", "Dormir como un tronco", Extra::Literal, "Dormir como un zueco"},
    {"Anar a pastar fang", "Irse a freír espárragos", Extra::Literal, "Ir a amasar barro"},
    {"Bufar i fer ampolles", "Coser y cantar", Extra::Literal, "Soplar y hacer botellas"},
    {"Ser un pixapins", "Ser un dominguero de ciudad", Extra::Literal, "Ser un meapinos"},
    {"Estar a la quinta forca", "Estar en el quinto pino", Extra::Literal, "Estar en la quinta horca"},
    {"De mica en mica s'omple la pica", "Poco a poco se llena la pila", Extra::None, ""},
    {"Fugir cames ajudeu-me", "Salir por piernas", Extra::Literal, "Huir gritando «piernas, ayudadme»"},
    {"Fer campana", "Hacer novillos", Extra::Literal, "Hacer campana"},
    {"Treure foc pels queixals", "Echar chispas", Extra::Literal, "Sacar fuego por las muelas"},
    {"Déu n'hi do!", "¡Ahí es nada! ¡No está nada mal!", Extra::Literal, "Dios le dé"},
    {"Fer bullir l'olla", "Ganarse las habichuelas", Extra::Literal, "Hacer hervir la olla"},
    {"Fer dissabte", "Hacer limpieza general", Extra::Literal, "Hacer sábado"},
    {"Qui dia passa, any empeny", "Ir tirando también es avanzar", Extra::Literal, "Quien pasa un día, empuja un año"},
    {"Ser més vell que l'anar a peu", "Ser más viejo que Matusalén", Extra::Literal, "Ser más viejo que ir a pie"},
    {"Fer el préssec", "Hacer el ridículo", Extra::Literal, "Hacer el melocotón"},
    {"Aixecar la camisa", "Tomar el pelo", Extra::Literal, "Levantar la camisa"},
    {"Estar de mala lluna", "Estar de mal humor", Extra::Literal, "Estar de mala luna"},
    {"Fer-ne cinc cèntims", "Contarlo en cuatro palabras", Extra::Literal, "Hacer de ello cinco céntimos"},
    {"Ser de la ceba", "Ser catalán hasta la médula", Extra::Literal, "Ser de la cebolla"},
    {"Durar de Nadal a Sant Esteve", "Durar un suspiro", Extra::Literal,
     "Durar de Navidad a San Esteban, o sea, un día"},
    {"Qui no té feina, el gat pentina", "Quien no tiene nada que hacer, se lo inventa", Extra::Literal,
     "Quien no tiene trabajo, peina al gato"},
    {"Això rai!", "¡Eso es lo de menos!", Extra::None, ""},
    {"Ser un cap de suro", "Ser un cabeza de chorlito", Extra::Literal, "Ser un cabeza de corcho"},
    {"Tal dia farà un any", "Me da exactamente igual", Extra::Literal, "Tal día hará un año"},
    {"Tocar ferro", "Tocar madera", Extra::Literal, "Tocar hierro"},
    {"Barcelona és bona si la bossa sona", "Barcelona es buena si la bolsa suena", Extra::None, ""},
    {"Ser un setciències", "Ser un sabelotodo", Extra::Literal, "Ser un sieteciencias"},
    {"Posar fil a l'agulla", "Poner manos a la obra", Extra::Literal, "Poner hilo a la aguja"},
    {"Fer el vermut", "Tomar el aperitivo", Extra::Literal, "Hacer el vermut"},
    {"A l'estiu, tota cuca viu", "En verano, todo bicho vive", Extra::None, ""},
    {"Per Nadal, cada ovella al seu corral", "Por Navidad, cada oveja a su corral", Extra::None, ""},
    {"Nou nous noves", "Nueve nueces nuevas", Extra::Note, "«Nou» es nueve, nuevo y nuez"},
    {"Ser una bleda assolellada", "Ser una pánfila", Extra::Literal, "Ser una acelga soleada"},
    {"Anar a tot drap", "Ir a toda pastilla", Extra::Literal, "Ir a todo trapo"},
    {"Emprenyar-se com una mona", "Agarrar un cabreo de campeonato", Extra::Literal, "Enfadarse como una mona"},
    {"Apa, som-hi!", "¡Venga, vamos allá!", Extra::None, ""},
    {"Tenir cara de pomes agres", "Tener cara de pocos amigos", Extra::Literal, "Tener cara de manzanas agrias"},
    {"Estirar més el braç que la màniga", "Gastar más de lo que se tiene", Extra::Literal,
     "Estirar más el brazo que la manga"},
    {"Ploure a bots i barrals", "Llover a cántaros", Extra::Literal, "Llover a odres y barriles"},
    {"Fer goig", "Tener buena pinta, dar gusto verlo", Extra::Literal, "Hacer gozo"},
    {"Bon cap de setmana!", "¡Buen fin de semana!", Extra::Literal, "¡Buena cabeza de semana!"},
    {"Fer un cop de cap", "Lanzarse, decidirse de una vez", Extra::Literal, "Dar un golpe de cabeza"},
    {"T'estimo", "Te quiero", Extra::Note, "Ojo: «et vull» no es te quiero, es te deseo"},
    {"Això és mel!", "¡Esto es gloria!", Extra::Literal, "¡Esto es miel!"},
    {"Anar xino-xano", "Ir andando sin prisa", Extra::None, ""},
    {"Plou poc, però pel poc que plou, plou prou", "Llueve poco, pero para lo poco que llueve, llueve bastante",
     Extra::Note, "Trabalenguas"},
    {"Cercar la lluna en un cove", "Pedir peras al olmo", Extra::Literal, "Buscar la luna en un cesto"},
    {"Cada terra fa sa guerra", "En cada sitio tienen sus costumbres", Extra::Literal, "Cada tierra hace su guerra"},
    {"Arribar i moldre", "Llegar y besar el santo", Extra::Literal, "Llegar y moler"},
    {"Un plat blanc pla ple de pebre negre està", "Un plato blanco llano está lleno de pimienta negra", Extra::Note,
     "Trabalenguas"},
    {"Bona nit i tapa't", "Y se acabó la discusión", Extra::Literal, "Buenas noches y tápate"},
    {"En català tot es fa: un cafè, un petó i fins i tot mandra",
     "En catalán todo se hace: un café, un beso y hasta pereza", Extra::None, ""},
    {"Canviar l'aigua de les olives", "Ir a hacer pis", Extra::Literal, "Cambiar el agua de las aceitunas"},
    {"Al pot petit hi ha la bona confitura", "Lo bueno viene en frasco pequeño", Extra::Literal,
     "En el bote pequeño está la buena mermelada"},
    {"Posar-hi cullerada", "Meter baza", Extra::Literal, "Meter cucharada"},
    {"No hi ha un pam de net", "Aquí no se salva nadie", Extra::Literal, "No hay un palmo limpio"},
    {"Fer tard", "Llegar tarde", Extra::Literal, "Hacer tarde"},
    {"On Crist va perdre l'espardenya", "Donde Cristo perdió el gorro", Extra::Literal,
     "Donde Cristo perdió la alpargata"},
    {"Fer el cor fort", "Hacer de tripas corazón", Extra::Literal, "Hacer el corazón fuerte"},
    {"Ser cul i merda", "Ser uña y carne", Extra::Literal, "Ser culo y mierda"},
    {"Fer-ne un gra massa", "Pasarse de la raya", Extra::Literal, "Hacer un grano de más"},
    {"Qui no vulgui pols, que no vagi a l'era", "Quien no quiera líos, que no se meta", Extra::Literal,
     "Quien no quiera polvo, que no vaya a la era"},
    {"A cada bugada es perd un llençol", "En cada lío se pierde algo", Extra::Literal,
     "En cada colada se pierde una sábana"},
    {"Pixar fora de test", "Meter la pata, irse por las ramas", Extra::Literal, "Mear fuera del tiesto"},
    {"Fer mans i mànigues", "Hacer lo imposible por conseguirlo", Extra::Literal, "Hacer manos y mangas"},
    {"Ves-te'n a dida!", "¡Vete a paseo!", Extra::Literal, "¡Vete con la nodriza!"},
    {"Estar més content que un gínjol", "Estar más contento que unas castañuelas", Extra::Literal,
     "Estar más contento que una azufaifa"},
    {"Fer Pasqua abans de Rams", "Empezar la casa por el tejado", Extra::Literal, "Hacer Pascua antes de Ramos"},
    {"Tenir mala peça al teler", "Tenerlo crudo", Extra::Literal, "Tener mala pieza en el telar"},
    {"Fer volar coloms", "Hacer castillos en el aire", Extra::Literal, "Hacer volar palomas"},
    {"Que vagi bé!", "¡Que vaya bien!", Extra::Note, "La despedida de toda la vida"},
    {"Estar més pelat que una rata", "Estar sin blanca", Extra::Literal, "Estar más pelado que una rata"},
    {"Em fas perdre l'oremus", "Me haces perder el norte", Extra::Literal, "Me haces perder el oremus"},
    {"Fer petar la xerrada", "Echar una buena charla", Extra::Literal, "Hacer estallar la charla"},
    {"Tenir la butxaca foradada", "Ser un manirroto", Extra::Literal, "Tener el bolsillo agujereado"},
    {"Ser un tastaolletes", "Empezarlo todo y no acabar nada", Extra::Literal, "Ser un pruebaollitas"},
    {"Tenir pebrots", "Tener agallas", Extra::Literal, "Tener pimientos"},
    {"Roda el món i torna al Born", "Como en casa, en ningún sitio", Extra::Literal,
     "Da la vuelta al mundo y vuelve al Born"},
    {"Tenir el cul pelat de fer-ho", "Tener muchísima experiencia en algo", Extra::Literal,
     "Tener el culo pelado de hacerlo"},
    {"D'on no n'hi ha, no en raja", "De donde no hay, no se puede sacar", Extra::Literal, "De donde no hay, no mana"},
    {"Tenir un pet com un aglà", "Llevar una cogorza monumental", Extra::Literal, "Tener un pedo como una bellota"},
    {"Tenir molta barra", "Tener mucha cara", Extra::Literal, "Tener mucha mandíbula"},
    {"Anar curt d'armilla", "Andar justo de dinero", Extra::Literal, "Ir corto de chaleco"},
    {"Pagant, Sant Pere canta", "Con dinero todo se consigue", Extra::Literal, "Pagando, San Pedro canta"},
    {"Ser un llepafils", "Ser un tiquismiquis con la comida", Extra::Literal, "Ser un lamehilos"},
    {"Fer passar bou per bèstia grossa", "Dar gato por liebre", Extra::Literal, "Hacer pasar buey por bestia grande"},
    {"Ser un panxacontent", "Ser un vivalavirgen", Extra::Literal, "Ser un barrigacontenta"},
    {"Ser gat vell", "Ser perro viejo", Extra::Literal, "Ser gato viejo"},
    {"No hi toca ni quarts ni hores", "Está completamente ido", Extra::Literal, "No da ni los cuartos ni las horas"},
    {"Ser un cul d'en Jaumet", "Ser un culo inquieto", Extra::Literal, "Ser un culo de Jaimito"},
    {"Ser més curt que una màniga d'armilla", "Ser muy corto de entendederas", Extra::Literal,
     "Ser más corto que la manga de un chaleco"},
    {"S'ha acabat el bròquil", "Se acabó lo que se daba", Extra::Literal, "Se ha acabado el brócoli"},
    {"En cap cap cap el que cap en aquest cap", "En ninguna cabeza cabe lo que cabe en esta cabeza", Extra::Note,
     "Trabalenguas: «cap» es ninguna, cabeza y cabe"},
    {"Bon vent i barca nova!", "¡Hasta nunca!", Extra::Literal, "¡Buen viento y barca nueva!"},
    {"Parlant la gent s'entén", "Hablando se entiende la gente", Extra::None, ""},
    {"Tants caps, tants barrets", "Cada cabeza, una opinión", Extra::Literal, "Tantas cabezas, tantos sombreros"},
    {"Hi ha més dies que llonganisses", "No hay prisa", Extra::Literal, "Hay más días que longanizas"},
    {"Anar de bòlit", "Ir de cabeza, sin parar", Extra::Literal, "Ir como un bólido"},
    {"Ser un tros de quòniam", "Ser un zoquete", Extra::None, ""},
    {"Tinc son, però no tinc cap somni", "Tengo sueño, pero no tengo ningún sueño", Extra::Note,
     "«Son» son las ganas de dormir; «somni», lo que sueñas"},
    {"A les set tinc set", "A las siete tengo sed", Extra::Note, "«Set» es siete y también sed"},
    {"Tenir una gana que m'alça", "Tener un hambre que no veo", Extra::Literal, "Tener un hambre que me levanta"},
    {"No tenir ni cinc", "No tener ni un duro", Extra::Literal, "No tener ni cinco"},
    {"Estar com un llum", "Estar como una cabra", Extra::Literal, "Estar como una lámpara"},
    {"Un quart de nou", "Las ocho y cuarto", Extra::Literal, "Un cuarto de nueve"},
    {"Qui no arrisca no pisca", "Quien no arriesga no gana", Extra::None, ""},
    {"Tenir-hi la mà trencada", "Tener mucha maña para algo", Extra::Literal, "Tener la mano rota"},
    {"Ets la meva mitja taronja", "Eres mi media naranja", Extra::None, ""},
    {"Tenir la paella pel mànec", "Tener la sartén por el mango", Extra::Note,
     "«Paella» es la sartén; el arroz tomó el nombre prestado"},
    {"Menja bé, caga fort i no tinguis por de la mort", "Come bien, caga fuerte y no le temas a la muerte", Extra::Note,
     "Filosofía de vida, versión catalana"},
    {"Tenir el cap ple de pardals", "Tener la cabeza llena de pájaros", Extra::Literal,
     "Tener la cabeza llena de gorriones"},
    {"Arribar a misses dites", "Llegar cuando ya ha acabado todo", Extra::Literal, "Llegar con las misas dichas"},
    {"Arribar a tres quarts de quinze", "Llegar a las tantas", Extra::Literal,
     "Llegar a tres cuartos de quince, una hora que no existe"},
    {"Anar a escampar la boira", "Salir a despejarse", Extra::Literal, "Ir a esparcir la niebla"},
    {"Són tres quarts de dotze", "Son las doce menos cuarto", Extra::Literal, "Son tres cuartos de doce"},
    {"Fer la migdiada", "Echar la siesta", Extra::Literal, "Hacer la mediodiada"},
    {"L'any de la picor", "Hace una eternidad", Extra::Literal, "El año del picor"},
    {"Ser de Can Fanga", "Ser de Barcelona", Extra::Literal, "Ser de Casa Barro"},
    {"Qui matina fa farina", "A quien madruga, Dios le ayuda", Extra::Literal, "Quien madruga hace harina"},
    {"Estar per sucar-hi pa", "Estar para chuparse los dedos", Extra::Literal, "Estar para mojar pan"},
    {"Estar tocat del bolet", "Estar chiflado", Extra::Literal, "Estar tocado de la seta"},
    {"Anar fent", "Ir tirando", Extra::Literal, "Ir haciendo"},
    {"Estar a la lluna de València", "Estar en la luna", Extra::Literal, "Estar en la luna de Valencia"},
    {"La pela és la pela", "La pasta es la pasta", Extra::None, ""},
    {"Fer mandra", "Dar pereza", Extra::Literal, "Hacer pereza"},
    {"Fer safareig", "Cotillear", Extra::Literal, "Hacer lavadero"},
    {"Remenar les cireres", "Cortar el bacalao", Extra::Literal, "Remover las cerezas"},
    {"Tenir llana al clatell", "Ser un ingenuo", Extra::Literal, "Tener lana en el cogote"},
    {"Fer el gos", "Hacer el vago", Extra::Literal, "Hacer el perro"},
    {"Anar a can Felip", "Ir al baño", Extra::Literal, "Ir a casa de Felipe"},
    {"Compte!", "¡Cuidado!", Extra::Note, "La misma palabra que «cuenta»"},
    {"Suar la cansalada", "Sudar la gota gorda", Extra::Literal, "Sudar el tocino"},
    {"Parlar pels descosits", "Hablar por los codos", Extra::Literal, "Hablar por los descosidos"},
    {"Ser un figaflor", "Ser un blandengue", Extra::Literal, "Ser un higo flor"},
    {"Fer un petó", "Dar un beso", Extra::Literal, "Hacer un beso"},
    {"I un be negre!", "¡Y un cuerno!", Extra::Literal, "¡Y un cordero negro!"},
    {"Fer-ne de l'alçada d'un campanar", "Liarla parda", Extra::Literal, "Hacer una de la altura de un campanario"},
    {"Feina feta no té destorb", "No dejes para mañana lo que puedas hacer hoy", Extra::Literal,
     "Trabajo hecho no estorba"},
    {"Fer un cafè", "Tomar un café", Extra::Literal, "Hacer un café"},
};
constexpr int kCount = sizeof(kSayings) / sizeof(kSayings[0]);

// How the page is set; a swipe up or down goes from one to the next.
//   Editorial   the saying in a large italic on a white page
//   Cartell     a black poster, the saying bold and centred
//   Diccionari  an entry: its number in a column, then the saying and its senses
enum class Style : uint8_t { Editorial, Cartell, Diccionari, Count };
constexpr int kStyleCount = static_cast<int>(Style::Count);
constexpr Style stepStyle(const Style style, const int by) {
  return static_cast<Style>(((static_cast<int>(style) + by) % kStyleCount + kStyleCount) % kStyleCount);
}

constexpr int step(const int index, const int by) { return ((index + by) % kCount + kCount) % kCount; }

// Days since 1 January 1970 for a date on the calendar.
constexpr long dayNumber(const int year, const int month, const int day) {
  const int y = month <= 2 ? year - 1 : year;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// The saying that belongs to a day.
constexpr int sayingFor(const long day) { return static_cast<int>((day % kCount + kCount) % kCount); }

// What the app remembers: how the page is set, the saying on it, and the day
// it last opened on.
struct State {
  Style style = Style::Editorial;
  int index = 0;
  long lastDay = -1;
};

// The saying to open on. On a new day it is that day's; on the same day, or
// with a clock that has never been set (today < 0), it is the one left on the
// page, so tapping ahead is not undone by leaving and coming back.
constexpr void open(State& state, const long today) {
  if (today < 0 || today == state.lastDay) return;
  state.lastDay = today;
  state.index = sayingFor(today);
}

constexpr int kStateBytes = 7;
inline void packState(const State& state, uint8_t* out) {
  out[0] = 1;  // version
  out[1] = static_cast<uint8_t>(state.style);
  out[2] = static_cast<uint8_t>(state.index);
  const uint32_t day = static_cast<uint32_t>(state.lastDay);
  for (int i = 0; i < 4; ++i) out[3 + i] = static_cast<uint8_t>(day >> (8 * i));
}
inline bool unpackState(const uint8_t* in, const int size, State& state) {
  state = State{};
  if (size < kStateBytes || in[0] != 1 || in[1] >= kStyleCount || in[2] >= kCount) return false;
  state.style = static_cast<Style>(in[1]);
  state.index = in[2];
  uint32_t day = 0;
  for (int i = 0; i < 4; ++i) day |= static_cast<uint32_t>(in[3 + i]) << (8 * i);
  state.lastDay = static_cast<long>(static_cast<int32_t>(day));
  return true;
}

}  // namespace dita

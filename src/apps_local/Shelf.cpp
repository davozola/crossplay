#include "Shelf.h"

#include <HalStorage.h>
#include <Logging.h>
#include <strings.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../activities/ActivityManager.h"
#include "ShelfFolderActivity.h"
#include "ShelfHidden.h"
#include "ShelfOrder.h"
#include "ShelfState.h"
#include "batch/BatchActivity.h"
#include "battleship/BattleshipActivity.h"
#include "calculator/CalculatorActivity.h"
#include "casa/CasaActivity.h"
#include "checkers/CheckersActivity.h"
#include "chess/ChessActivity.h"
#include "chessclock/ChessClockActivity.h"
#include "clock/ClockActivity.h"
#include "connectfour/ConnectFourActivity.h"
#include "connections/ConnectionsActivity.h"
#include "discover/DiscoverActivity.h"
#include "dungeon/DungeonActivity.h"
#include "fmguide/FmGuideActivity.h"
#include "forehead/ForeheadActivity.h"
#include "frame/FrameActivity.h"
#include "go/GoActivity.h"
#include "hackernews/HackerNewsActivity.h"
#include "handwriting/HandwritingActivity.h"
#include "hearts/HeartsActivity.h"
#include "hex/HexActivity.h"
#include "history/HistoryActivity.h"
#include "hive/HiveActivity.h"
#include "innerclock/InnerClockActivity.h"
#include "insider/InsiderActivity.h"
#include "instapaper/InstapaperActivity.h"
#include "jaipur/JaipurActivity.h"
#include "jukebox/JukeboxActivity.h"
#include "kakuro/KakuroActivity.h"
#include "kids/KidsActivity.h"
#include "knucklebones/KnucklebonesActivity.h"
#include "learn/LearnActivity.h"
#include "logic/LogicActivity.h"
#include "mancala/MancalaActivity.h"
#include "markets/MarketsActivity.h"
#include "mastermind/MastermindActivity.h"
#include "minesweeper/MinesweeperActivity.h"
#include "murdle/MurdleActivity.h"
#include "mystery/MysteryActivity.h"
#include "naval/NavalActivity.h"
#include "notes/NotesActivity.h"
#include "paint/PaintActivity.h"
#include "picross/PicrossActivity.h"
#include "player/PlayerActivity.h"
#include "pomodoro/PomodoroActivity.h"
#include "rpn/RpnActivity.h"
#include "seasalt/SeaSaltActivity.h"
#include "solitaire/SolitaireActivity.h"
#include "sudoku/SudokuActivity.h"
#include "tactics/TacticsActivity.h"
#include "tak/TakActivity.h"
#include "theory/TheoryActivity.h"
#include "today/TodayActivity.h"
#include "toybattle/ToyBattleActivity.h"
#include "triples/TriplesActivity.h"
#include "trivia/TriviaActivity.h"
#include "ui/ExtraIcons.h"
#include "ui/ToyboxIcons.h"
#include "underhand/UnderhandActivity.h"
#include "wallpapers/WallpapersActivity.h"
#include "wavelength/WavelengthActivity.h"
#include "weather/WeatherActivity.h"
#include "wikipedia/WikipediaActivity.h"
#include "wordle/WordleActivity.h"
#include "yahtzee/YahtzeeActivity.h"

namespace {

// Icons come from tools_local/toybox/icons.txt via Lucide. Picked for silhouette
// rather than literalness: a crown, a hull, a grid and a card suit share no
// shape, so a row is scannable before the label is read.
// The kinds a folder sorts its items into, in the order of their tabs. An
// item's `category` is an index into its folder's list; the folder screen adds
// a tab before them (the favourites and what was last opened) and one after
// (everything).
constexpr const char* kGameTabs[] = {"TABLERO", "CARTAS", "LOGICA", "PALABRAS", "GRUPO", "ILYA"};
constexpr const char* kAppTabs[] = {"INFO", "LEER", "APRENDER", "UTIL"};

constexpr shelf::Item kGames[] = {
    {"CHESS", &icon_chess_32, &ChessActivity::create, 0, false, "Ajedrez: máquina o a dos",
     "Un motor de ajedrez completo en el dispositivo, con varios niveles, o un tablero para jugar entre dos personas."},
    {"BATTLESHIP", &icon_battleship_32, &BattleshipActivity::create, 0, false, "Hundir la flota",
     "Coloca tu flota y da caza a la del rival, contra el dispositivo."},
    {"CONNECTIONS", &icon_connections_32, &ConnectionsActivity::create, 3, false, "Agrupa 16 palabras en 4 temas",
     "La cuadrícula diaria de palabras: encuentra los cuatro grupos de cuatro. Con archivo de tableros anteriores. En "
     "inglés."},
    {"SOLITAIRE", &icon_solitaire_32, &SolitaireActivity::create, 1, false, "El solitario clásico",
     "Klondike con la pantalla en horizontal, que es la forma que tiene una mesa de solitario."},
    {"HEARTS", &icon_hearts_32, &HeartsActivity::create, 1, false, "Corazones: esquiva las bazas",
     "Evita llevarte bazas: cada corazón cuesta un punto y la dama de picas, trece. Contra tres rivales del "
     "dispositivo."},
    {"D&DIAGRAMS", &icon_dungeon_32, &DungeonActivity::create, 2, false, "Nonogramas de mazmorras",
     "Un nonograma cuyas pistas dibujan una mazmorra: paredes, monstruos y tesoros. Sesenta y cuatro tableros."},
    {"INSIDER", &icon_insider_32, &InsiderActivity::create, 4, false, "Fiesta: hay un infiltrado",
     "Para una mesa y un solo dispositivo: se adivina una palabra a preguntas mientras alguien, que ya la sabe, guía "
     "sin delatarse."},
    {"JAIPUR", &icon_jaipur_32, &JaipurActivity::create, 1, false, "Mercaderes, para dos",
     "El juego de mercaderes para dos: compra, vende y hazte con los camellos. Contra el dispositivo."},
    {"SEA SALT", &icon_seasalt_32, &SeaSaltActivity::create, 1, false, "Parejas de cartas y apuesta",
     "Colecciona parejas de cartas y decide cuándo plantarte: STOP o ÚLTIMA OPORTUNIDAD."},
    {"MURDLE", &icon_murdle_32, &MurdleActivity::create, 2, false, "Deduce quién, dónde y con qué",
     "Un misterio en cuadrícula lógica: cruzas pistas hasta saber quién fue, dónde y con qué. Nunca hay que adivinar."},
    {"CHECKERS", &icon_checkers_32, &CheckersActivity::create, 0, false, "Damas inglesas",
     "Damas con captura obligatoria, que el propio tablero te señala. Contra el dispositivo o a dos."},
    {"CONNECT FOUR", &icon_connectfour_32, &ConnectFourActivity::create, 0, false, "Cuatro en raya vertical",
     "Deja caer una ficha y alinea cuatro. Siete columnas, un toque cada una."},
    {"YAHTZEE", &icon_yahtzee_32, &YahtzeeActivity::create, 1, false, "Dados: 13 casillas, 3 tiradas",
     "El juego de dados de las trece casillas, con tres tiradas por turno y las reglas del comodín completas."},
    {"KNUCKLEBONES", &icon_knucklebones_32, &KnucklebonesActivity::create, 1, false, "Dados que suman y destruyen",
     "El juego de dados de Cult of the Lamb: los dados iguales multiplican y los tuyos eliminan los del rival."},
    {"MINESWEEPER", &icon_minesweeper_32, &MinesweeperActivity::create, 2, false, "Buscaminas",
     "Toca para destapar, mantén para marcar una bandera y toca un número completo para abrir sus vecinas."},
    {"SUDOKU", &icon_sudoku_32, &SudokuActivity::create, 2, false, "Sudoku generado y graduado",
     "Tableros generados en el dispositivo y clasificados por la técnica que exigen, no por el número de pistas."},
    {"PICROSS", &icon_picross_32, &PicrossActivity::create, 2, false, "Rellena y sale un dibujo",
     "Nonogramas: rellena casillas según las pistas de filas y columnas hasta que aparece la imagen."},
    {"TOY BATTLE", &icon_toybattle_32, &ToyBattleActivity::create, 0, false, "Bases, caminos y medallas",
     "Nueve tableros de bases y caminos: ocupa regiones y gana medallas. Contra el dispositivo."},
    {"FOREHEAD", &icon_forehead_32, &ForeheadActivity::create, 4, false, "Pantalla en la frente, 60 s",
     "Te pones la pantalla en la frente y los demás te dan pistas a gritos para que adivines la palabra. Sesenta "
     "segundos."},
    {"TRIVIA", &icon_trivia_32, &TriviaActivity::create, 3, false, "50.000 preguntas de Jeopardy",
     "Preguntas de 42 años de Jeopardy, para leer en voz alta o jugar a solas. En inglés."},
    {"WAVELENGTH", &icon_wavelength_32, &WavelengthActivity::create, 4, false, "Una pista y a discutir",
     "Hay un punto oculto en un espectro entre dos extremos; alguien da una pista y el resto discute dónde cae."},
    {"GO", &icon_go_32, &GoActivity::create, 0, false, "Go en 9x9, 13x13 o 19x19",
     "Contra el dispositivo o con alguien al lado. Cuenta el territorio por ti al acabar."},
    {"HEX", &icon_hex_32, &HexActivity::create, 0, false, "Une tus dos orillas",
     "Sin capturas y sin empates: gana quien conecta primero sus dos lados del tablero."},
    {"UNDERHAND", &icon_underhand_32, &UnderhandActivity::create, 1, false, "Cartas: soborna e invoca",
     "El juego de cartas de culto de 2017: administra recursos, soborna, sacrifica, esquiva a la policía e invoca a un "
     "dios."},
    {"WORDLE", &icon_wordle_32, &WordleActivity::create, 3, false, "La palabra del día en 6",
     "La palabra del día en seis intentos, con todos los días anteriores en un archivo. En inglés."},
    {"TAK", &icon_tak_32, &TakActivity::create, 0, false, "Un camino de lado a lado",
     "El juego de Playtak, de 4x4 a 6x6, contra la máquina o a dos."},
    {"INNER CLOCK", &icon_innerclock_32, &InnerClockActivity::create, 4, false, "Cuenta segundos de cabeza",
     "Toca, cuenta los segundos de cabeza y vuelve a tocar: te dice a cuántas centésimas te has quedado."},
    {"KAKURO", &icon_kakuro_32, &KakuroActivity::create, 2, false, "Sumas cruzadas",
     "De 5x5 a 9x9, cada tablero con una única solución demostrada. Sin adivinar."},
    {"MASTERMIND", &icon_mastermind_32, &MastermindActivity::create, 2, false, "Descifra el código de formas",
     "A solas contra el dispositivo, en tres niveles."},
    {"HIVE", &icon_hive_32, &HiveActivity::create, 0, false, "Insectos sin tablero",
     "Rodea a la reina rival con tus insectos. Contra el dispositivo, en tres niveles, o a dos."},
    {"TRIPLES", &icon_triples_32, &TriplesActivity::create, 2, false, "Desliza fichas: 1 y 2 hacen 3",
     "En un tablero de 4x4: el 1 y el 2 suman 3 y las parejas se doblan. Entra una ficha nueva en cada movimiento."},
    {"MANCALA", &icon_mancala_32, &MancalaActivity::create, 0, false, "Kalah: siembra y captura",
     "Contra la máquina, en tres niveles, o entre dos; de 3 a 6 semillas por hoyo y las reglas a un toque."},
    {"TACTICS", &icon_tactics_32, &TacticsActivity::create, 0, false, "Problemas de ajedrez",
     "Problemas de Lichess desde un paquete en la tarjeta: la dificultad sigue tu nivel, con pistas, reto diario y "
     "estadísticas."},
    {"MISTERIOS", &icon_mystery_32, &MysteryActivity::create, 2, false, "Seis casos de detective",
     "En castellano: interrogas a cuatro sospechosos, registras tres lugares y acusas cuando tienes motivo, ocasión y "
     "coartada rota."},
    {"ILYA", &icon_kids_32, &KidsActivity::create, 5, false, "Seis minijuegos en catalán",
     "Para un niño de tres años: dibujos, parejas, letras, números y El Talp, un golpea-al-topo sin prisas."},
    {"PINTA", &icon_pinta_32, &PaintActivity::create, 5, false, "Pintar con el dedo",
     "Trazo fino, trazo gordo, sellos de dibujos, goma y hoja nueva. Sin letras."},
    {"SLITHERLINK", &icon_slither_32, &LogicActivity::createSlither, 2, false, "Un lazo cerrado entre puntos",
     "Dibuja un único lazo siguiendo los números de cada casilla. Noventa tableros con solución única."},
    {"SHIKAKU", &icon_shikaku_32, &LogicActivity::createShikaku, 2, false, "Corta en rectángulos",
     "Cada rectángulo contiene un número, que es su área. Noventa tableros."},
    {"AKARI", &icon_akari_32, &LogicActivity::createAkari, 2, false, "Ilumina todas las casillas",
     "Coloca bombillas hasta iluminar cada casilla sin que dos se vean entre sí. Noventa tableros."},
    {"HASHI", &icon_hashi_32, &LogicActivity::createHashi, 2, false, "Une las islas con puentes",
     "Conecta todas las islas en una sola pieza con el número de puentes que pide cada una. Noventa tableros."},
};
constexpr shelf::Item kApps[] = {
    {"HACKER NEWS", &icon_hackernews_32, &HackerNewsActivity::create, 1, true, "La portada de Hacker News",
     "La portada en una letra de lectura; los artículos que abres se guardan en la tarjeta para leerlos sin red."},
    {"THE BATCH", &icon_batch_32, &BatchActivity::create, 1, true, "El boletín semanal de IA",
     "El boletín de DeepLearning.AI noticia a noticia, con archivo desde el primer número, favoritos y lectura sin red "
     "de lo ya abierto."},
    {"DISCOVER", &icon_discover_32, &DiscoverActivity::create, 1, true, "Lo nuevo de GitHub",
     "Repositorios nuevos con más estrellas, filtrados por tema, lenguaje y antigüedad; con ficha, README y lista de "
     "guardados."},
    {"APRENDE", &icon_learn_32, &LearnActivity::create, 2, false, "Una lección de 5 min al día",
     "Una idea de matemáticas, economía, electrónica o IA con un ejemplo y una pregunta. Cuarenta lecciones y racha "
     "diaria."},
    {"EFEMERIDES", &icon_history_32, &HistoryActivity::create, 0, true, "Qué pasó un día como hoy",
     "Un hecho de ciencia, uno de historia y uno de cultura en portada, y la lista completa del día detrás. De "
     "Wikipedia en español."},
    {"INSTAPAPER", &icon_instapaper_32, &InstapaperActivity::create, 1, true, "Tu lista de lectura pendiente",
     "Tu cola de Instapaper sincronizada en los dos sentidos. Se empareja a través de un servidor del autor original "
     "del firmware."},
    {"WALLPAPERS", &icon_wallpapers_32, &WallpapersActivity::create, 3, false, "El fondo de reposo",
     "Elige una imagen o un conjunto de ellas para la pantalla de reposo, y cada cuánto cambia con el marco de fotos."},
    {"MARCO", &icon_frame_32, &FrameActivity::create, 3, false, "Cambia la foto de reposo sola",
     "El marco de fotos: cada cuánto cambia la imagen de la pantalla de reposo mientras la Sticky duerme. Las fotos "
     "se eligen en Wallpapers."},
    {"JUKEBOX", &icon_jukebox_32, &JukeboxActivity::create, 3, true, "Mando para Plexamp",
     "Un mando para Plexamp: lo que suena en un reproductor de tu red, con su carátula, y pausa, salto y volumen desde "
     "aquí. Se enlaza a tu cuenta de Plex con un código."},
    {"WIKIPEDIA", &icon_wikipedia_32, &WikipediaActivity::create, 1, false, "50.000 artículos sin internet",
     "Cincuenta mil artículos en la tarjeta, que se leen como un libro. En inglés."},
    {"CALCULATOR", &icon_calculator_32, &CalculatorActivity::create, 3, false, "Calculadora de teclas grandes",
     "Con teclas del tamaño de un pulgar y el historial de las cuentas que ya has hecho."},
    // Card #516. The icon is the Lucide list mark borrowed from Murdle while
    // the screens are being chosen; a Notes mark comes with the real app.
    {"NOTES", &icon_murdle_face_clues_32, &NotesActivity::create, 3, false, "Listas para marcar",
     "Listas que tachas con una mano, guardadas como archivos de texto en la tarjeta."},
    {"HOY", &icon_hoy_32, &TodayActivity::create, 0, true, "El día en una página",
     "Una página para dejar en el soporte, en diez estilos que se cambian deslizando. Tocar cada parte abre su app."},
    {"CASA", &icon_casa_32, &CasaActivity::create, 0, false, "Temperatura y humedad",
     "Con el sensor de la propia Sticky, leído cada tres segundos, y las últimas 24 horas en una gráfica."},
    {"RELOJ", &icon_clock_32, &ClockActivity::create, 0, false, "Reloj de mesa, siete esferas",
     "En vertical u horizontal, en claro u oscuro: trazo, paletas, esfera, anillos, matriz, segmentos y palabras."},
    {"POMODORO", &icon_pomodoro_32, &PomodoroActivity::create, 3, false, "Temporizador de concentración",
     "Sesiones de trabajo y descanso; pita al acabar cada una."},
    {"CHESS CLOCK", &icon_chessclock_32, &ChessClockActivity::create, 3, false, "Reloj de ajedrez",
     "Dos relojes para un tablero entre los dos; cada uno toca su mitad de la pantalla."},
    {"HANDWRITING", &icon_notebook_32, &HandwritingActivity::create, 3, false, "Libreta para el dedo",
     "Páginas para escribir y dibujar; cada una se guarda también como imagen."},
    {"WEATHER", &icon_weather_32, &WeatherActivity::create, 0, true, "Previsión para tus ciudades",
     "De Open-Meteo, hasta seis ciudades, con tableros de tres y de cinco y actualización automática."},
    {"NAVAL WEATHER", &icon_naval_32, &NavalActivity::create, 0, true, "Viento, oleaje y agua",
     "El parte marítimo de los sitios guardados en Weather: viento en nudos, olas, mar de fondo y temperatura del "
     "agua."},
    {"MARKETS", &icon_markets_32, &MarketsActivity::create, 0, true, "Tres valores y tu cartera",
     "Tres valores a elegir en un panel para la mesa, búsqueda por ticker o nombre, detalle de cada uno y una "
     "cartera."},
    {"FM-1 GUIAS", &icon_fmlab_32, &FmGuideActivity::create, 2, false, "Guías de firmwares de la FM-1",
     "Guías por capítulos, con el panel dibujado, de cuatro firmwares de la comunidad para la M-VAVE FM-1."},
    {"MUSIC THEORY", &icon_theory_32, &TheoryActivity::create, 2, false, "Escalas, acordes y ritmos",
     "Escalas, acordes y progresiones en cualquier tonalidad sobre un teclado, y ritmos euclidianos en una rueda."},
    {"RPN 12C", &icon_rpn_32, &RpnActivity::create, 3, false, "Calculadora financiera RPN",
     "Al estilo de la HP-12C: pila de cuatro niveles, n, i, PV, PMT, FV, NPV e IRR."},
};

// The two rows Home grows, in reading order. Titles are Title Case because
// these sit in upstream's Home list and have to look like it; the folder screen
// shouts its own header, which is our side of the line. A third folder is one row here and
// nothing else: the Home hook counts this table rather than knowing its length.
// Neither folder shows the player bar: the name it leads to is for playing
// against a second reader in the room, and this fork runs on one.
constexpr shelf::Folder kFolders[] = {
    {"Games", UIIcon::Games, &icon_games_32, kGames, static_cast<int>(sizeof(kGames) / sizeof(shelf::Item)), false,
     kGameTabs, static_cast<int>(sizeof(kGameTabs) / sizeof(kGameTabs[0]))},
    {"Apps", UIIcon::Apps, &icon_apps_32, kApps, static_cast<int>(sizeof(kApps) / sizeof(shelf::Item)), false, kAppTabs,
     static_cast<int>(sizeof(kAppTabs) / sizeof(kAppTabs[0]))},
};

constexpr int kFolderCount = static_cast<int>(sizeof(kFolders) / sizeof(kFolders[0]));

// An item with no icon draws a blank gutter and nothing says so. Caught at
// compile time rather than in a test, because a test can be forgotten and this
// cannot: a new row without an icon does not build.
// Sorting into tabs must not lose anything: every item names a kind its folder
// has, so it is on exactly one of the kind tabs, and every kind has something
// in it, so no tab opens empty. The last tab lists the whole folder besides.
constexpr bool everyItemHasItsTab() {
  for (const auto& folder : kFolders) {
    for (int i = 0; i < folder.count; ++i) {
      const shelf::Item& item = folder.items[i];
      if (item.category >= folder.tabCount) return false;
      if (item.blurb == nullptr || item.blurb[0] == '\0' || item.about == nullptr || item.about[0] == '\0')
        return false;
    }
    for (int tab = 0; tab < folder.tabCount; ++tab) {
      bool used = false;
      for (int i = 0; i < folder.count; ++i) used = used || folder.items[i].category == tab;
      if (!used) return false;
    }
  }
  return true;
}
static_assert(everyItemHasItsTab(), "a shelf item is in no tab, has no description, or a tab is empty");

constexpr bool everyItemHasAnIcon() {
  for (const auto& folder : kFolders) {
    for (int i = 0; i < folder.count; ++i) {
      if (folder.items[i].icon == nullptr) return false;
    }
  }
  return true;
}
static_assert(everyItemHasAnIcon(), "every shelf item needs an icon; see tools_local/toybox/icons.txt");

constexpr bool everyFolderHasAMark() {
  for (const auto& folder : kFolders) {
    if (folder.mark == nullptr) return false;
  }
  return true;
}
static_assert(everyFolderHasAMark(), "every shelf folder needs a mark; see tools_local/toybox/icons.txt");

// Beside the reader's own state and the player's name, so clearing
// `.crosspoint/` clears this too and there is one place to look. Inside the
// guard because the host build has no storage and an unused constant is a
// -Werror failure there.
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kStatePath[] = "/.crosspoint/shelf.cfg";
// Beside it rather than inside it. The position is one short line a navigation
// rewrites constantly; this is a list that changes a handful of times in a
// device's life, and the two have no reason to share a write, a buffer or a
// parser -- shelf.cfg's is a fixed 96 bytes precisely because it is small.
constexpr char kHiddenPath[] = "/.crosspoint/shelf-hidden.cfg";
constexpr char kRecentPath[] = "/.crosspoint/shelf-recent.cfg";
constexpr char kFavouritesPath[] = "/.crosspoint/shelf-favs.cfg";
#endif

// Where leave() sends an app. Set when an item is opened, read when it leaves.
// -1 means "nothing is open below Home", which is what a folder itself sees.
int openFolderIndex = -1;

// Set when a folder or an app of ours is opened, taken by Home on entry. See
// takeHomeReturn().
bool homeReturn = false;

// The remembered position, mirroring /.crosspoint/shelf.cfg:
//
// - `lastFolder`: which shelf row Home should land on when you come back out.
//   CrossPoint restores Home's selection by matching the departing activity's
//   name against its own HomeMenuItem list, which cannot know about ours, so
//   without this you leave GAMES and the cursor is sitting on Browse Files.
// - `resumeRow`: per folder, the row it reopens on. The row of the item last
//   opened from it, or -- when you paged and then walked out without opening
//   anything -- the first row of the page you were looking at. It is the page
//   you were ON, not the page holding the game you last played; those are the
//   same thing until you browse and leave, and browsing and leaving is the case
//   that was wrong.
// - `openTitle`: the item that was open when the device went to sleep, which is
//   what wake reopens instead of dropping you on Home.
//
// All of it survives the device going to sleep, which none of it did before.
// `main.cpp` deep-sleeps on the idle timeout and says of it that wake is
// effectively a chip reset, so every time Mario put the device down and came
// back the shelf had forgotten which game he was playing.
//
// Written next to the reader's own state rather than into CrossPointState,
// which is upstream's file: a fork-local fact belongs in a fork-local file, and
// player.cfg already established the pattern.
shelf::State state;
bool stateLoaded = false;

// The items no folder shows, by title, mirroring shelf-hidden.cfg. Loaded on
// the first question anyone asks of it and not at boot, because the shelf has
// no init hook -- the same lazy load the position uses, for the same reason.
shelf::HiddenSet hiddenItems;
shelf::HiddenSet recentItems;
shelf::HiddenSet favouriteItems;
bool hiddenLoaded = false;

// The Activity that the open item launched, by name. `openFolderIndex` alone
// cannot answer "is that item still what is on screen": the Home gesture leaves
// an app without going through leave(), so a sleep from Home would otherwise
// record the game you left ten minutes ago as still open. A pushed sub-screen
// (the frontlight panel) also fails this check, and falls back to Home the way
// every wake used to.
std::string openActivityName;

// A title that does not fit the state file cannot be resumed, and would fail
// silently at the write. Caught at compile time instead, next to the icon and
// mark checks, so a long-titled new game does not build.
constexpr bool everyTitleFitsTheStateFile() {
  for (const auto& folder : kFolders) {
    for (int i = 0; i < folder.count; ++i) {
      if (shelf::constexprLength(folder.items[i].title) > shelf::MAX_ITEM_TITLE) return false;
    }
  }
  return true;
}
static_assert(everyTitleFitsTheStateFile(), "shelf item titles must fit shelf::MAX_ITEM_TITLE; see ShelfState.h");
static_assert(kFolderCount <= shelf::MAX_FOLDERS, "raise shelf::MAX_FOLDERS in ShelfState.h");

// Row limits as the registry stands now, for parseState's clamp.
const int* itemLimits() {
  static int limits[shelf::MAX_FOLDERS] = {};
  for (int i = 0; i < kFolderCount; ++i) limits[i] = kFolders[i].count - 1;
  return limits;
}

// The folder and row of the item with this title, case-insensitively. The one
// place a title is turned back into a row, shared by wake and by the
// autostart environment variable.
bool findItemByTitle(const char* title, int& folder, int& item) {
  for (int f = 0; f < kFolderCount; ++f) {
    for (int i = 0; i < kFolders[f].count; ++i) {
      if (strcasecmp(kFolders[f].items[i].title, title) == 0) {
        folder = f;
        item = i;
        return true;
      }
    }
  }
  return false;
}

// The parse and the format live in ShelfState.cpp, where a host test can reach
// them without a card: the file has to survive a truncated write, a file
// written before wake could resume, and a game renamed since it was written.
void loadState() {
  stateLoaded = true;
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.exists(kStatePath)) return;
  char buffer[96] = {};
  if (Storage.readFileToBuffer(kStatePath, buffer, sizeof(buffer)) == 0) return;
  // Fails quietly: the worst a corrupt file can cost is starting at the top,
  // and there is nothing for anyone to do about it.
  shelf::parseState(buffer, kFolderCount, itemLimits(), state);
#endif
}

void saveState() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  char line[96];
  const size_t used = shelf::formatState(state, kFolderCount, line, sizeof(line));
  if (used == 0) {
    LOG_ERR("SHELF", "State line did not fit %d bytes", static_cast<int>(sizeof(line)));
    return;
  }
  Storage.writeFile(kStatePath, String(line));
#endif
}

// Every path that reads or writes the remembered position goes through this
// first. Lazily rather than at boot because the shelf has no init hook, and
// unconditionally rather than only on the read paths because openFolder passes
// the current resumeRow back in: without the load, the first navigation of a
// session would write the defaults over the saved file and the persistence
// would silently do nothing.
void ensureLoaded() {
  if (!stateLoaded) loadState();
}

// The hidden list, read once. Read whole rather than into a fixed buffer: the
// worst case is every item in the registry, and that grows every time Mario
// adds a game -- a buffer sized for today is a setting silently lost on the
// day the twenty-first one lands.
void ensureHiddenLoaded() {
  if (hiddenLoaded) return;
  hiddenLoaded = true;
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (Storage.exists(kRecentPath)) shelf::parseHidden(Storage.readFile(kRecentPath).c_str(), recentItems);
  if (Storage.exists(kFavouritesPath)) shelf::parseHidden(Storage.readFile(kFavouritesPath).c_str(), favouriteItems);
  if (!Storage.exists(kHiddenPath)) return;
  shelf::parseHidden(Storage.readFile(kHiddenPath).c_str(), hiddenItems);
#endif
}

void saveTitles(const char* path, const shelf::HiddenSet& titles) {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.writeFile(path, String(shelf::formatHidden(titles).c_str()))) {
    LOG_ERR("SHELF", "Could not write %s", path);
  }
#else
  (void)path;
  (void)titles;
#endif
}

void saveHiddenItems() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  // An empty set writes an empty file rather than removing it: a file that
  // exists and says nothing is hidden is one state, and a missing file that
  // means the same thing is the same state by another route. One write path,
  // and `exists` above is the only place that has to know both.
  if (!Storage.writeFile(kHiddenPath, String(shelf::formatHidden(hiddenItems).c_str()))) {
    LOG_ERR("SHELF", "Could not write %s; the list is only in RAM until the next boot", kHiddenPath);
  }
#endif
}

// Only when something actually changed. Opening a folder happens on every Back,
// and SPIFFS sectors have a finite erase count, so an unconditional write here
// would be a write per navigation for no gain.
void saveIfChanged(const int folder, const int row) {
  ensureLoaded();
  if (state.lastFolder == folder && (folder < 0 || state.resumeRow[folder] == row)) return;
  state.lastFolder = folder;
  if (folder >= 0) state.resumeRow[folder] = row;
  saveState();
}

// The item wake should reopen, or none. Kept separate from saveIfChanged
// because the two facts change on different events: the position changes as you
// navigate, this changes when an item opens, closes, or is left behind.
void setOpenTitle(const char* title) {
  ensureLoaded();
  const char* wanted = title == nullptr ? "" : title;
  if (strcmp(state.openTitle, wanted) == 0) return;
  snprintf(state.openTitle, sizeof(state.openTitle), "%s", wanted);
  saveState();
}

// Replaces the running activity, or logs and stays put. Every launch in this
// file funnels through here so an OOM cannot leave the shelf thinking it opened
// something it did not.
bool replaceWith(std::unique_ptr<Activity> activity, const char* what) {
  if (!activity) {
    LOG_ERR("SHELF", "OOM opening %s", what);
    return false;
  }
  activityManager.replaceActivity(std::move(activity));
  // Read back from the activity itself rather than written down beside the
  // registry: a table mapping titles to activity names would be a second copy
  // of a fact the activity already carries, and would rot the first time one is
  // renamed. Asked after the request, because Activity::name is not ours to
  // read directly.
  openActivityName = activityManager.currentActivityName();
  return true;
}

}  // namespace

namespace shelf {

const Folder* folders() { return kFolders; }

int folderCount() { return kFolderCount; }

void openFolder(const int index, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  if (index < 0 || index >= kFolderCount) {
    LOG_ERR("SHELF", "Bad folder index: %d", index);
    return;
  }
  // Opening a folder means nothing below it is open any more. Clearing here
  // rather than in leave() keeps the fact true even when a folder is reached by
  // some route that did not go through leave().
  openFolderIndex = -1;
  homeReturn = true;
  ensureLoaded();
  saveIfChanged(index, state.resumeRow[index]);
  setOpenTitle(nullptr);
  replaceWith(ShelfFolderActivity::create(renderer, mappedInput, index), kFolders[index].title);
}

namespace {
// Where leave() goes instead of the folder, once. Set by openFrom().
char returnTitle[24] = "";
bool keepReturn = false;

bool findTitle(const char* title, int& folder, int& item) {
  if (title == nullptr || *title == '\0') return false;
  for (int f = 0; f < kFolderCount; ++f) {
    for (int i = 0; i < kFolders[f].count; ++i) {
      if (std::strcmp(kFolders[f].items[i].title, title) == 0) {
        folder = f;
        item = i;
        return true;
      }
    }
  }
  return false;
}
}  // namespace

bool openFrom(const char* title, const char* returnTo, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  int folder = 0;
  int item = 0;
  if (!findTitle(title, folder, item)) {
    LOG_ERR("SHELF", "No item titled %s", title == nullptr ? "" : title);
    return false;
  }
  std::snprintf(returnTitle, sizeof(returnTitle), "%s", returnTo == nullptr ? "" : returnTo);
  keepReturn = true;
  const bool opened = openItem(folder, item, renderer, mappedInput);
  keepReturn = false;
  if (!opened) returnTitle[0] = '\0';
  return opened;
}

bool openItem(const int folder, const int item, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Any other way of opening something forgets where a link meant to return.
  if (!keepReturn) returnTitle[0] = '\0';

  if (folder < 0 || folder >= kFolderCount) {
    LOG_ERR("SHELF", "Bad folder index: %d", folder);
    return false;
  }
  const Folder& parent = kFolders[folder];
  if (item < 0 || item >= parent.count) {
    LOG_ERR("SHELF", "Bad item index %d in %s", item, parent.title);
    return false;
  }

  // Recorded before the launch, not after: replaceActivity destroys the caller,
  // so there is no "after" to run in.
  openFolderIndex = folder;
  homeReturn = true;
  ensureHiddenLoaded();
  if (noteRecent(recentItems, parent.items[item].title)) saveTitles(kRecentPath, recentItems);
  saveIfChanged(folder, item);
  setOpenTitle(parent.items[item].title);
  if (!replaceWith(parent.items[item].create(renderer, mappedInput), parent.items[item].title)) {
    openFolderIndex = -1;
    setOpenTitle(nullptr);
    return false;
  }
  return true;
}

// A folder's titles, by row, for the conversions in ShelfHidden.h.
auto titlesOf(const int folder) {
  return [folder](const int i) { return kFolders[folder].items[i].title; };
}

bool isHidden(const int folder, const int item) {
  if (folder < 0 || folder >= kFolderCount) return false;
  if (item < 0 || item >= kFolders[folder].count) return false;
  ensureHiddenLoaded();
  return hiddenItems.contains(kFolders[folder].items[item].title);
}

void setHidden(const int folder, const int item, const bool hide) {
  if (folder < 0 || folder >= kFolderCount) {
    LOG_ERR("SHELF", "Bad folder index: %d", folder);
    return;
  }
  if (item < 0 || item >= kFolders[folder].count) {
    LOG_ERR("SHELF", "Bad item index %d in %s", item, kFolders[folder].title);
    return;
  }
  ensureHiddenLoaded();
  if (!hiddenItems.set(kFolders[folder].items[item].title, hide)) return;
  LOG_INF("SHELF", "%s is now %s", kFolders[folder].items[item].title, hide ? "hidden" : "shown");
  saveHiddenItems();
}

// The three of them are the same conversion asked three ways, and it lives in
// ShelfHidden.h where a host test can reach it: this file cannot be built off a
// device. All each one does here is bind the folder's titles to it.
int shownCount(const int folder) {
  if (folder < 0 || folder >= kFolderCount) return 0;
  ensureHiddenLoaded();
  return shownCountIn(hiddenItems, kFolders[folder].count, titlesOf(folder));
}

int shownItem(const int folder, const int row) {
  if (folder < 0 || folder >= kFolderCount) return -1;
  ensureHiddenLoaded();
  return orderedItemIn(hiddenItems, recentItems, kFolders[folder].count, row, titlesOf(folder));
}

int recents(const int folder, int* out, const int max) {
  if (folder < 0 || folder >= kFolderCount) return 0;
  ensureHiddenLoaded();
  const shelf::Folder& self = kFolders[folder];
  int n = 0;
  for (size_t r = recentItems.size(); r > 0 && n < max; --r) {
    const std::string& title = recentItems.at(r - 1);
    if (hiddenItems.contains(title.c_str())) continue;
    for (int i = 0; i < self.count; ++i) {
      if (title == self.items[i].title) {
        out[n++] = i;
        break;
      }
    }
  }
  return n;
}

int shownRowFor(const int folder, const int item) {
  if (folder < 0 || folder >= kFolderCount) return 0;
  ensureHiddenLoaded();
  return orderedRowForIn(hiddenItems, recentItems, kFolders[folder].count, item, titlesOf(folder));
}

void autostartFromEnv(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Once per process: leaving the app afterwards must land on the shelf like
  // any other exit, not bounce straight back in.
  static bool consumed = false;
  if (consumed) {
    return;
  }
  const char* wanted = std::getenv("CROSSPLAY_AUTOSTART");
  if (wanted == nullptr || *wanted == '\0') {
    return;
  }
  consumed = true;
  int folder = -1;
  int item = -1;
  if (!findItemByTitle(wanted, folder, item)) {
    LOG_ERR("SHELF", "Autostart: no item titled '%s'", wanted);
    return;
  }
  LOG_INF("SHELF", "Autostart into %s", kFolders[folder].items[item].title);
  openItem(folder, item, renderer, mappedInput);
}

void rememberForWake(const char* currentActivityName) {
  ensureLoaded();
  if (state.openTitle[0] == '\0') return;
  // An item is only still open if the activity it launched is the one on
  // screen. Leaving a game by the Home gesture never passes through leave(),
  // so without this a sleep taken on Home would resume into the game you left.
  const char* onScreen = currentActivityName == nullptr ? "" : currentActivityName;
  if (openActivityName.empty() || openActivityName != onScreen) {
    LOG_DBG("SHELF", "Sleeping on %s, not %s: nothing to resume", onScreen, state.openTitle);
    setOpenTitle(nullptr);
  }
}

bool resumeFromWake(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  ensureLoaded();
  if (state.openTitle[0] == '\0') return false;

  int folder = -1;
  int item = -1;
  if (!findItemByTitle(state.openTitle, folder, item)) {
    // The card outlives firmware updates, so the item may have been renamed or
    // removed since it was written. Home, and forget it.
    LOG_INF("SHELF", "Wake: nothing titled '%s' any more", state.openTitle);
    setOpenTitle(nullptr);
    return false;
  }

  LOG_INF("SHELF", "Wake: resuming %s", kFolders[folder].items[item].title);
  return openItem(folder, item, renderer, mappedInput);
}

void openPlayer(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Only the folder footer offers it, and only a folder that shows the name, so
  // lastFolder is the folder we are standing in. Recorded before the launch for
  // the same reason openItem does it: replaceActivity destroys the caller.
  ensureLoaded();
  openFolderIndex = state.lastFolder;
  homeReturn = true;
  setOpenTitle(nullptr);
  if (!replaceWith(PlayerActivity::create(renderer, mappedInput), "Player")) {
    openFolderIndex = -1;
  }
}

void leave(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  if (returnTitle[0] != '\0') {
    char title[sizeof(returnTitle)];
    std::snprintf(title, sizeof(title), "%s", returnTitle);
    returnTitle[0] = '\0';
    int folder = 0;
    int item = 0;
    if (findTitle(title, folder, item) && openItem(folder, item, renderer, mappedInput)) return;
  }
  if (openFolderIndex >= 0) {
    openFolder(openFolderIndex, renderer, mappedInput);
    return;
  }
  activityManager.goHome();
}

int favourites(const int folder, int* out) {
  if (folder < 0 || folder >= kFolderCount) return 0;
  ensureHiddenLoaded();
  return favouriteItemsIn(hiddenItems, favouriteItems, kFolders[folder].count, titlesOf(folder), out);
}

bool isFavourite(const int folder, const int item) {
  if (folder < 0 || folder >= kFolderCount || item < 0 || item >= kFolders[folder].count) return false;
  ensureHiddenLoaded();
  return favouriteItems.contains(kFolders[folder].items[item].title);
}

void toggleFavourite(const int folder, const int item) {
  if (folder < 0 || folder >= kFolderCount) return;
  ensureHiddenLoaded();
  toggleFavouriteIn(favouriteItems, kFolders[folder].count, item, titlesOf(folder));
  saveTitles(kFavouritesPath, favouriteItems);
}

int lastFolderOnHome() {
  ensureLoaded();
  return state.lastFolder;
}

bool takeHomeReturn() {
  const bool was = homeReturn;
  homeReturn = false;
  return was;
}

int resumeRowIn(const int index) {
  ensureLoaded();
  if (index < 0 || index >= kFolderCount) return 0;
  // Stored as the ITEM, answered as the ROW. The conversion is here, beside the
  // file, and not in the folder that asks: those are the only two units in this
  // feature and the whole hazard is a caller holding one while believing the
  // other -- they are both small ints in the same range, so nothing would say
  // so. The folder does hold items, at the two points where it must (opening
  // one, hiding one) and through ONE named function that produces them
  // (ShelfFolderActivity::itemAtRow); what it never does is store one in a
  // variable that means a row.
  return shownRowFor(index, state.resumeRow[index]);
}

void rememberRowIn(const int index, const int row) {
  if (index < 0 || index >= kFolderCount) {
    LOG_ERR("SHELF", "Bad folder index: %d", index);
    return;
  }
  // `row` is a SHOWN row, so this is also the range check: a row past the end
  // of what the folder is showing has no item and is refused, which is the same
  // guard the registry count used to give when the two were the same number.
  const int item = shownItem(index, row);
  if (item < 0) {
    LOG_ERR("SHELF", "Bad row %d in %s", row, kFolders[index].title);
    return;
  }
  saveIfChanged(index, item);
}

}  // namespace shelf

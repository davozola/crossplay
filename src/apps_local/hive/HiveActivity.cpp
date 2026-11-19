#include "HiveActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxHowTo.h"
#include "../ui/ToyboxTheme.h"

namespace hv = hive;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/hive.sav";
constexpr char kLevelsPath[] = "/.crosspoint/hive-levels.cfg";
constexpr uint8_t kSaveVersion = 1;
constexpr int kBackSwipeEdge = 12;

struct SaveState {
  uint8_t mode;
  uint8_t inProgress;
  uint8_t turn;
  uint8_t turnsTaken[2];
  int8_t lastPiece;
  hv::Piece pieces[hv::kPieces];
};

const char* sideName(const int color) { return color == 0 ? "WHITE" : "BLACK"; }

constexpr const char* kHowToPages[] = {
    "EACH SIDE HAS ONE QUEEN, TWO BEETLES, TWO SPIDERS, THREE GRASSHOPPERS AND THREE ANTS. THERE IS NO BOARD: THE "
    "PIECES MAKE IT. YOU WIN WHEN THE OTHER QUEEN IS SURROUNDED ON ALL SIX SIDES, BY PIECES OF EITHER COLOUR.",
    "ON YOUR TURN, PLACE A PIECE OR MOVE ONE. TAP IT, THEN TAP A MARKED SPACE. A NEW PIECE MUST TOUCH YOUR OWN "
    "PIECES AND NONE OF THE OTHER SIDE'S; ONLY THE VERY FIRST PIECES ARE EXCUSED. YOUR QUEEN MUST BE DOWN BY YOUR "
    "FOURTH TURN, AND NOTHING OF YOURS MOVES UNTIL SHE IS.",
    "QUEEN: ONE STEP. BEETLE: ONE STEP, AND IT MAY CLIMB ON TOP OF THE HIVE, PINNING WHAT IS UNDER IT. SPIDER: "
    "EXACTLY THREE STEPS ROUND THE EDGE. GRASSHOPPER: JUMPS IN A STRAIGHT LINE OVER PIECES TO THE FIRST GAP. ANT: "
    "ANY DISTANCE ROUND THE EDGE.",
    "TWO LIMITS. THE HIVE MUST STAY IN ONE PIECE, EVEN WHILE A PIECE IS LIFTED: ONE THAT HOLDS TWO PARTS TOGETHER "
    "CANNOT MOVE. AND PIECES SLIDE, SO THEY CANNOT SQUEEZE THROUGH A GAP NARROWER THAN THEMSELVES. IF YOU CAN "
    "NEITHER PLACE NOR MOVE, YOUR TURN PASSES.",
};
constexpr toyboxhowto::Guide kHowTo{kHowToPages, 4};

}  // namespace

std::unique_ptr<Activity> HiveActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<HiveActivity>(renderer, mappedInput);
}

void HiveActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  buffers_ = makeUniqueNoThrow<hv::SearchBuffers>();
  if (!buffers_) LOG_ERR("HIVE", "OOM: search buffers");
  rng_.state = static_cast<uint32_t>(millis()) * 2654435761u + 3u;
  loadState();
  sologame::loadStats("HIVE", kLevelsPath, levelStats_);
  history_.clear();
  view_ = View::Menu;
  requestUpdate();
}

void HiveActivity::onExit() {
  saveState();
  buffers_.reset();
  Activity::onExit();
}

void HiveActivity::startGame(const int mode) {
  mode_ = mode;
  history_.clear();
  game_.reset();
  inProgress_ = true;
  selected_ = -1;
  targetCount_ = 0;
  lastPiece_ = -1;
  thinking_ = false;
  view_ = View::Play;
  settle();
}

void HiveActivity::settle() {
  selected_ = -1;
  targetCount_ = 0;
  const int w = game_.winner();
  if (w >= 0) {
    if (inProgress_ && mode_ != kTwoPlayers) {
      // The player is white. Counted once: inProgress_ drops right here.
      levelStats_.count(mode_, w == 0 ? 0 : (w == 1 ? 1 : 2));
      sologame::saveStats("HIVE", kLevelsPath, levelStats_);
    }
    history_.clear();
    inProgress_ = false;
    thinking_ = false;
    if (w == 2) {
      std::snprintf(status_, sizeof(status_), "BOTH QUEENS SURROUNDED: A DRAW");
    } else {
      std::snprintf(status_, sizeof(status_), "%s WINS. BACK FOR A NEW GAME", sideName(w));
    }
    return;
  }
  // A side with nothing to do passes, and the turn is said to be the other's.
  bool passed = false;
  if (buffers_) {
    const int n = game_.legalMoves(buffers_->moves[0], hv::kMaxMoves);
    if (n == 1 && buffers_->moves[0][0].pass()) {
      game_.apply(buffers_->moves[0][0]);
      passed = true;
    }
  }
  const int side = game_.turn;
  const bool queenDue = game_.turnsTaken[side] >= 3 && !game_.queenPlaced(side);
  if (deviceToMove()) {
    std::snprintf(status_, sizeof(status_), "BLACK IS THINKING");
    thinking_ = true;
  } else if (passed) {
    std::snprintf(status_, sizeof(status_), "%s HAD NO MOVE. %s AGAIN", sideName(1 - side), sideName(side));
  } else if (queenDue) {
    std::snprintf(status_, sizeof(status_), "%s: YOUR QUEEN GOES DOWN NOW", sideName(side));
  } else {
    std::snprintf(status_, sizeof(status_), "%s TO MOVE", sideName(side));
  }
}

void HiveActivity::select(const int piece) {
  selected_ = -1;
  targetCount_ = 0;
  if (piece < 0) return;
  targetCount_ = game_.destinations(piece, targets_, static_cast<int>(sizeof(targets_) / sizeof(targets_[0])));
  if (targetCount_ > 0) {
    selected_ = piece;
    return;
  }
  const hv::Piece& p = game_.pieces[piece];
  if (p.color != game_.turn) return;
  if (game_.turnsTaken[game_.turn] >= 3 && !game_.queenPlaced(game_.turn)) {
    std::snprintf(status_, sizeof(status_), "THE QUEEN FIRST: IT IS YOUR FOURTH TURN");
  } else if (p.placed && !game_.queenPlaced(game_.turn)) {
    std::snprintf(status_, sizeof(status_), "NOTHING MOVES UNTIL YOUR QUEEN IS DOWN");
  } else if (p.placed) {
    std::snprintf(status_, sizeof(status_), "THAT PIECE CANNOT MOVE: THE HIVE WOULD SPLIT");
  } else {
    std::snprintf(status_, sizeof(status_), "NOWHERE TO PLACE THAT");
  }
}

// Back to before the last move made by hand; the device's answer goes with it.
void HiveActivity::takeBack() {
  if (thinking_ || deviceToMove() || game_.winner() >= 0) return;
  if (!history_.pop(game_)) return;
  lastPiece_ = -1;
  settle();
  saveState();
}

void HiveActivity::play(const hv::Move& move) {
  history_.push(game_);
  game_.apply(move);
  lastPiece_ = move.piece;
  haptic_feedback::touchAction();
  settle();
}

void HiveActivity::boardTap(const int x, const int y) {
  const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
  const hiveui::Layout layout = hiveui::layoutOf(device, game_);
  const int reach = static_cast<int>(layout.size * layout.size * 0.8f);
  auto near = [&](const int q, const int r) {
    int cx = 0, cy = 0;
    layout.centre(q, r, cx, cy);
    const int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    return d <= reach ? d : -1;
  };
  // A lit cell first: that is what the tap is most likely for.
  int best = -1;
  int bestDistance = 0;
  for (int i = 0; i < targetCount_; ++i) {
    const int d = near(targets_[i].q, targets_[i].r);
    if (d >= 0 && (best < 0 || d < bestDistance)) {
      best = i;
      bestDistance = d;
    }
  }
  if (best >= 0) {
    play(targets_[best]);
    return;
  }
  for (int i = 0; i < hv::kPieces; ++i) {
    const hv::Piece& p = game_.pieces[i];
    if (!p.placed || game_.topAt(p.q, p.r) != i || near(p.q, p.r) < 0) continue;
    if (i == selected_) {
      select(-1);
    } else {
      settle();  // the plain whose-turn line, before select() may replace it
      select(i);
    }
    return;
  }
  select(-1);
}

void HiveActivity::loop() {
  if (thinking_ && view_ == View::Play) {
    // The THINKING frame has to be on the panel before the search blocks.
    requestUpdateAndWait();
    thinking_ = false;
    if (buffers_ && deviceToMove()) {
      const hv::Move move = hv::chooseMove(game_, mode_, rng_, *buffers_);
      game_.apply(move);
      lastPiece_ = move.piece;
      settle();
      saveState();
    }
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (howTo_.open) {
      howTo_.back();
      requestUpdate();
      return;
    }
    if (view_ == View::Play) {
      saveState();
      view_ = View::Menu;
      requestUpdate();
      return;
    }
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);
  if (howTo_.open) {
    if (howTo_.tap(event, kHowTo)) requestUpdate();
    return;
  }
  switch (event.action) {
    case toyboxhowto::kOpen:
      howTo_.start();
      break;
    case hiveui::ActionStart:
      if (event.value < 0 || event.value > kTwoPlayers) return;
      startGame(event.value);
      break;
    case hiveui::ActionContinue:
      if (!inProgress_) return;
      view_ = View::Play;
      settle();
      break;
    case hiveui::ActionUndo:
      if (view_ != View::Play) return;
      takeBack();
      break;
    case hiveui::ActionHand: {
      if (view_ != View::Play || game_.winner() >= 0 || deviceToMove()) return;
      const int piece = game_.handPiece(game_.turn, static_cast<hv::Bug>(event.value));
      if (piece < 0) return;
      if (piece == selected_) {
        select(-1);
      } else {
        settle();
        select(piece);
      }
      break;
    }
    default:
      if (view_ != View::Play || tapX < kBackSwipeEdge || game_.winner() >= 0 || deviceToMove()) return;
      boardTap(tapX, tapY);
      break;
  }
  requestUpdate();
}

void HiveActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen surface(frame);

  if (howTo_.open) {
    toyboxhowto::build(surface, kHowTo, howTo_);
  } else if (view_ == View::Menu) {
    hiveui::MenuModel m;
    m.canContinue = inProgress_;
    for (int i = 0; i < 3; ++i) {
      m.won[i] = levelStats_.won[i];
      m.lost[i] = levelStats_.lost[i];
    }
    hiveui::buildMenu(surface, m);
  } else {
    hiveui::PlayModel m;
    m.game = &game_;
    m.status = status_;
    m.modeLabel = mode_ == kTwoPlayers ? "2 PLAYERS" : hv::levelName(mode_);
    // Against the device the tray is always yours; across a table it is
    // whoever's turn it is.
    m.handColor = mode_ == kTwoPlayers ? game_.turn : 0;
    m.selectedPiece = selected_;
    m.targets = targets_;
    m.targetCount = targetCount_;
    m.lastPiece = lastPiece_;
    m.trayEnabled = game_.winner() < 0 && !deviceToMove();
    m.canUndo = m.trayEnabled && !thinking_ && history_.count > 0;
    hiveui::buildPlay(surface, m);
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Hive");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void HiveActivity::saveState() {
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  s->mode = static_cast<uint8_t>(mode_);
  s->inProgress = inProgress_ ? 1 : 0;
  s->turn = game_.turn;
  s->turnsTaken[0] = game_.turnsTaken[0];
  s->turnsTaken[1] = game_.turnsTaken[1];
  s->lastPiece = static_cast<int8_t>(lastPiece_);
  for (int i = 0; i < hv::kPieces; ++i) s->pieces[i] = game_.pieces[i];
  HalFile file;
  if (!Storage.openFileForWrite("HIVE", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(s.get()), sizeof(SaveState));
  file.flush();
}

void HiveActivity::loadState() {
  inProgress_ = false;
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("HIVE", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  if (file.read(reinterpret_cast<uint8_t*>(s.get()), sizeof(SaveState)) != sizeof(SaveState)) return;
  if (s->mode > kTwoPlayers || s->turn > 1) return;
  // The pieces' kinds and colours are the game's own; only where they are is
  // taken from the file, and only if it still makes one hive.
  hv::Game loaded;
  for (int i = 0; i < hv::kPieces; ++i) {
    loaded.pieces[i].placed = s->pieces[i].placed ? 1 : 0;
    loaded.pieces[i].q = s->pieces[i].q;
    loaded.pieces[i].r = s->pieces[i].r;
    loaded.pieces[i].z = s->pieces[i].z;
  }
  loaded.turn = s->turn;
  loaded.turnsTaken[0] = s->turnsTaken[0];
  loaded.turnsTaken[1] = s->turnsTaken[1];
  if (!loaded.connected()) return;
  for (int i = 0; i < hv::kPieces; ++i) {
    if (loaded.pieces[i].placed && loaded.pieces[i].z >= loaded.heightAt(loaded.pieces[i].q, loaded.pieces[i].r))
      return;
  }
  game_ = loaded;
  mode_ = s->mode;
  inProgress_ = s->inProgress != 0 && game_.winner() < 0;
  lastPiece_ = s->lastPiece >= 0 && s->lastPiece < hv::kPieces ? s->lastPiece : -1;
}

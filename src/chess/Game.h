#pragma once

#include "chess/ChessLabelRenderer.h"
#include "chess/ChessOverlay.h"
#include "chess/Engine.h"
#include "chess/GameState.h"
#include "chess/Types.h"
#include "mosaic/MosaicComposer.h"
#include "mosaic/MosaicRenderer.h"

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace chess {

class Sounds;

// Interactive chess UI built on the mosaic engine:
//   - MosaicComposer renders the animated background; its grid is independent
//     of the chess board (only its canvas dimensions must equal the overlay's).
//   - ChessOverlay (its own 8x8 layout) paints pieces on top.
//   - Game draws rank/file labels and click highlights.
//   - Engine plays the non-human side on a background thread so the mosaic
//     keeps animating while it thinks.
// Click selects a piece, second click moves it (promotion auto-queens). Keys:
// left/right review history, Home/End jump to start/live, U/Backspace take
// back, R restart, SPACE pause, ESC quit. Moves only from the live position.
class Game : public mosaic::MosaicRenderer {
public:
  Game(mosaic::MosaicComposer composer,
       ChessOverlay overlay,
       std::unique_ptr<Engine> engine,
       Color humanColor = Color::White,
       int engineThinkMs = 500,
       int targetFps = 30,
       std::string windowTitle = "gif-chess");

  void run() override;

private:
  mosaic::MosaicComposer composer_;
  ChessOverlay overlay_;
  ChessLabelRenderer labels_;
  std::unique_ptr<Engine> engine_;
  GameState gameState_;
  Color humanColor_;
  int engineThinkMs_;
  int targetFps_;
  std::string title_;

  std::optional<Position> selected_;
  std::vector<Move> selectedTargets_;

  // Position after every ply; front() is the start, back() == gameState_ (live).
  // viewPly_ is the position currently shown: the last index unless the user
  // stepped back to review. Moves only from the live position.
  std::vector<GameState> history_;
  int viewPly_ = 0;
  bool outcomeAnnounced_ = false;  // reset by restart()/takeBack()
  // Bumped whenever the live line changes out from under a running search
  // (restart / takeback), so a stale engine result can be discarded.
  unsigned posEpoch_ = 0;
  unsigned engineEpoch_ = 0;       // posEpoch_ snapshot when the search started

  // Engine move computed off the main thread. Declared after engine_ so it is
  // destroyed (joined) before engine_ (the worker holds a pointer to it).
  std::future<Move> engineFuture_;
  bool engineThinking_ = false;
  bool engineFailed_ = false;  // sticky: stop pestering a broken engine

  GameState const& displayed() const { return history_[static_cast<std::size_t>(viewPly_)]; }
  bool atLivePosition() const { return viewPly_ == static_cast<int>(history_.size()) - 1; }

  std::optional<Position> pixelToBoard(int x, int y) const;
  void onClick(int x, int y, Sounds& sounds);
  void selectSquare(Position p);
  void clearSelection();
  void stepView(int delta);   // move viewPly_ by delta, clamped; clears selection
  void restart();             // back to the starting position, fresh game
  void takeBack();            // undo the live line to the human's previous turn
  // If it's the engine's turn at the live position and it isn't already
  // thinking, kick off bestMove on a background thread (handed a snapshot).
  void startEngineThinkingIfNeeded();
  // If the background search has finished, apply its move (on this thread).
  void collectEngineMove(Sounds& sounds);
  // Applies a (legal) move to gameState_, records it in history_, and plays the
  // matching cue (check / castle / promote / capture / move). Re-throws if
  // makeMove rejects it. The game-end cue is handled by run()'s outcome block.
  void applyMoveAndAnnounce(Move const& m, Sounds& sounds);
};

}  // namespace chess

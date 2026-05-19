#include "chess/Game.h"

#include "chess/Piece.h"
#include "chess/Sounds.h"
#include "mosaic/Painter.h"
#include "mosaic/SDLTypes.h"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>

namespace chess {

namespace {

int pieceCount(Board const& b) {
  int n = 0;
  for (int i = 0; i < 64; ++i) if (b.atIndex(i)) ++n;
  return n;
}

mosaic::RGB constexpr kSelectedColor{255, 220,  80};
mosaic::RGB constexpr kTargetColor  {255, 220,  80};
mosaic::RGB constexpr kCheckColor   {255,  90,  90};

void drawCellBorder(mosaic::Painter& p, mosaic::Rect const& cell,
                    mosaic::RGB color, int thickness) {
  p.fillRect({cell.x, cell.y, cell.w, thickness}, color);
  p.fillRect({cell.x, cell.y + cell.h - thickness, cell.w, thickness}, color);
  p.fillRect({cell.x, cell.y + thickness, thickness, cell.h - 2 * thickness}, color);
  p.fillRect({cell.x + cell.w - thickness, cell.y + thickness,
              thickness, cell.h - 2 * thickness}, color);
}

void drawTargetDot(mosaic::Painter& p, mosaic::Rect const& cell, mosaic::RGB color) {
  int const cx = cell.x + cell.w / 2;
  int const cy = cell.y + cell.h / 2;
  int const r = std::max(3, cell.w / 8);
  p.fillRect({cx - r, cy - r, 2 * r, 2 * r}, color);
}

class PausableClock {
public:
  PausableClock() : start_(SDL_GetTicks()) {}
  double elapsedMs() const {
    if (paused_) return static_cast<double>(pauseAt_ - start_);
    return static_cast<double>(SDL_GetTicks() - start_);
  }
  void togglePause() {
    Uint32 const now = SDL_GetTicks();
    if (paused_) { start_ += (now - pauseAt_); paused_ = false; }
    else         { pauseAt_ = now;             paused_ = true;  }
  }

private:
  Uint32 start_;
  Uint32 pauseAt_ = 0;
  bool paused_ = false;
};

}  // namespace

Game::Game(mosaic::MosaicComposer composer, ChessOverlay overlay,
           std::unique_ptr<Engine> engine, Color humanColor, int engineThinkMs,
           int targetFps, std::string windowTitle)
    : composer_(std::move(composer)),
      overlay_(std::move(overlay)),
      engine_(std::move(engine)),
      humanColor_(humanColor),
      engineThinkMs_(engineThinkMs),
      targetFps_(targetFps),
      title_(std::move(windowTitle)) {
  if (!engine_) throw std::invalid_argument("Game: engine must not be null");
  if (targetFps_ <= 0) throw std::invalid_argument("Game: targetFps must be positive");
  if (composer_.layout().canvasWidth()  != overlay_.layout().canvasWidth() ||
      composer_.layout().canvasHeight() != overlay_.layout().canvasHeight()) {
    throw std::invalid_argument(
        "Game: composer canvas size must equal the chess overlay's layout canvas size");
  }
  history_.push_back(gameState_);  // history_[0] == the starting position
}

std::optional<Position> Game::pixelToBoard(int x, int y) const {
  auto const& layout = overlay_.layout();  // the 8x8 chess board layout
  int const m = layout.margin();
  int const cs = layout.cellSize();
  if (x < m || y < m) return std::nullopt;
  int const col = (x - m) / cs;
  int const row = (y - m) / cs;
  if (col < 0 || col >= layout.cols() || row < 0 || row >= layout.rows()) {
    return std::nullopt;
  }
  // White (rank 0) is displayed at the bottom (highest row index).
  return Position{col, layout.rows() - 1 - row};
}

void Game::selectSquare(Position p) {
  selected_ = p;
  selectedTargets_.clear();
  for (auto const& m : gameState_.legalMoves()) {
    if (m.from == p) selectedTargets_.push_back(m);
  }
}

void Game::clearSelection() {
  selected_.reset();
  selectedTargets_.clear();
}

void Game::onClick(int x, int y, Sounds& sounds) {
  if (!atLivePosition()) return;  // reviewing history - press -> (or End) to play
  if (gameState_.outcome() != GameState::Outcome::Ongoing) return;
  if (gameState_.sideToMove() != humanColor_) return;

  auto sq = pixelToBoard(x, y);
  if (!sq) { clearSelection(); return; }

  if (!selected_) {
    Piece const* p = gameState_.board().at(*sq);
    if (p && p->color() == humanColor_) selectSquare(*sq);
    return;
  }

  if (*sq == *selected_) { clearSelection(); return; }

  Move const* matching = nullptr;
  for (auto const& m : selectedTargets_) {
    if (m.to != *sq) continue;
    if (matching == nullptr || m.promotion == PieceType::Queen) {
      matching = &m;
    }
  }
  if (matching) {
    applyMoveAndAnnounce(*matching, sounds);
    clearSelection();
    return;
  }

  Piece const* p = gameState_.board().at(*sq);
  if (p && p->color() == humanColor_) {
    selectSquare(*sq);
  } else {
    clearSelection();
  }
}

void Game::startEngineThinkingIfNeeded() {
  if (engineThinking_ || engineFailed_) return;
  if (!atLivePosition()) return;  // game is "paused" while you review the past
  if (gameState_.outcome() != GameState::Outcome::Ongoing) return;
  if (gameState_.sideToMove() == humanColor_) return;
  // Engine gets its own position snapshot and thinks on a background thread.
  Engine* engine = engine_.get();
  int const thinkMs = engineThinkMs_;
  engineEpoch_ = posEpoch_;
  engineFuture_ = std::async(std::launch::async,
                             [engine, thinkMs, snap = gameState_]() {
                               return engine->bestMove(snap, thinkMs);
                             });
  engineThinking_ = true;
}

void Game::collectEngineMove(Sounds& sounds) {
  if (!engineThinking_) return;
  if (engineFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
  engineThinking_ = false;
  bool const stale = (engineEpoch_ != posEpoch_);  // restart/takeback happened mid-search

  std::optional<Move> m;
  try {
    m = engineFuture_.get();
  } catch (std::exception const& e) {
    if (!stale) { std::cerr << "[engine error] " << e.what() << "\n"; engineFailed_ = true; }
    return;
  }
  if (stale) return;  // the position this move was for no longer exists

  try {
    applyMoveAndAnnounce(*m, sounds);
  } catch (std::exception const& e) {
    std::cerr << "[engine error] illegal move " << m->toUci() << ": " << e.what() << "\n";
    engineFailed_ = true;
    return;
  }
  std::cout << "[engine] " << m->toUci() << (gameState_.isCheck() ? " +" : "") << "\n";

  // Discard clicks the human queued up while the engine was thinking.
  SDL_FlushEvent(SDL_MOUSEBUTTONDOWN);
  SDL_FlushEvent(SDL_MOUSEBUTTONUP);
}

void Game::applyMoveAndAnnounce(Move const& m, Sounds& sounds) {
  // Classify the move from the *pre-move* board.
  Piece const* mover = gameState_.board().at(m.from);
  bool const isKing = mover && mover->type() == PieceType::King;
  bool const isPawn = mover && mover->type() == PieceType::Pawn;
  int  const fileDelta = m.to.file - m.from.file;
  bool const isCastle    = isKing && (fileDelta == 2 || fileDelta == -2);
  bool const isPromotion = isPawn && (m.to.rank == 0 || m.to.rank == 7);
  int  const piecesBefore = pieceCount(gameState_.board());
  bool const wasAtLive = atLivePosition();

  gameState_.makeMove(m);  // throws if illegal - callers handle it

  history_.push_back(gameState_);
  if (wasAtLive) viewPly_ = static_cast<int>(history_.size()) - 1;  // follow the live game

  // Game-ending moves get only the game-end cue (run()'s outcome block);
  // otherwise pick the most specific cue for what just happened.
  if (gameState_.outcome() != GameState::Outcome::Ongoing) return;
  bool const isCapture = pieceCount(gameState_.board()) < piecesBefore;
  if      (gameState_.isCheck()) sounds.check();
  else if (isCastle)             sounds.castle();
  else if (isPromotion)          sounds.promote();
  else if (isCapture)            sounds.capture();
  else                           sounds.move();
}

void Game::stepView(int delta) {
  int const last = static_cast<int>(history_.size()) - 1;
  viewPly_ = std::clamp(viewPly_ + delta, 0, last);
  clearSelection();
}

void Game::restart() {
  gameState_ = GameState{};
  history_.assign(1, gameState_);
  viewPly_ = 0;
  clearSelection();
  ++posEpoch_;            // discard any engine search that was in flight
  engineFailed_ = false;  // give the engine another chance
  outcomeAnnounced_ = false;
  std::cout << "[game] restarted\n";
}

void Game::takeBack() {
  if (history_.size() <= 1) return;  // nothing played yet
  // Drop plies until it's the human's turn again (undoes engine reply + last
  // human move), or we hit the start.
  do { history_.pop_back(); }
  while (history_.size() > 1 && history_.back().sideToMove() != humanColor_);
  gameState_ = history_.back();
  viewPly_ = static_cast<int>(history_.size()) - 1;
  clearSelection();
  ++posEpoch_;
  engineFailed_ = false;
  outcomeAnnounced_ = false;
  std::cout << "[game] takeback (ply " << viewPly_ << ")\n";
}

void Game::run() {
  mosaic::SDLInit sdl;
  Sounds sounds;  // chess.com-style move/capture/game-end cues; silent if no audio

  int const w = composer_.layout().canvasWidth();
  int const h = composer_.layout().canvasHeight();

  mosaic::WindowPtr window{SDL_CreateWindow(
      title_.c_str(),
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      w, h, SDL_WINDOW_SHOWN)};
  if (!window) throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());

  mosaic::RendererPtr renderer{SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED)};
  if (!renderer) throw std::runtime_error(std::string("SDL_CreateRenderer: ") + SDL_GetError());

  mosaic::TexturePtr texture{SDL_CreateTexture(
      renderer.get(), SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, w, h)};
  if (!texture) throw std::runtime_error(std::string("SDL_CreateTexture: ") + SDL_GetError());

  PausableClock clock;
  Uint32 const frameMs = 1000u / static_cast<Uint32>(targetFps_);
  bool running = true;

  std::cout << "[game] " << w << "x" << h
            << ", you are " << (humanColor_ == Color::White ? "white" : "black")
            << ".\n  click: move   ←/→: review history   Home/End: start/live"
               "   U/Backspace: take back   R: restart   SPACE: pause   ESC: quit\n";

  sounds.gameStart();

  while (running) {
    Uint32 const frameStart = SDL_GetTicks();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) { running = false; break; }
      if (ev.type == SDL_KEYDOWN) {
        switch (ev.key.keysym.sym) {
          case SDLK_ESCAPE:    running = false; break;
          case SDLK_SPACE:     clock.togglePause(); break;
          case SDLK_LEFT:      stepView(-1); break;
          case SDLK_RIGHT:     stepView(+1); break;
          case SDLK_HOME:      viewPly_ = 0; clearSelection(); break;
          case SDLK_END:       viewPly_ = static_cast<int>(history_.size()) - 1; clearSelection(); break;
          case SDLK_u:
          case SDLK_BACKSPACE: takeBack(); break;
          case SDLK_r:         restart(); break;
          default: break;
        }
        if (!running) break;
      }
      if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        onClick(ev.button.x, ev.button.y, sounds);
      }
    }
    if (!running) break;

    // Engine runs on a worker thread; the loop keeps rendering meanwhile.
    startEngineThinkingIfNeeded();
    collectEngineMove(sounds);

    auto canvas = composer_.composite(clock.elapsedMs());
    mosaic::Painter painter(canvas.data(), w, h);
    auto const& boardLayout = overlay_.layout();

    labels_.drawLabels(painter, boardLayout);

    GameState const& shown = displayed();  // the live game, or a reviewed ply

    if (shown.isCheck()) {
      Position king;
      for (int i = 0; i < 64; ++i) {
        Piece const* p = shown.board().atIndex(i);
        if (p && p->color() == shown.sideToMove() && p->type() == PieceType::King) {
          king = Position::fromIndex(i);
          break;
        }
      }
      drawCellBorder(painter, boardLayout.cellRect(7 - king.rank, king.file), kCheckColor, 4);
    }

    if (selected_) {
      drawCellBorder(painter, boardLayout.cellRect(7 - selected_->rank, selected_->file),
                     kSelectedColor, 4);
      for (auto const& m : selectedTargets_) {
        drawTargetDot(painter, boardLayout.cellRect(7 - m.to.rank, m.to.file), kTargetColor);
      }
    }

    overlay_.draw(painter, shown.board());

    SDL_UpdateTexture(texture.get(), nullptr, canvas.data(), w * 3);
    SDL_RenderClear(renderer.get());
    SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);
    SDL_RenderPresent(renderer.get());

    if (!outcomeAnnounced_ &&
        gameState_.outcome() != GameState::Outcome::Ongoing) {
      std::cout << "[game over] " << gameState_.outcomeReason() << "\n";
      sounds.gameEnd();
      outcomeAnnounced_ = true;
    }

    Uint32 const elapsed = SDL_GetTicks() - frameStart;
    if (elapsed < frameMs) SDL_Delay(frameMs - elapsed);
  }

  // The engine worker holds a pointer to engine_; let it finish before teardown.
  if (engineFuture_.valid()) engineFuture_.wait();
}

}  // namespace chess

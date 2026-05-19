#include "chess/Engine.h"

#include <stdexcept>

namespace chess {

RandomEngine::RandomEngine(std::uint64_t seed) : rng_(seed) {}

Move RandomEngine::bestMove(GameState const& state, int /*timeLimitMs*/) {
  auto moves = state.legalMoves();
  if (moves.empty()) {
    throw std::runtime_error("RandomEngine: no legal moves available");
  }
  std::uniform_int_distribution<std::size_t> dist(0, moves.size() - 1);
  return moves[dist(rng_)];
}

}  // namespace chess

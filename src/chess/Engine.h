#pragma once

#include "chess/GameState.h"
#include "chess/Types.h"

#include <cstdint>
#include <random>

namespace chess {

class Engine {
public:
  virtual ~Engine() = default;

  // Picks a move for `state`'s side-to-move. `timeLimitMs` is a hint; engines
  // may interpret it strictly (StockfishEngine) or ignore it (RandomEngine).
  virtual Move bestMove(GameState const& state, int timeLimitMs) = 0;
};

class RandomEngine : public Engine {
public:
  explicit RandomEngine(std::uint64_t seed);
  Move bestMove(GameState const& state, int timeLimitMs) override;

private:
  std::mt19937_64 rng_;
};

}  // namespace chess

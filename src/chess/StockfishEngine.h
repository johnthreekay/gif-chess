#pragma once

#include "chess/Engine.h"
#include "mosaic/Subprocess.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace chess {

// Spawns `stockfish` (binary on PATH unless overridden) and talks UCI on its
// stdin/stdout. Nerf via Skill Level [0..20] and/or UCI_Elo [1320..3190].
class StockfishEngine : public Engine {
public:
  // skillLevel: 0..20 (Stockfish "Skill Level" option). -1 = don't set.
  // uciElo:     1320..3190 (enables UCI_LimitStrength). 0 = don't set.
  // binaryPath: defaults to "stockfish" (searched on PATH).
  explicit StockfishEngine(int skillLevel = -1,
                           int uciElo = 0,
                           std::string binaryPath = "stockfish");
  ~StockfishEngine() override;

  StockfishEngine(StockfishEngine const&) = delete;
  StockfishEngine& operator=(StockfishEngine const&) = delete;

  Move bestMove(GameState const& state, int timeLimitMs) override;

  // Identifying string the engine returned at handshake (e.g. "Stockfish 16").
  std::string const& name() const { return name_; }

private:
  std::unique_ptr<mosaic::Subprocess> proc_;
  std::string lineBuf_;
  std::string name_;

  void send(std::string line);
  // Reads one line of engine output (without its newline). Returns std::nullopt
  // on EOF (engine closed stdout); a blank line comes back as an empty string,
  // not nullopt. With timeoutMs >= 0, throws if no data arrives within that
  // window (so a hung/non-UCI engine can't freeze us).
  std::optional<std::string> readLine(int timeoutMs = -1);
  void waitForToken(std::string_view token, int timeoutMs = -1);
};

}  // namespace chess

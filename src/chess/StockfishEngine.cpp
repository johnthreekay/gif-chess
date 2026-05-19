#include "chess/StockfishEngine.h"

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace chess {

namespace {

bool startsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Generous ceiling for the `uci`/`isready` handshake. A real engine answers in
// milliseconds; if nothing comes back by now it's broken or not a UCI engine,
// so we bail (the caller falls back to RandomEngine instead of hanging).
constexpr int kHandshakeTimeoutMs = 8000;

}  // namespace

StockfishEngine::StockfishEngine(int skillLevel, int uciElo, std::string binaryPath) {
  proc_ = std::make_unique<mosaic::Subprocess>(
      std::vector<std::string>{std::move(binaryPath)},
      mosaic::Subprocess::Options{.captureStdout = true, .captureStdin = true});

  send("uci");
  std::string transcript;  // what the engine actually said, for a useful error
  while (true) {
    auto line = readLine(kHandshakeTimeoutMs);
    if (!line) {  // engine closed stdout before finishing the handshake
      std::string const how = proc_->exitDescription(/*graceMs=*/300);
      throw std::runtime_error(
          "StockfishEngine: engine closed before 'uciok' (" + how + ")" +
          (transcript.empty()
               ? std::string("; it produced no output. A broken/incompatible build "
                             "or a missing NNUE network file does this - try running "
                             "`stockfish` directly.")
               : ". It said:\n" + transcript));
    }
    transcript += "  " + *line + "\n";
    if (startsWith(*line, "id name ")) name_ = line->substr(8);
    if (*line == "uciok") break;
    // Blank lines and other chatter (e.g. Stockfish's "option name ..." dump)
    // just fall through and we keep reading.
  }

  if (skillLevel >= 0) {
    if (skillLevel > 20) skillLevel = 20;
    send("setoption name Skill Level value " + std::to_string(skillLevel));
  }
  if (uciElo > 0) {
    if (uciElo < 1320) uciElo = 1320;
    if (uciElo > 3190) uciElo = 3190;
    send("setoption name UCI_LimitStrength value true");
    send("setoption name UCI_Elo value " + std::to_string(uciElo));
  }

  send("isready");
  waitForToken("readyok", kHandshakeTimeoutMs);
}

StockfishEngine::~StockfishEngine() {
  if (!proc_) return;
  // Ask it to quit, then close stdin (EOF). ~Subprocess reaps it (and
  // force-kills it if it doesn't exit), so this can't hang. Calls here may
  // fail if the engine already died; that's fine.
  try { send("quit"); }       catch (...) {}
  try { proc_->closeStdin(); } catch (...) {}
}

void StockfishEngine::send(std::string line) {
  line += '\n';
  proc_->write(reinterpret_cast<std::uint8_t const*>(line.data()), line.size());
}

std::optional<std::string> StockfishEngine::readLine(int timeoutMs) {
  while (true) {
    if (auto nl = lineBuf_.find('\n'); nl != std::string::npos) {
      std::string line = lineBuf_.substr(0, nl);
      lineBuf_.erase(0, nl + 1);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      return line;  // may be ""; a blank line is still a line, not EOF
    }
    if (timeoutMs >= 0 && proc_->pollReadable(timeoutMs) == 0) {
      throw std::runtime_error("StockfishEngine: timed out waiting for engine output");
    }
    std::uint8_t tmp[4096];
    // readSome(), not read(): UCI lines are short and the engine goes idle
    // between commands, so a fill-the-buffer read would deadlock.
    std::size_t got = proc_->readSome(tmp, sizeof(tmp));
    if (got == 0) {  // EOF: the engine closed its stdout
      if (lineBuf_.empty()) return std::nullopt;
      std::string rest = std::move(lineBuf_);
      lineBuf_.clear();
      return rest;  // a final unterminated line, then nullopt next call
    }
    lineBuf_.append(reinterpret_cast<char const*>(tmp), got);
  }
}

void StockfishEngine::waitForToken(std::string_view token, int timeoutMs) {
  while (true) {
    auto line = readLine(timeoutMs);
    if (!line) {
      throw std::runtime_error(
          "StockfishEngine: engine closed before '" + std::string(token) + "'");
    }
    if (*line == token) return;
  }
}

Move StockfishEngine::bestMove(GameState const& state, int timeLimitMs) {
  send("position fen " + state.toFen());
  send("go movetime " + std::to_string(timeLimitMs));

  // `go movetime N` answers in ~N ms; allow generous slack, then give up
  // rather than freezing the UI on a misbehaving engine.
  int const replyTimeoutMs = (timeLimitMs > 0 ? timeLimitMs : 0) + 5000;
  while (true) {
    auto line = readLine(replyTimeoutMs);
    if (!line) {
      throw std::runtime_error("StockfishEngine: engine closed before 'bestmove'");
    }
    if (!startsWith(*line, "bestmove ")) continue;
    std::string_view rest = std::string_view(*line).substr(9);
    auto space = rest.find(' ');
    std::string_view uci = (space == std::string_view::npos) ? rest : rest.substr(0, space);
    if (uci == "(none)" || uci == "0000") {
      throw std::runtime_error("StockfishEngine: engine reports no legal move");
    }
    auto parsed = Move::fromUci(uci);
    if (!parsed) {
      throw std::runtime_error("StockfishEngine: bad UCI in bestmove: " + std::string(uci));
    }
    return *parsed;
  }
}

}  // namespace chess

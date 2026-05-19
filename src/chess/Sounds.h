#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chess {

// Plays short WAV cues over SDL's audio queue (chess.com-style move / capture /
// check / castle / promote / game-start / game-end), loaded from
// `<assetsDir>/{move,capture,check,castle,promote,game-start,game-end}.wav`.
//
// Strictly best-effort: never throws, never fails the game. On any failure
// (no audio, device won't open, WAVs missing, GIF_CHESS_MUTE set) it logs
// once and play() is a no-op; a missing single WAV is silent, others still
// work. Owns the SDL_INIT_AUDIO subsystem and output device, so construct it
// after an SDL video init and destroy it before SDL_Quit() (i.e. as a local
// living alongside the renderer's mosaic::SDLInit).
class Sounds {
public:
  explicit Sounds(std::string assetsDir = "assets/sounds");
  ~Sounds();

  Sounds(Sounds const&) = delete;
  Sounds& operator=(Sounds const&) = delete;

  void move();       // quiet / normal move
  void capture();    // a capture, including en passant
  void check();      // a move that gives check
  void castle();     // castling (either side)
  void promote();    // a pawn promotion
  void gameStart();  // a game begins
  void gameEnd();    // checkmate, stalemate, or draw

  bool enabled() const { return dev_ != 0; }

private:
  enum Cue { kMove, kCapture, kCheck, kCastle, kPromote, kGameStart, kGameEnd, kCueCount };

  // Loads `path` (must already be in the open device's PCM format). Returns
  // false on any failure; the cue then just stays silent.
  bool loadCue(Cue c, std::string const& path);
  void play(Cue c);

  SDL_AudioDeviceID dev_ = 0;
  SDL_AudioSpec spec_{};  // the device's PCM format (what pcm_ is stored in)
  std::array<std::vector<std::uint8_t>, kCueCount> pcm_;
};

}  // namespace chess

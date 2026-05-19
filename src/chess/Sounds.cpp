#include "chess/Sounds.h"

#include <cstdlib>
#include <iostream>

namespace chess {

namespace {

constexpr int             kFreq     = 44100;
constexpr SDL_AudioFormat kFormat   = AUDIO_S16SYS;
constexpr Uint8           kChannels = 2;

bool muteRequested() {
  char const* m = std::getenv("GIF_CHESS_MUTE");
  return m != nullptr && m[0] != '\0' && m[0] != '0';
}

}  // namespace

Sounds::Sounds(std::string assetsDir) {
  // Best-effort: never fails the game; logs once then play() no-ops if audio
  // is unavailable. GIF_CHESS_MUTE=1 (or SDL_AUDIODRIVER=dummy) skips it.
  if (muteRequested()) {
    std::cerr << "[sound] GIF_CHESS_MUTE set - running silent\n";
    return;
  }

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    std::cerr << "[sound] audio subsystem unavailable (" << SDL_GetError()
              << ") - running silent\n";
    return;
  }

  // If the backend reports zero playback devices, don't even try to open one
  // (some drivers otherwise stall on a nonexistent default device). A return
  // of -1 means "couldn't enumerate"; in that case we still try the default.
  if (SDL_GetNumAudioDevices(/*iscapture=*/0) == 0) {
    std::cerr << "[sound] no audio output devices - running silent\n";
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return;
  }

  SDL_AudioSpec want{};
  want.freq     = kFreq;
  want.format   = kFormat;
  want.channels = kChannels;
  want.samples  = 2048;
  want.callback = nullptr;  // we push PCM with SDL_QueueAudio
  dev_ = SDL_OpenAudioDevice(nullptr, /*iscapture=*/0, &want, &spec_,
                             /*allowed_changes=*/0);  // SDL converts internally
  if (dev_ == 0) {
    std::cerr << "[sound] couldn't open audio output (" << SDL_GetError()
              << ") - running silent\n";
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return;
  }

  bool any = false;
  any |= loadCue(kMove,      assetsDir + "/move.wav");
  any |= loadCue(kCapture,   assetsDir + "/capture.wav");
  any |= loadCue(kCheck,     assetsDir + "/check.wav");
  any |= loadCue(kCastle,    assetsDir + "/castle.wav");
  any |= loadCue(kPromote,   assetsDir + "/promote.wav");
  any |= loadCue(kGameStart, assetsDir + "/game-start.wav");
  any |= loadCue(kGameEnd,   assetsDir + "/game-end.wav");
  if (!any) {
    std::cerr << "[sound] no cues loaded from " << assetsDir << "/ - running silent\n";
    SDL_CloseAudioDevice(dev_);
    dev_ = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return;
  }

  SDL_PauseAudioDevice(dev_, 0);  // unpause; the queue is empty until we play
}

Sounds::~Sounds() {
  if (dev_ != 0) {
    SDL_CloseAudioDevice(dev_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
}

bool Sounds::loadCue(Cue c, std::string const& path) {
  SDL_AudioSpec wav{};
  Uint8* buf = nullptr;
  Uint32 len = 0;
  if (SDL_LoadWAV(path.c_str(), &wav, &buf, &len) == nullptr) {
    std::cerr << "[sound] couldn't load " << path << " (" << SDL_GetError() << ")\n";
    return false;
  }
  // The bundled WAVs match the device format we requested; if a hand-swapped
  // file doesn't, skip it rather than pull in a resampler.
  if (wav.freq != spec_.freq || wav.format != spec_.format ||
      wav.channels != spec_.channels) {
    std::cerr << "[sound] " << path
              << " isn't 44.1kHz/16-bit/stereo PCM WAV - skipping\n";
    SDL_FreeWAV(buf);
    return false;
  }
  pcm_[c].assign(buf, buf + len);
  SDL_FreeWAV(buf);
  return true;
}

void Sounds::play(Cue c) {
  if (dev_ == 0 || pcm_[c].empty()) return;
  // Guard against a runaway backlog (e.g. frantic clicking): if more than ~1s
  // is already queued, drop it and start fresh.
  Uint32 const bytesPerSec = static_cast<Uint32>(spec_.freq) * spec_.channels *
                             (SDL_AUDIO_BITSIZE(spec_.format) / 8);
  if (SDL_GetQueuedAudioSize(dev_) > bytesPerSec) SDL_ClearQueuedAudio(dev_);
  SDL_QueueAudio(dev_, pcm_[c].data(), static_cast<Uint32>(pcm_[c].size()));
}

void Sounds::move()      { play(kMove); }
void Sounds::capture()   { play(kCapture); }
void Sounds::check()     { play(kCheck); }
void Sounds::castle()    { play(kCastle); }
void Sounds::promote()   { play(kPromote); }
void Sounds::gameStart() { play(kGameStart); }
void Sounds::gameEnd()   { play(kGameEnd); }

}  // namespace chess

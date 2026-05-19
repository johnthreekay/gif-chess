#include "mosaic/SDLRenderer.h"

#include "mosaic/SDLTypes.h"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace mosaic {

namespace {

// Wall-clock that can be paused; elapsedMs() reports unpaused time in ms.
class PausableClock {
public:
  PausableClock() : start_(SDL_GetTicks()) {}

  double elapsedMs() const {
    if (paused_) return static_cast<double>(pauseAt_ - start_);
    return static_cast<double>(SDL_GetTicks() - start_);
  }

  void togglePause() {
    Uint32 const now = SDL_GetTicks();
    if (paused_) {
      start_ += (now - pauseAt_);
      paused_ = false;
    } else {
      pauseAt_ = now;
      paused_ = true;
    }
  }

  bool paused() const { return paused_; }

private:
  Uint32 start_;
  Uint32 pauseAt_ = 0;
  bool paused_ = false;
};

}  // namespace

SDLRenderer::SDLRenderer(MosaicComposer composer, int targetFps, std::string windowTitle)
    : composer_(std::move(composer)),
      targetFps_(targetFps),
      title_(std::move(windowTitle)) {
  if (targetFps_ <= 0) throw std::invalid_argument("SDLRenderer: targetFps must be positive");
}

void SDLRenderer::run() {
  SDLInit sdl;

  int const w = composer_.layout().canvasWidth();
  int const h = composer_.layout().canvasHeight();

  WindowPtr window{SDL_CreateWindow(
      title_.c_str(),
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      w, h,
      SDL_WINDOW_SHOWN)};
  if (!window) throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());

  RendererPtr renderer{SDL_CreateRenderer(
      window.get(), -1, SDL_RENDERER_ACCELERATED)};
  if (!renderer) throw std::runtime_error(std::string("SDL_CreateRenderer: ") + SDL_GetError());

  TexturePtr texture{SDL_CreateTexture(
      renderer.get(),
      SDL_PIXELFORMAT_RGB24,
      SDL_TEXTUREACCESS_STREAMING,
      w, h)};
  if (!texture) throw std::runtime_error(std::string("SDL_CreateTexture: ") + SDL_GetError());

  PausableClock clock;
  Uint32 const frameMs = 1000u / static_cast<Uint32>(targetFps_);
  bool running = true;

  std::cout << "[preview] " << w << "x" << h << " @ " << targetFps_
            << " fps - SPACE to pause, ESC to quit\n";

  while (running) {
    Uint32 const frameStart = SDL_GetTicks();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        running = false;
      } else if (ev.type == SDL_KEYDOWN) {
        switch (ev.key.keysym.sym) {
          case SDLK_ESCAPE: running = false; break;
          case SDLK_SPACE:  clock.togglePause(); break;
          default: break;
        }
      }
    }
    if (!running) break;

    Frame canvas = composer_.composite(clock.elapsedMs());
    SDL_UpdateTexture(texture.get(), nullptr, canvas.data(), w * 3);
    SDL_RenderClear(renderer.get());
    SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);
    SDL_RenderPresent(renderer.get());

    Uint32 const elapsed = SDL_GetTicks() - frameStart;
    if (elapsed < frameMs) SDL_Delay(frameMs - elapsed);
  }
}

}  // namespace mosaic

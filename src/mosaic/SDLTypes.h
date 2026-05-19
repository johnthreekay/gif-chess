#pragma once

#include <SDL.h>

#include <memory>

namespace mosaic {

struct WindowDeleter   { void operator()(SDL_Window* w)   const { if (w) SDL_DestroyWindow(w);   } };
struct RendererDeleter { void operator()(SDL_Renderer* r) const { if (r) SDL_DestroyRenderer(r); } };
struct TextureDeleter  { void operator()(SDL_Texture* t)  const { if (t) SDL_DestroyTexture(t);  } };
using WindowPtr   = std::unique_ptr<SDL_Window,   WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;
using TexturePtr  = std::unique_ptr<SDL_Texture,  TextureDeleter>;

// RAII wrapper for SDL_Init(SDL_INIT_VIDEO) / SDL_Quit. Construct once per
// renderer run() to ensure clean shutdown even on exceptions.
class SDLInit {
public:
  SDLInit();
  ~SDLInit();
  SDLInit(SDLInit const&) = delete;
  SDLInit& operator=(SDLInit const&) = delete;
};

}  // namespace mosaic

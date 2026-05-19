#include "mosaic/SDLTypes.h"

#include <stdexcept>
#include <string>

namespace mosaic {

SDLInit::SDLInit() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());
  }
}

SDLInit::~SDLInit() { SDL_Quit(); }

}  // namespace mosaic

#pragma once

#include "mosaic/MosaicComposer.h"
#include "mosaic/MosaicRenderer.h"

#include <string>

namespace mosaic {

class SDLRenderer : public MosaicRenderer {
public:
  SDLRenderer(MosaicComposer composer, int targetFps = 30,
              std::string windowTitle = "mosaic");

  void run() override;

private:
  MosaicComposer composer_;
  int targetFps_;
  std::string title_;
};

}  // namespace mosaic

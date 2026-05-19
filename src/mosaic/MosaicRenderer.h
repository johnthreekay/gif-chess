#pragma once

namespace mosaic {

// Common base for things that drive a MosaicComposer over time: the live
// SDL preview and the MP4 exporter.
class MosaicRenderer {
public:
  virtual ~MosaicRenderer() = default;

  // Blocking; runs until done (user quit, export complete, or error).
  virtual void run() = 0;
};

}  // namespace mosaic

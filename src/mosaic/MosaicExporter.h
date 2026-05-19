#pragma once

#include "mosaic/MosaicComposer.h"
#include "mosaic/MosaicRenderer.h"

#include <string>

namespace mosaic {

// Drives a MosaicComposer at a fixed framerate for a fixed duration and pipes
// the resulting raw RGB24 frames to a single `ffmpeg` subprocess that encodes
// an h264/main/yuv420p MP4 (no audio, +faststart) to disk.
class MosaicExporter : public MosaicRenderer {
public:
  MosaicExporter(MosaicComposer composer, std::string outputPath,
                 int fps, double durationSec);

  void run() override;

private:
  MosaicComposer composer_;
  std::string outputPath_;
  int fps_;
  double durationSec_;
};

}  // namespace mosaic

#include "mosaic/MosaicExporter.h"

#include "mosaic/Subprocess.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace mosaic {

MosaicExporter::MosaicExporter(MosaicComposer composer, std::string outputPath,
                               int fps, double durationSec)
    : composer_(std::move(composer)),
      outputPath_(std::move(outputPath)),
      fps_(fps),
      durationSec_(durationSec) {
  if (fps_ <= 0) {
    throw std::invalid_argument("MosaicExporter: fps must be positive");
  }
  if (durationSec_ <= 0.0) {
    throw std::invalid_argument("MosaicExporter: durationSec must be positive");
  }
  if (outputPath_.empty()) {
    throw std::invalid_argument("MosaicExporter: outputPath must not be empty");
  }
}

void MosaicExporter::run() {
  int const w = composer_.layout().canvasWidth();
  int const h = composer_.layout().canvasHeight();
  std::string const size = std::to_string(w) + "x" + std::to_string(h);
  std::string const fpsStr = std::to_string(fps_);
  std::string const durStr = std::to_string(durationSec_);

  long long const totalFrames =
      static_cast<long long>(static_cast<double>(fps_) * durationSec_);

  Subprocess proc({
      "ffmpeg", "-y", "-v", "error",
      "-f", "rawvideo",
      "-pix_fmt", "rgb24",
      "-s", size,
      "-r", fpsStr,
      "-i", "-",
      "-c:v", "libx264",
      "-profile:v", "main",
      "-pix_fmt", "yuv420p",
      "-movflags", "+faststart",
      "-an",
      "-t", durStr,
      outputPath_,
  }, {.captureStdout = false, .captureStdin = true});

  std::cout << "[export] " << size << " @ " << fps_ << " fps for "
            << durationSec_ << "s (" << totalFrames << " frames) -> "
            << outputPath_ << "\n";

  auto const wallStart = std::chrono::steady_clock::now();
  for (long long n = 0; n < totalFrames; ++n) {
    double const timeMs = static_cast<double>(n) * 1000.0 / fps_;
    Frame canvas = composer_.composite(timeMs);
    proc.write(canvas.data(), canvas.size());
    if (n > 0 && (n % fps_) == 0) {
      std::cout << "." << std::flush;
    }
  }
  std::cout << "\n";

  proc.closeStdin();
  int const rc = proc.wait();
  if (rc != 0) {
    throw std::runtime_error("MosaicExporter: ffmpeg exited " + std::to_string(rc));
  }

  auto const wallMs = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - wallStart).count();
  std::cout << "[export] done in " << wallMs / 1000.0 << "s wall time\n";
}

}  // namespace mosaic

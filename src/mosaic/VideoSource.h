#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mosaic {

// One frame's worth of packed RGB24 pixels: size == width * height * 3.
using Frame = std::vector<std::uint8_t>;

class VideoSource {
public:
  VideoSource(std::string path, int targetW, int targetH);

  // Returns the frame that should be visible at the given time, with the
  // clip looping forever (timeMs is taken modulo clip duration in frames).
  Frame const& frameAt(double timeMs) const;

  int width() const { return targetW_; }
  int height() const { return targetH_; }
  std::size_t frameCount() const { return frames_.size(); }
  double nativeFps() const { return nativeFps_; }
  double durationSec() const { return durationSec_; }
  std::string const& path() const { return path_; }

private:
  std::string path_;
  int targetW_;
  int targetH_;
  double nativeFps_ = 0.0;
  double durationSec_ = 0.0;
  std::vector<Frame> frames_;

  void probe();
  void decode();
};

}  // namespace mosaic

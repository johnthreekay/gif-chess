#include "mosaic/VideoSource.h"

#include "mosaic/Subprocess.h"

#include <cassert>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace mosaic {

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.remove_suffix(1);
  return s;
}

// Parses "10/1"-style rational or a plain decimal. Returns 0 on failure.
double parseRational(std::string_view s) {
  try {
    auto slash = s.find('/');
    if (slash == std::string_view::npos) return std::stod(std::string(s));
    double num = std::stod(std::string(s.substr(0, slash)));
    double den = std::stod(std::string(s.substr(slash + 1)));
    return den == 0.0 ? 0.0 : num / den;
  } catch (...) {
    return 0.0;
  }
}

double parseDouble(std::string_view s) {
  try { return std::stod(std::string(s)); } catch (...) { return 0.0; }
}

}  // namespace

VideoSource::VideoSource(std::string path, int targetW, int targetH)
    : path_(std::move(path)), targetW_(targetW), targetH_(targetH) {
  if (targetW_ <= 0 || targetH_ <= 0) {
    throw std::invalid_argument("VideoSource: target dimensions must be positive");
  }
  probe();
  decode();
}

void VideoSource::probe() {
  std::string out = Subprocess::runCapture({
      "ffprobe", "-v", "error",
      "-select_streams", "v:0",
      "-show_entries", "stream=width,height,r_frame_rate,nb_frames,duration",
      "-of", "default=noprint_wrappers=1",
      path_,
  });

  std::istringstream iss(out);
  std::string line;
  while (std::getline(iss, line)) {
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string_view key = trim(std::string_view(line).substr(0, eq));
    std::string_view val = trim(std::string_view(line).substr(eq + 1));
    if      (key == "r_frame_rate") nativeFps_   = parseRational(val);
    else if (key == "duration")     durationSec_ = parseDouble(val);
  }

  if (nativeFps_ <= 0.0) {
    throw std::runtime_error("VideoSource: could not determine framerate for " + path_);
  }
}

void VideoSource::decode() {
  std::string scale = std::to_string(targetW_) + ":" + std::to_string(targetH_);
  Subprocess proc({
      "ffmpeg", "-v", "error",
      "-i", path_,
      "-vf", "scale=" + scale,
      "-pix_fmt", "rgb24",
      "-f", "rawvideo",
      "-",
  });

  std::size_t const frameBytes =
      static_cast<std::size_t>(targetW_) * static_cast<std::size_t>(targetH_) * 3u;
  Frame buf(frameBytes);

  while (true) {
    std::size_t got = proc.read(buf.data(), frameBytes);
    if (got == 0) break;
    if (got != frameBytes) {
      throw std::runtime_error(
          "VideoSource: short read from ffmpeg (got " + std::to_string(got) +
          " of " + std::to_string(frameBytes) + ")");
    }
    frames_.push_back(buf);
  }

  int rc = proc.wait();
  if (rc != 0) {
    throw std::runtime_error("VideoSource: ffmpeg decode failed (exit " + std::to_string(rc) + ")");
  }
  if (frames_.empty()) {
    throw std::runtime_error("VideoSource: decoded 0 frames from " + path_);
  }

  if (durationSec_ <= 0.0) {
    durationSec_ = static_cast<double>(frames_.size()) / nativeFps_;
  }
}

Frame const& VideoSource::frameAt(double timeMs) const {
  assert(!frames_.empty());
  double t = timeMs / 1000.0;
  long long idx = static_cast<long long>(std::floor(t * nativeFps_));
  long long n = static_cast<long long>(frames_.size());
  long long wrapped = ((idx % n) + n) % n;
  return frames_[static_cast<std::size_t>(wrapped)];
}

}  // namespace mosaic

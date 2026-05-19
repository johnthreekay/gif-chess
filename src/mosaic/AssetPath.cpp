#include "mosaic/AssetPath.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace mosaic {

namespace {

namespace fs = std::filesystem;

// Directory containing the running executable, via /proc/self/exe (Linux-only
// project). Empty path if it can't be resolved.
fs::path exeDir() {
  std::error_code ec;
  fs::path p = fs::read_symlink("/proc/self/exe", ec);
  if (ec) return {};
  return p.parent_path();
}

}  // namespace

std::string assetRoot() {
  if (char const* env = std::getenv("GIF_CHESS_ASSETS"); env && env[0] != '\0') {
    return env;
  }

  std::error_code ec;
  if (fs::path d = exeDir(); !d.empty()) {
    for (fs::path const& cand : {d / "assets", d / ".." / "assets"}) {
      if (fs::is_directory(cand, ec)) return cand.lexically_normal().string();
    }
  }

  return "assets";
}

}  // namespace mosaic

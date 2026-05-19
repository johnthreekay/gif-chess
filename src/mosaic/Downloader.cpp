#include "mosaic/Downloader.h"

#include "mosaic/Subprocess.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace mosaic {

namespace {

constexpr char const* kUserAgent = "gif-chess/0.1";

void mkdirP(std::string const& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) throw std::runtime_error("mkdir '" + path + "': " + ec.message());
}

}  // namespace

Downloader::Downloader(std::string cacheDir) : cacheDir_(std::move(cacheDir)) {}

std::string Downloader::download(std::string_view url, std::string_view filename) {
  mkdirP(cacheDir_);
  std::string outPath = cacheDir_ + "/" + std::string(filename);
  Subprocess proc({
      "curl", "-sL", "--fail",
      "--max-time", "60",
      "-A", kUserAgent,
      "-o", outPath,
      std::string(url),
  }, Subprocess::Options{.captureStdout = false, .captureStdin = false});
  int rc = proc.wait();
  if (rc != 0) {
    throw std::runtime_error("Downloader: curl exited " + std::to_string(rc) +
                              " for " + std::string(url));
  }
  std::error_code ec;
  if (!std::filesystem::exists(outPath, ec) ||
      std::filesystem::file_size(outPath, ec) == 0) {
    throw std::runtime_error("Downloader: empty file from " + std::string(url));
  }
  return outPath;
}

std::string Downloader::fetch(std::string_view url) {
  std::string out = Subprocess::runCapture({
      "curl", "-sL", "--fail",
      "--max-time", "30",
      "-A", kUserAgent,
      std::string(url),
  });
  return out;
}

std::string urlEncode(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    bool unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                      c == '.' || c == '~';
    if (unreserved) {
      out += c;
    } else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
      out += buf;
    }
  }
  return out;
}

}  // namespace mosaic

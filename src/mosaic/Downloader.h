#pragma once

#include <string>
#include <string_view>

namespace mosaic {

// Curl-backed downloader and small HTTP helpers.
class Downloader {
public:
  explicit Downloader(std::string cacheDir = "cache");

  // Downloads `url` to `cacheDir/filename`. Creates the cache dir if needed.
  // Returns the full output path. Throws on non-zero curl exit or empty file.
  std::string download(std::string_view url, std::string_view filename);

  // Fetches `url` and returns the body as a string (text content, e.g. JSON).
  // Throws on non-zero curl exit.
  std::string fetch(std::string_view url);

  std::string const& cacheDir() const { return cacheDir_; }

private:
  std::string cacheDir_;
};

// Percent-encodes characters that aren't unreserved per RFC 3986.
std::string urlEncode(std::string_view s);

}  // namespace mosaic

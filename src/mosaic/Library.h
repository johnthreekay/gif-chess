#pragma once

#include "mosaic/VideoSource.h"

#include <cstddef>
#include <string>
#include <vector>

namespace mosaic {

struct LibraryItem {
  std::string label;  // short human-readable name (filename basename, search title, ...)
  std::string path;   // local file path on disk
};

// A curated list of mosaic source files. Items can be added in any order;
// load() materializes them as VideoSources at the requested cell size.
class Library {
public:
  void add(std::string label, std::string path);
  void remove(std::size_t index);
  void clear();

  std::vector<LibraryItem> const& items() const { return items_; }
  std::size_t size() const { return items_.size(); }
  bool empty() const { return items_.empty(); }

  // Decodes each item at (cellSize, cellSize). Skips items that fail to load,
  // returning what succeeded plus a list of failure messages.
  struct LoadResult {
    std::vector<VideoSource> sources;
    std::vector<std::string> failures;
  };
  LoadResult load(int cellSize) const;

private:
  std::vector<LibraryItem> items_;
};

}  // namespace mosaic

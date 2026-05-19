#include "mosaic/Library.h"

#include <stdexcept>
#include <utility>

namespace mosaic {

void Library::add(std::string label, std::string path) {
  items_.push_back({std::move(label), std::move(path)});
}

void Library::remove(std::size_t index) {
  if (index >= items_.size()) {
    throw std::out_of_range("Library::remove: index " + std::to_string(index) +
                             " out of range (" + std::to_string(items_.size()) + ")");
  }
  items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
}

void Library::clear() {
  items_.clear();
}

Library::LoadResult Library::load(int cellSize) const {
  LoadResult result;
  result.sources.reserve(items_.size());
  for (auto const& item : items_) {
    try {
      result.sources.emplace_back(item.path, cellSize, cellSize);
    } catch (std::exception const& e) {
      result.failures.push_back(item.label + " (" + item.path + "): " + e.what());
    }
  }
  return result;
}

}  // namespace mosaic

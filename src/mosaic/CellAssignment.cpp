#include "mosaic/CellAssignment.h"

#include "mosaic/VideoSource.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>

namespace mosaic {

namespace {

void checkBounds(int rows, int cols, int row, int col) {
  if (row < 0 || row >= rows || col < 0 || col >= cols) {
    throw std::out_of_range(
        "CellAssignment: cell (" + std::to_string(row) + ", " +
        std::to_string(col) + ") out of range for " +
        std::to_string(rows) + "x" + std::to_string(cols) + " grid");
  }
}

std::size_t flatIdx(int row, int col, int cols) {
  return static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
         static_cast<std::size_t>(col);
}

}  // namespace

CellAssignment::CellAssignment(int rows, int cols, std::vector<VideoSource const*> sources)
    : rows_(rows), cols_(cols), sources_(std::move(sources)) {
  if (rows_ <= 0 || cols_ <= 0) {
    throw std::invalid_argument("CellAssignment: rows/cols must be positive");
  }
}

VideoSource const* CellAssignment::sourceFor(int row, int col) const {
  int idx = sourceIndexFor(row, col);
  if (idx < 0) return nullptr;
  auto u = static_cast<std::size_t>(idx);
  return u < sources_.size() ? sources_[u] : nullptr;
}

// ---------- RepeatTile ----------

RepeatTile::RepeatTile(int rows, int cols, std::vector<VideoSource const*> sources)
    : CellAssignment(rows, cols, std::move(sources)) {}

int RepeatTile::sourceIndexFor(int row, int col) const {
  checkBounds(rows_, cols_, row, col);
  if (sources_.empty()) return -1;
  return static_cast<int>(flatIdx(row, col, cols_) % sources_.size());
}

// ---------- RandomFill ----------

RandomFill::RandomFill(int rows, int cols, std::vector<VideoSource const*> sources,
                       std::uint64_t seed, bool allowRepeats)
    : CellAssignment(rows, cols, std::move(sources)) {
  std::size_t const total = static_cast<std::size_t>(rows_) * cols_;
  std::size_t const n = sources_.size();
  placement_.assign(total, -1);
  if (n == 0) return;

  std::mt19937_64 rng(seed);
  if (allowRepeats) {
    std::uniform_int_distribution<std::size_t> dist(0, n - 1);
    for (std::size_t i = 0; i < total; ++i) {
      placement_[i] = static_cast<int>(dist(rng));
    }
  } else {
    if (total > n) {
      throw std::invalid_argument(
          "RandomFill: cannot fill " + std::to_string(total) +
          " cells without repeats; only " + std::to_string(n) +
          " sources available");
    }
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);
    for (std::size_t i = 0; i < total; ++i) {
      placement_[i] = indices[i];
    }
  }
}

int RandomFill::sourceIndexFor(int row, int col) const {
  checkBounds(rows_, cols_, row, col);
  return placement_[flatIdx(row, col, cols_)];
}

// ---------- ManualPlacement ----------

ManualPlacement::ManualPlacement(int rows, int cols, std::vector<VideoSource const*> sources,
                                 std::vector<int> placement)
    : CellAssignment(rows, cols, std::move(sources)),
      placement_(std::move(placement)) {
  std::size_t const total = static_cast<std::size_t>(rows_) * cols_;
  if (placement_.size() != total) {
    throw std::invalid_argument(
        "ManualPlacement: placement size " + std::to_string(placement_.size()) +
        " does not match grid size " + std::to_string(total));
  }
  int const n = static_cast<int>(sources_.size());
  for (int idx : placement_) {
    if (idx < -1 || idx >= n) {
      throw std::invalid_argument(
          "ManualPlacement: index " + std::to_string(idx) +
          " out of range [-1, " + std::to_string(n - 1) + "]");
    }
  }
}

int ManualPlacement::sourceIndexFor(int row, int col) const {
  checkBounds(rows_, cols_, row, col);
  return placement_[flatIdx(row, col, cols_)];
}

}  // namespace mosaic

#include "mosaic/GridLayout.h"

#include <stdexcept>
#include <string>

namespace mosaic {

GridLayout::GridLayout(int rows, int cols, int cellSize, int margin, LabelStyle style)
    : rows_(rows), cols_(cols), cellSize_(cellSize), margin_(margin), style_(style) {
  if (rows_ <= 0 || cols_ <= 0) {
    throw std::invalid_argument("GridLayout: rows/cols must be positive");
  }
  if (cellSize_ <= 0) {
    throw std::invalid_argument("GridLayout: cellSize must be positive");
  }
  if (margin_ < 0) {
    throw std::invalid_argument("GridLayout: margin must be non-negative");
  }
  if (rows_ > 26) {
    throw std::invalid_argument("GridLayout: row labels only support up to 26 rows (A..Z)");
  }
}

int GridLayout::topMargin() const {
  return style_ == LabelStyle::Margin ? margin_ : 0;
}

int GridLayout::leftMargin() const {
  return style_ == LabelStyle::Margin ? margin_ : 0;
}

int GridLayout::canvasWidth() const {
  return leftMargin() + cols_ * cellSize_;
}

int GridLayout::canvasHeight() const {
  return topMargin() + rows_ * cellSize_;
}

Rect GridLayout::cellRect(int row, int col) const {
  return Rect{
      leftMargin() + col * cellSize_,
      topMargin() + row * cellSize_,
      cellSize_,
      cellSize_,
  };
}

std::vector<LabelPos> GridLayout::labelPositions() const {
  std::vector<LabelPos> out;
  if (style_ == LabelStyle::None) return out;

  int const topY = topMargin() / 2;
  for (int c = 0; c < cols_; ++c) {
    int cx = leftMargin() + c * cellSize_ + cellSize_ / 2;
    out.push_back({std::to_string(c + 1), cx, topY});
  }

  int const leftX = leftMargin() / 2;
  for (int r = 0; r < rows_; ++r) {
    int cy = topMargin() + r * cellSize_ + cellSize_ / 2;
    std::string letter(1, static_cast<char>('A' + r));
    out.push_back({std::move(letter), leftX, cy});
  }

  return out;
}

}  // namespace mosaic

#pragma once

#include "mosaic/Geometry.h"

#include <string>
#include <vector>

namespace mosaic {

enum class LabelStyle {
  None,
  Margin,
};

struct LabelPos {
  std::string text;
  int centerX;
  int centerY;
};

class GridLayout {
public:
  GridLayout(int rows, int cols, int cellSize, int margin, LabelStyle style);

  int rows() const { return rows_; }
  int cols() const { return cols_; }
  int cellSize() const { return cellSize_; }
  int margin() const { return margin_; }
  LabelStyle style() const { return style_; }

  int canvasWidth() const;
  int canvasHeight() const;
  Rect cellRect(int row, int col) const;

  // Returns label anchors as center points. Empty if style == None.
  std::vector<LabelPos> labelPositions() const;

private:
  int rows_;
  int cols_;
  int cellSize_;
  int margin_;
  LabelStyle style_;

  int topMargin() const;
  int leftMargin() const;
};

}  // namespace mosaic

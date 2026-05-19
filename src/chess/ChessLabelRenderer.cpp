#include "chess/ChessLabelRenderer.h"

#include "mosaic/Font.h"
#include "mosaic/GridLayout.h"

#include <string>

namespace chess {

ChessLabelRenderer::ChessLabelRenderer(mosaic::RGB color, int scale,
                                       mosaic::BitmapFont const* font)
    : color_(color),
      scale_(scale),
      font_(font ? font : &mosaic::defaultFont()) {}

void ChessLabelRenderer::drawLabels(mosaic::Painter& painter,
                                    mosaic::GridLayout const& layout) const {
  if (layout.margin() <= 0) return;
  int const margin   = layout.margin();
  int const cellSize = layout.cellSize();
  int const cols     = layout.cols();
  int const rows     = layout.rows();

  int const topY = margin / 2;
  for (int c = 0; c < cols; ++c) {
    int const cx = margin + c * cellSize + cellSize / 2;
    std::string letter(1, static_cast<char>('a' + c));
    painter.drawText(*font_, letter, cx, topY, color_, scale_);
  }

  int const leftX = margin / 2;
  for (int r = 0; r < rows; ++r) {
    int const rank = rows - 1 - r;  // row 0 = top = display "8"
    int const cy = margin + r * cellSize + cellSize / 2;
    painter.drawText(*font_, std::to_string(rank + 1), leftX, cy, color_, scale_);
  }
}

}  // namespace chess

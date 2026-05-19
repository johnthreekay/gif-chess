#include "mosaic/LabelRenderer.h"

#include "mosaic/Font.h"
#include "mosaic/GridLayout.h"

namespace mosaic {

MarginLabelRenderer::MarginLabelRenderer(RGB color, int scale, BitmapFont const* font)
    : color_(color), scale_(scale), font_(font ? font : &defaultFont()) {}

void MarginLabelRenderer::drawLabels(Painter& painter, GridLayout const& layout) const {
  for (auto const& pos : layout.labelPositions()) {
    painter.drawText(*font_, pos.text, pos.centerX, pos.centerY, color_, scale_);
  }
}

}  // namespace mosaic

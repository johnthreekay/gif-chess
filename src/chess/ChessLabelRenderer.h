#pragma once

#include "mosaic/LabelRenderer.h"
#include "mosaic/Painter.h"

namespace mosaic {
class BitmapFont;
class GridLayout;
}  // namespace mosaic

namespace chess {

// Renders chess-style labels in the layout's margin: files a-h centered on top
// of each column, ranks 1-8 centered on the left of each row, with rank 1 at
// the bottom (so it lines up with the white side).
class ChessLabelRenderer : public mosaic::LabelRenderer {
public:
  explicit ChessLabelRenderer(mosaic::RGB color = mosaic::RGB{0, 0, 0},
                              int scale = 2,
                              mosaic::BitmapFont const* font = nullptr);

  void drawLabels(mosaic::Painter& painter, mosaic::GridLayout const& layout) const override;

private:
  mosaic::RGB color_;
  int scale_;
  mosaic::BitmapFont const* font_;
};

}  // namespace chess

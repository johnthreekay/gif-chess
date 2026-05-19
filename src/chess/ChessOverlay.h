#pragma once

#include "chess/Board.h"
#include "chess/PieceSprites.h"
#include "mosaic/GridLayout.h"
#include "mosaic/Painter.h"

namespace chess {

// Paints chess pieces over an already-composed canvas. The layout must be 8x8
// and the sprites must have been loaded at the same cellSize.
class ChessOverlay {
public:
  ChessOverlay(mosaic::GridLayout const& layout, PieceSprites sprites);

  // Alpha-blends each piece on `board` onto `painter`'s canvas. White (rank 1)
  // sits at the bottom of the displayed board (GridLayout row 7).
  void draw(mosaic::Painter& painter, Board const& board) const;

  mosaic::GridLayout const& layout() const { return layout_; }

private:
  mosaic::GridLayout layout_;
  PieceSprites sprites_;
};

}  // namespace chess

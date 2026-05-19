#include "chess/ChessOverlay.h"

#include "chess/Piece.h"

#include <stdexcept>
#include <utility>

namespace chess {

ChessOverlay::ChessOverlay(mosaic::GridLayout const& layout, PieceSprites sprites)
    : layout_(layout), sprites_(std::move(sprites)) {
  if (layout_.rows() != 8 || layout_.cols() != 8) {
    throw std::invalid_argument("ChessOverlay: layout must be 8x8");
  }
  if (sprites_.width() != layout_.cellSize() ||
      sprites_.height() != layout_.cellSize()) {
    throw std::invalid_argument(
        "ChessOverlay: sprite size " + std::to_string(sprites_.width()) +
        " does not match cellSize " + std::to_string(layout_.cellSize()));
  }
}

void ChessOverlay::draw(mosaic::Painter& painter, Board const& board) const {
  int const cell = layout_.cellSize();
  for (int rank = 0; rank < 8; ++rank) {
    for (int file = 0; file < 8; ++file) {
      Piece const* p = board.at(Position{file, rank});
      if (!p) continue;
      auto const& rgba = sprites_.spriteFor(p->color(), p->type());
      mosaic::Rect cellRect = layout_.cellRect(7 - rank, file);
      painter.blitRgba(cellRect, rgba.data(), cell, cell);
    }
  }
}

}  // namespace chess

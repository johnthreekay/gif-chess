#include "chess/Board.h"

#include <stdexcept>

namespace chess {

Piece const* Board::at(Position p) const {
  if (!p.valid()) throw std::out_of_range("Board::at: invalid position");
  return squares_[p.index()];
}

Piece const* Board::atIndex(int i) const {
  if (i < 0 || i >= 64) throw std::out_of_range("Board::atIndex: out of range");
  return squares_[i];
}

void Board::set(Position p, Piece const* piece) {
  if (!p.valid()) throw std::out_of_range("Board::set: invalid position");
  squares_[p.index()] = piece;
}

}  // namespace chess

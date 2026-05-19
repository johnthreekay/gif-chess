#pragma once

#include "chess/Types.h"

#include <array>

namespace chess {

class Piece;

// 8x8 array of non-owning piece pointers. nullptr means empty.
// Square index is `rank * 8 + file`.
class Board {
public:
  Board() = default;

  Piece const* at(Position p) const;
  Piece const* atIndex(int i) const;
  void set(Position p, Piece const* piece);
  void clear() { squares_.fill(nullptr); }

private:
  std::array<Piece const*, 64> squares_{};
};

}  // namespace chess

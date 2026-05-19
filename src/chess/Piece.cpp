#include "chess/Piece.h"

#include "chess/Board.h"

#include <array>
#include <cctype>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace chess {

namespace {

using Dir = std::pair<int, int>;

constexpr std::array<Dir, 8> kKnightOffsets = {{
    {+1, +2}, {+2, +1}, {+2, -1}, {+1, -2},
    {-1, -2}, {-2, -1}, {-2, +1}, {-1, +2},
}};

constexpr std::array<Dir, 8> kKingOffsets = {{
    {-1, -1}, {-1, 0}, {-1, +1},
    { 0, -1},          { 0, +1},
    {+1, -1}, {+1, 0}, {+1, +1},
}};

constexpr std::array<Dir, 4> kBishopDirs = {{
    {+1, +1}, {+1, -1}, {-1, +1}, {-1, -1},
}};

constexpr std::array<Dir, 4> kRookDirs = {{
    {+1, 0}, {-1, 0}, {0, +1}, {0, -1},
}};

constexpr std::array<Dir, 8> kQueenDirs = {{
    {+1,  0}, {-1,  0}, { 0, +1}, { 0, -1},
    {+1, +1}, {+1, -1}, {-1, +1}, {-1, -1},
}};

template <typename Dirs>
void slidingMoves(Color myColor, Position from, Board const& board,
                  Dirs const& dirs, std::vector<Move>& out) {
  for (auto [df, dr] : dirs) {
    Position p{from.file + df, from.rank + dr};
    while (p.valid()) {
      Piece const* target = board.at(p);
      if (target == nullptr) {
        out.push_back(Move{from, p, PieceType::None});
      } else {
        if (target->color() != myColor) out.push_back(Move{from, p, PieceType::None});
        break;
      }
      p.file += df;
      p.rank += dr;
    }
  }
}

template <typename Dirs>
void slidingAttacks(Position from, Board const& board, Dirs const& dirs,
                    std::vector<Position>& out) {
  for (auto [df, dr] : dirs) {
    Position p{from.file + df, from.rank + dr};
    while (p.valid()) {
      out.push_back(p);
      if (board.at(p) != nullptr) break;
      p.file += df;
      p.rank += dr;
    }
  }
}

void addPawnPromotionsOrPlain(Position from, Position to, int promoRank,
                              std::vector<Move>& out) {
  if (to.rank == promoRank) {
    for (PieceType promo : {PieceType::Queen, PieceType::Rook,
                            PieceType::Bishop, PieceType::Knight}) {
      out.push_back(Move{from, to, promo});
    }
  } else {
    out.push_back(Move{from, to, PieceType::None});
  }
}

}  // namespace

void Pawn::pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const {
  int const dr = (color() == Color::White) ? +1 : -1;
  int const promoRank = (color() == Color::White) ? 7 : 0;
  int const startRank = (color() == Color::White) ? 1 : 6;

  Position one{from.file, from.rank + dr};
  if (one.valid() && board.at(one) == nullptr) {
    addPawnPromotionsOrPlain(from, one, promoRank, out);
    if (from.rank == startRank) {
      Position two{from.file, from.rank + 2 * dr};
      if (two.valid() && board.at(two) == nullptr) {
        out.push_back(Move{from, two, PieceType::None});
      }
    }
  }
  for (int df : {-1, +1}) {
    Position diag{from.file + df, from.rank + dr};
    if (!diag.valid()) continue;
    Piece const* target = board.at(diag);
    if (target != nullptr && target->color() != color()) {
      addPawnPromotionsOrPlain(from, diag, promoRank, out);
    }
  }
}

void Pawn::attackedSquares(Position from, Board const& /*board*/,
                           std::vector<Position>& out) const {
  int const dr = (color() == Color::White) ? +1 : -1;
  for (int df : {-1, +1}) {
    Position diag{from.file + df, from.rank + dr};
    if (diag.valid()) out.push_back(diag);
  }
}

void Knight::pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const {
  for (auto [df, dr] : kKnightOffsets) {
    Position p{from.file + df, from.rank + dr};
    if (!p.valid()) continue;
    Piece const* target = board.at(p);
    if (target == nullptr || target->color() != color()) {
      out.push_back(Move{from, p, PieceType::None});
    }
  }
}

void Knight::attackedSquares(Position from, Board const& /*board*/,
                             std::vector<Position>& out) const {
  for (auto [df, dr] : kKnightOffsets) {
    Position p{from.file + df, from.rank + dr};
    if (p.valid()) out.push_back(p);
  }
}

void Bishop::pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const {
  slidingMoves(color(), from, board, kBishopDirs, out);
}
void Bishop::attackedSquares(Position from, Board const& board, std::vector<Position>& out) const {
  slidingAttacks(from, board, kBishopDirs, out);
}

void Rook::pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const {
  slidingMoves(color(), from, board, kRookDirs, out);
}
void Rook::attackedSquares(Position from, Board const& board, std::vector<Position>& out) const {
  slidingAttacks(from, board, kRookDirs, out);
}

void Queen::pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const {
  slidingMoves(color(), from, board, kQueenDirs, out);
}
void Queen::attackedSquares(Position from, Board const& board, std::vector<Position>& out) const {
  slidingAttacks(from, board, kQueenDirs, out);
}

void King::pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const {
  for (auto [df, dr] : kKingOffsets) {
    Position p{from.file + df, from.rank + dr};
    if (!p.valid()) continue;
    Piece const* target = board.at(p);
    if (target == nullptr || target->color() != color()) {
      out.push_back(Move{from, p, PieceType::None});
    }
  }
}

void King::attackedSquares(Position from, Board const& /*board*/,
                           std::vector<Position>& out) const {
  for (auto [df, dr] : kKingOffsets) {
    Position p{from.file + df, from.rank + dr};
    if (p.valid()) out.push_back(p);
  }
}

Piece const& canonicalPiece(Color c, PieceType t) {
  static Pawn   wp(Color::White), bp(Color::Black);
  static Knight wn(Color::White), bn(Color::Black);
  static Bishop wb(Color::White), bb(Color::Black);
  static Rook   wr(Color::White), br(Color::Black);
  static Queen  wq(Color::White), bq(Color::Black);
  static King   wk(Color::White), bk(Color::Black);
  bool const white = (c == Color::White);
  switch (t) {
    case PieceType::Pawn:   return white ? static_cast<Piece&>(wp) : static_cast<Piece&>(bp);
    case PieceType::Knight: return white ? static_cast<Piece&>(wn) : static_cast<Piece&>(bn);
    case PieceType::Bishop: return white ? static_cast<Piece&>(wb) : static_cast<Piece&>(bb);
    case PieceType::Rook:   return white ? static_cast<Piece&>(wr) : static_cast<Piece&>(br);
    case PieceType::Queen:  return white ? static_cast<Piece&>(wq) : static_cast<Piece&>(bq);
    case PieceType::King:   return white ? static_cast<Piece&>(wk) : static_cast<Piece&>(bk);
    case PieceType::None:   break;
  }
  throw std::invalid_argument("canonicalPiece: PieceType::None");
}

Piece const* pieceFromFenLetter(char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  Color const color = std::isupper(uc) ? Color::White : Color::Black;
  PieceType type;
  switch (std::tolower(uc)) {
    case 'p': type = PieceType::Pawn;   break;
    case 'n': type = PieceType::Knight; break;
    case 'b': type = PieceType::Bishop; break;
    case 'r': type = PieceType::Rook;   break;
    case 'q': type = PieceType::Queen;  break;
    case 'k': type = PieceType::King;   break;
    default:  return nullptr;
  }
  return &canonicalPiece(color, type);
}

}  // namespace chess

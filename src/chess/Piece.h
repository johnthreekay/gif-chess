#pragma once

#include "chess/Types.h"

#include <vector>

namespace chess {

class Board;

// Abstract chess piece. Concrete subclasses (Pawn, Knight, etc.) are held as
// stateless flyweights (see canonicalPiece()) so the Board can store
// `Piece const*` per square with no allocation.
class Piece {
public:
  virtual ~Piece() = default;

  Color color() const { return color_; }
  virtual PieceType type() const = 0;

  // FEN piece letter: uppercase for White, lowercase for Black.
  virtual char fenLetter() const = 0;

  // Geometric pseudo-legal moves from `from` on `board`. Excludes en passant
  // (Pawn) and castling (King), which require GameState. Excludes legality
  // (whether the moving side's king ends up in check).
  virtual void pseudoLegalMoves(Position from, Board const& board,
                                std::vector<Move>& out) const = 0;

  // Squares this piece would attack from `from`. For pawns this is the two
  // forward diagonals (regardless of occupancy). For other pieces, all
  // reachable squares including the first piece blocking a slider's line.
  virtual void attackedSquares(Position from, Board const& board,
                               std::vector<Position>& out) const = 0;

  explicit Piece(Color c) : color_(c) {}

private:
  Color color_;
};

class Pawn : public Piece {
public:
  using Piece::Piece;
  PieceType type() const override { return PieceType::Pawn; }
  char fenLetter() const override { return color() == Color::White ? 'P' : 'p'; }
  void pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const override;
  void attackedSquares(Position from, Board const& board, std::vector<Position>& out) const override;
};

class Knight : public Piece {
public:
  using Piece::Piece;
  PieceType type() const override { return PieceType::Knight; }
  char fenLetter() const override { return color() == Color::White ? 'N' : 'n'; }
  void pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const override;
  void attackedSquares(Position from, Board const& board, std::vector<Position>& out) const override;
};

class Bishop : public Piece {
public:
  using Piece::Piece;
  PieceType type() const override { return PieceType::Bishop; }
  char fenLetter() const override { return color() == Color::White ? 'B' : 'b'; }
  void pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const override;
  void attackedSquares(Position from, Board const& board, std::vector<Position>& out) const override;
};

class Rook : public Piece {
public:
  using Piece::Piece;
  PieceType type() const override { return PieceType::Rook; }
  char fenLetter() const override { return color() == Color::White ? 'R' : 'r'; }
  void pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const override;
  void attackedSquares(Position from, Board const& board, std::vector<Position>& out) const override;
};

class Queen : public Piece {
public:
  using Piece::Piece;
  PieceType type() const override { return PieceType::Queen; }
  char fenLetter() const override { return color() == Color::White ? 'Q' : 'q'; }
  void pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const override;
  void attackedSquares(Position from, Board const& board, std::vector<Position>& out) const override;
};

class King : public Piece {
public:
  using Piece::Piece;
  PieceType type() const override { return PieceType::King; }
  char fenLetter() const override { return color() == Color::White ? 'K' : 'k'; }
  void pseudoLegalMoves(Position from, Board const& board, std::vector<Move>& out) const override;
  void attackedSquares(Position from, Board const& board, std::vector<Position>& out) const override;
};

// Returns the singleton flyweight for the given (color, type). Throws if
// type is PieceType::None.
Piece const& canonicalPiece(Color color, PieceType type);

// Maps a FEN letter ('P','n', ...) to a canonical piece pointer, or nullptr.
Piece const* pieceFromFenLetter(char c);

}  // namespace chess

#pragma once

#include "chess/Board.h"
#include "chess/Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chess {

class GameState {
public:
  // Default-constructed = standard starting position.
  GameState();

  Board const& board() const { return board_; }
  Color sideToMove() const   { return side_; }

  bool whiteCanCastleKingside()  const { return castle_.whiteK; }
  bool whiteCanCastleQueenside() const { return castle_.whiteQ; }
  bool blackCanCastleKingside()  const { return castle_.blackK; }
  bool blackCanCastleQueenside() const { return castle_.blackQ; }

  std::optional<Position> enPassantTarget() const { return ep_; }
  int halfmoveClock()  const { return halfmove_; }
  int fullmoveNumber() const { return fullmove_; }

  std::string toFen() const;
  static GameState fromFen(std::string_view fen);

  // Human-readable dump: metadata header + ASCII board.
  std::string asciiDump() const;

  // Rule predicates + move application (step 7b).

  std::vector<Move> legalMoves() const;

  bool isSquareAttacked(Position target, Color byColor) const;
  bool isCheck() const;
  bool isCheckmate() const;
  bool isStalemate() const;
  bool isFiftyMoveRule() const { return halfmove_ >= 100; }
  bool isThreefoldRepetition() const;

  enum class Outcome { Ongoing, WhiteWins, BlackWins, Draw };
  Outcome outcome() const;
  std::string outcomeReason() const;

  // Validates legality, then applies. Throws std::invalid_argument on illegal.
  void makeMove(Move const& m);

private:
  struct CastleRights {
    bool whiteK = false;
    bool whiteQ = false;
    bool blackK = false;
    bool blackQ = false;
  };

  Board board_;
  Color side_ = Color::White;
  CastleRights castle_;
  std::optional<Position> ep_;
  int halfmove_ = 0;
  int fullmove_ = 1;
  std::vector<std::string> positionHistory_;

  void applyMoveUnchecked(Move const& m);
  std::string positionKey() const;
  Position findKing(Color c) const;
};

}  // namespace chess

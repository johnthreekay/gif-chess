#include "chess/GameState.h"

#include "chess/Piece.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace chess {

namespace {

std::vector<std::string> splitWhitespace(std::string_view s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) { out.push_back(std::move(cur)); cur.clear(); }
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) out.push_back(std::move(cur));
  return out;
}

std::string castleString(GameState const& s) {
  std::string out;
  if (s.whiteCanCastleKingside())  out += 'K';
  if (s.whiteCanCastleQueenside()) out += 'Q';
  if (s.blackCanCastleKingside())  out += 'k';
  if (s.blackCanCastleQueenside()) out += 'q';
  return out.empty() ? "-" : out;
}

}  // namespace

GameState::GameState() {
  for (int file = 0; file < 8; ++file) {
    board_.set(Position{file, 1}, &canonicalPiece(Color::White, PieceType::Pawn));
    board_.set(Position{file, 6}, &canonicalPiece(Color::Black, PieceType::Pawn));
  }
  static constexpr PieceType backRank[8] = {
      PieceType::Rook,   PieceType::Knight, PieceType::Bishop, PieceType::Queen,
      PieceType::King,   PieceType::Bishop, PieceType::Knight, PieceType::Rook,
  };
  for (int file = 0; file < 8; ++file) {
    board_.set(Position{file, 0}, &canonicalPiece(Color::White, backRank[file]));
    board_.set(Position{file, 7}, &canonicalPiece(Color::Black, backRank[file]));
  }
  side_ = Color::White;
  castle_ = {true, true, true, true};
  positionHistory_.push_back(positionKey());
}

GameState GameState::fromFen(std::string_view fen) {
  auto tokens = splitWhitespace(fen);
  if (tokens.empty()) throw std::invalid_argument("fromFen: empty input");

  GameState s;
  s.board_.clear();
  s.castle_ = {};
  s.ep_ = std::nullopt;
  s.halfmove_ = 0;
  s.fullmove_ = 1;
  s.side_ = Color::White;

  std::string const& boardTok = tokens[0];
  int rank = 7;
  int file = 0;
  for (char c : boardTok) {
    if (c == '/') {
      if (file != 8) throw std::invalid_argument("fromFen: rank length != 8");
      --rank;
      file = 0;
      if (rank < 0) throw std::invalid_argument("fromFen: too many ranks");
    } else if (c >= '1' && c <= '8') {
      file += (c - '0');
      if (file > 8) throw std::invalid_argument("fromFen: rank overflow");
    } else {
      Piece const* p = pieceFromFenLetter(c);
      if (!p) throw std::invalid_argument(std::string("fromFen: bad piece '") + c + "'");
      if (file >= 8 || rank < 0) throw std::invalid_argument("fromFen: out of range");
      s.board_.set(Position{file, rank}, p);
      ++file;
    }
  }
  if (rank != 0 || file != 8) throw std::invalid_argument("fromFen: incomplete board");

  if (tokens.size() >= 2) {
    if      (tokens[1] == "w") s.side_ = Color::White;
    else if (tokens[1] == "b") s.side_ = Color::Black;
    else throw std::invalid_argument("fromFen: bad side '" + tokens[1] + "'");
  }

  if (tokens.size() >= 3 && tokens[2] != "-") {
    for (char c : tokens[2]) {
      switch (c) {
        case 'K': s.castle_.whiteK = true; break;
        case 'Q': s.castle_.whiteQ = true; break;
        case 'k': s.castle_.blackK = true; break;
        case 'q': s.castle_.blackQ = true; break;
        default: throw std::invalid_argument(std::string("fromFen: bad castle '") + c + "'");
      }
    }
  }

  if (tokens.size() >= 4 && tokens[3] != "-") {
    auto pos = Position::fromAlgebraic(tokens[3]);
    if (!pos) throw std::invalid_argument("fromFen: bad ep '" + tokens[3] + "'");
    s.ep_ = pos;
  }

  if (tokens.size() >= 5) {
    try { s.halfmove_ = std::stoi(tokens[4]); }
    catch (...) { throw std::invalid_argument("fromFen: bad halfmove '" + tokens[4] + "'"); }
  }
  if (tokens.size() >= 6) {
    try { s.fullmove_ = std::stoi(tokens[5]); }
    catch (...) { throw std::invalid_argument("fromFen: bad fullmove '" + tokens[5] + "'"); }
  }

  s.positionHistory_.push_back(s.positionKey());
  return s;
}

std::string GameState::toFen() const {
  std::string out;
  for (int rank = 7; rank >= 0; --rank) {
    int empty = 0;
    for (int file = 0; file < 8; ++file) {
      Piece const* p = board_.at(Position{file, rank});
      if (p) {
        if (empty > 0) { out += std::to_string(empty); empty = 0; }
        out += p->fenLetter();
      } else {
        ++empty;
      }
    }
    if (empty > 0) out += std::to_string(empty);
    if (rank > 0) out += '/';
  }
  out += ' ';
  out += (side_ == Color::White) ? 'w' : 'b';
  out += ' ';
  out += castleString(*this);
  out += ' ';
  out += ep_ ? ep_->toAlgebraic() : "-";
  out += ' ';
  out += std::to_string(halfmove_);
  out += ' ';
  out += std::to_string(fullmove_);
  return out;
}

std::string GameState::asciiDump() const {
  std::ostringstream out;
  out << "  side: " << (side_ == Color::White ? "white" : "black")
      << ", castle: " << castleString(*this)
      << ", ep: " << (ep_ ? ep_->toAlgebraic() : "-")
      << ", halfmove: " << halfmove_
      << ", fullmove: " << fullmove_ << "\n";
  out << "  fen: " << toFen() << "\n\n";
  out << "    a b c d e f g h\n";
  out << "  +-----------------+\n";
  for (int rank = 7; rank >= 0; --rank) {
    out << (rank + 1) << " |";
    for (int file = 0; file < 8; ++file) {
      Piece const* p = board_.at(Position{file, rank});
      out << ' ' << (p ? p->fenLetter() : '.');
    }
    out << " | " << (rank + 1) << "\n";
  }
  out << "  +-----------------+\n";
  out << "    a b c d e f g h\n";
  return out.str();
}

// Rule predicates and move application.

Position GameState::findKing(Color c) const {
  for (int i = 0; i < 64; ++i) {
    Piece const* p = board_.atIndex(i);
    if (p && p->color() == c && p->type() == PieceType::King) {
      return Position::fromIndex(i);
    }
  }
  return Position{-1, -1};
}

bool GameState::isSquareAttacked(Position target, Color byColor) const {
  std::vector<Position> attacked;
  for (int i = 0; i < 64; ++i) {
    Piece const* p = board_.atIndex(i);
    if (!p || p->color() != byColor) continue;
    attacked.clear();
    p->attackedSquares(Position::fromIndex(i), board_, attacked);
    for (auto a : attacked) {
      if (a == target) return true;
    }
  }
  return false;
}

bool GameState::isCheck() const {
  Position king = findKing(side_);
  if (!king.valid()) return false;
  return isSquareAttacked(king, opponent(side_));
}

std::vector<Move> GameState::legalMoves() const {
  std::vector<Move> candidates;
  for (int i = 0; i < 64; ++i) {
    Piece const* p = board_.atIndex(i);
    if (!p || p->color() != side_) continue;
    p->pseudoLegalMoves(Position::fromIndex(i), board_, candidates);
  }

  if (ep_) {
    Position const target = *ep_;
    int const sourceRank = (side_ == Color::White) ? 4 : 3;
    for (int df : {-1, +1}) {
      Position from{target.file + df, sourceRank};
      if (!from.valid()) continue;
      Piece const* p = board_.at(from);
      if (p && p->color() == side_ && p->type() == PieceType::Pawn) {
        candidates.push_back(Move{from, target, PieceType::None});
      }
    }
  }

  int const rank = (side_ == Color::White) ? 0 : 7;
  Position const kingPos{4, rank};
  Piece const* kingPiece = board_.at(kingPos);
  if (kingPiece && kingPiece->color() == side_ &&
      kingPiece->type() == PieceType::King && !isCheck()) {
    bool const canK = (side_ == Color::White) ? castle_.whiteK : castle_.blackK;
    bool const canQ = (side_ == Color::White) ? castle_.whiteQ : castle_.blackQ;
    Color const them = opponent(side_);
    if (canK) {
      Piece const* rook = board_.at(Position{7, rank});
      if (rook && rook->color() == side_ && rook->type() == PieceType::Rook &&
          board_.at(Position{5, rank}) == nullptr &&
          board_.at(Position{6, rank}) == nullptr &&
          !isSquareAttacked(Position{5, rank}, them) &&
          !isSquareAttacked(Position{6, rank}, them)) {
        candidates.push_back(Move{kingPos, Position{6, rank}, PieceType::None});
      }
    }
    if (canQ) {
      Piece const* rook = board_.at(Position{0, rank});
      if (rook && rook->color() == side_ && rook->type() == PieceType::Rook &&
          board_.at(Position{1, rank}) == nullptr &&
          board_.at(Position{2, rank}) == nullptr &&
          board_.at(Position{3, rank}) == nullptr &&
          !isSquareAttacked(Position{3, rank}, them) &&
          !isSquareAttacked(Position{2, rank}, them)) {
        candidates.push_back(Move{kingPos, Position{2, rank}, PieceType::None});
      }
    }
  }

  std::vector<Move> legal;
  legal.reserve(candidates.size());
  Color const us = side_;
  for (Move const& m : candidates) {
    GameState copy = *this;
    copy.applyMoveUnchecked(m);
    Position king = copy.findKing(us);
    if (king.valid() && !copy.isSquareAttacked(king, opponent(us))) {
      legal.push_back(m);
    }
  }
  return legal;
}

void GameState::applyMoveUnchecked(Move const& m) {
  Piece const* moving = board_.at(m.from);
  if (!moving) {
    throw std::invalid_argument("applyMoveUnchecked: no piece at " + m.from.toAlgebraic());
  }
  Piece const* captured = board_.at(m.to);
  Color const us = side_;

  bool const isPawnMove = (moving->type() == PieceType::Pawn);
  bool const isCastleKingside  = (moving->type() == PieceType::King) && (m.to.file - m.from.file == +2);
  bool const isCastleQueenside = (moving->type() == PieceType::King) && (m.to.file - m.from.file == -2);
  bool const isEnPassant = isPawnMove && (m.to.file != m.from.file) && (captured == nullptr);

  board_.set(m.from, nullptr);
  if (m.promotion != PieceType::None) {
    board_.set(m.to, &canonicalPiece(us, m.promotion));
  } else {
    board_.set(m.to, moving);
  }
  if (isEnPassant) {
    board_.set(Position{m.to.file, m.from.rank}, nullptr);
  }
  if (isCastleKingside) {
    Piece const* rook = board_.at(Position{7, m.from.rank});
    board_.set(Position{7, m.from.rank}, nullptr);
    board_.set(Position{5, m.from.rank}, rook);
  } else if (isCastleQueenside) {
    Piece const* rook = board_.at(Position{0, m.from.rank});
    board_.set(Position{0, m.from.rank}, nullptr);
    board_.set(Position{3, m.from.rank}, rook);
  }

  if (isPawnMove && std::abs(m.to.rank - m.from.rank) == 2) {
    int const epRank = (us == Color::White) ? 2 : 5;
    ep_ = Position{m.from.file, epRank};
  } else {
    ep_ = std::nullopt;
  }

  if (isPawnMove || captured != nullptr || isEnPassant) {
    halfmove_ = 0;
  } else {
    ++halfmove_;
  }

  if (moving->type() == PieceType::King) {
    if (us == Color::White) { castle_.whiteK = false; castle_.whiteQ = false; }
    else                    { castle_.blackK = false; castle_.blackQ = false; }
  }
  if (moving->type() == PieceType::Rook) {
    if (m.from == Position{0, 0}) castle_.whiteQ = false;
    if (m.from == Position{7, 0}) castle_.whiteK = false;
    if (m.from == Position{0, 7}) castle_.blackQ = false;
    if (m.from == Position{7, 7}) castle_.blackK = false;
  }
  if (m.to == Position{0, 0}) castle_.whiteQ = false;
  if (m.to == Position{7, 0}) castle_.whiteK = false;
  if (m.to == Position{0, 7}) castle_.blackQ = false;
  if (m.to == Position{7, 7}) castle_.blackK = false;

  side_ = opponent(us);
  if (us == Color::Black) ++fullmove_;
}

void GameState::makeMove(Move const& m) {
  auto legal = legalMoves();
  if (std::find(legal.begin(), legal.end(), m) == legal.end()) {
    throw std::invalid_argument("Illegal move: " + m.toUci());
  }
  applyMoveUnchecked(m);
  positionHistory_.push_back(positionKey());
}

std::string GameState::positionKey() const {
  std::string key;
  for (int rank = 7; rank >= 0; --rank) {
    int empty = 0;
    for (int file = 0; file < 8; ++file) {
      Piece const* p = board_.at(Position{file, rank});
      if (p) {
        if (empty > 0) { key += std::to_string(empty); empty = 0; }
        key += p->fenLetter();
      } else {
        ++empty;
      }
    }
    if (empty > 0) key += std::to_string(empty);
    if (rank > 0) key += '/';
  }
  key += ' ';
  key += (side_ == Color::White) ? 'w' : 'b';
  key += ' ';
  key += castleString(*this);
  key += ' ';
  key += ep_ ? ep_->toAlgebraic() : "-";
  return key;
}

bool GameState::isThreefoldRepetition() const {
  if (positionHistory_.empty()) return false;
  std::string const& cur = positionHistory_.back();
  int count = 0;
  for (auto const& p : positionHistory_) {
    if (p == cur && ++count >= 3) return true;
  }
  return false;
}

bool GameState::isCheckmate() const {
  return isCheck() && legalMoves().empty();
}

bool GameState::isStalemate() const {
  return !isCheck() && legalMoves().empty();
}

GameState::Outcome GameState::outcome() const {
  if (isCheckmate()) {
    return side_ == Color::White ? Outcome::BlackWins : Outcome::WhiteWins;
  }
  if (isStalemate() || isFiftyMoveRule() || isThreefoldRepetition()) {
    return Outcome::Draw;
  }
  return Outcome::Ongoing;
}

std::string GameState::outcomeReason() const {
  if (isCheckmate())         return side_ == Color::White ? "checkmate, Black wins" : "checkmate, White wins";
  if (isStalemate())         return "stalemate, draw";
  if (isFiftyMoveRule())     return "fifty-move rule, draw";
  if (isThreefoldRepetition()) return "threefold repetition, draw";
  return "ongoing";
}

}  // namespace chess

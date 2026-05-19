#include "chess/Types.h"

namespace chess {

namespace {

char promotionToChar(PieceType t) {
  switch (t) {
    case PieceType::Queen:  return 'q';
    case PieceType::Rook:   return 'r';
    case PieceType::Bishop: return 'b';
    case PieceType::Knight: return 'n';
    default:                return '\0';
  }
}

PieceType promotionFromChar(char c) {
  switch (c) {
    case 'q': return PieceType::Queen;
    case 'r': return PieceType::Rook;
    case 'b': return PieceType::Bishop;
    case 'n': return PieceType::Knight;
    default:  return PieceType::None;
  }
}

}  // namespace

std::string Position::toAlgebraic() const {
  if (!valid()) return "??";
  return std::string{
      static_cast<char>('a' + file),
      static_cast<char>('1' + rank),
  };
}

std::optional<Position> Position::fromAlgebraic(std::string_view s) {
  if (s.size() != 2) return std::nullopt;
  if (s[0] < 'a' || s[0] > 'h') return std::nullopt;
  if (s[1] < '1' || s[1] > '8') return std::nullopt;
  return Position{s[0] - 'a', s[1] - '1'};
}

std::string Move::toUci() const {
  std::string out = from.toAlgebraic() + to.toAlgebraic();
  if (char c = promotionToChar(promotion); c) out += c;
  return out;
}

std::optional<Move> Move::fromUci(std::string_view s) {
  if (s.size() != 4 && s.size() != 5) return std::nullopt;
  auto from = Position::fromAlgebraic(s.substr(0, 2));
  auto to   = Position::fromAlgebraic(s.substr(2, 2));
  if (!from || !to) return std::nullopt;
  PieceType promo = PieceType::None;
  if (s.size() == 5) {
    promo = promotionFromChar(s[4]);
    if (promo == PieceType::None) return std::nullopt;
  }
  return Move{*from, *to, promo};
}

}  // namespace chess

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace chess {

enum class Color : std::uint8_t {
  White,
  Black,
};

constexpr Color opponent(Color c) {
  return c == Color::White ? Color::Black : Color::White;
}

enum class PieceType : std::uint8_t {
  None,
  Pawn,
  Knight,
  Bishop,
  Rook,
  Queen,
  King,
};

// Board square coordinates. file 0..7 maps to a..h; rank 0..7 maps to 1..8.
struct Position {
  int file = 0;
  int rank = 0;

  bool operator==(Position const&) const = default;

  bool valid() const { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

  int index() const { return rank * 8 + file; }
  static Position fromIndex(int i) { return Position{i % 8, i / 8}; }

  std::string toAlgebraic() const;
  static std::optional<Position> fromAlgebraic(std::string_view s);
};

struct Move {
  Position from;
  Position to;
  PieceType promotion = PieceType::None;

  bool operator==(Move const&) const = default;

  std::string toUci() const;
  static std::optional<Move> fromUci(std::string_view s);
};

}  // namespace chess

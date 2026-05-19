#pragma once

#include "chess/Types.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chess {

// Holds 12 piece sprites (6 types x 2 colors), pre-decoded to packed RGBA of
// size width*height*4, all at the target cell size given at construction.
// Each sprite is post-processed for legibility over busy mosaics (contrasting
// halo + translucent fill; see PieceSprites.cpp). Sprite PNGs are expected at
// `<assetsDir>/<name>.png` where name is one of wK,wQ,wR,wB,wN,wP,bK,bQ,bR,bB,bN,bP.
class PieceSprites {
public:
  PieceSprites(std::string assetsDir, int targetSize);

  int width() const  { return size_; }
  int height() const { return size_; }

  // Returns the packed RGBA bytes for the given piece. Lifetime tied to *this.
  std::vector<std::uint8_t> const& spriteFor(Color color, PieceType type) const;

private:
  std::string assetsDir_;
  int size_;
  std::array<std::vector<std::uint8_t>, 12> sprites_;

  static int slot(Color color, PieceType type);
  static std::string filename(Color color, PieceType type);
  std::vector<std::uint8_t> decode(std::string const& path) const;

  // Bakes the contrasting halo and translucent fill into a packed
  // size_*size_*4 RGBA buffer, in place.
  void addContrast(std::vector<std::uint8_t>& rgba, Color color) const;
};

}  // namespace chess

#pragma once

#include <array>
#include <cstdint>

namespace mosaic {

// Compact 5-pixel-wide, 7-pixel-tall bitmap font. Each row is a uint8 with the
// 5 low bits set: bit 4 = leftmost pixel, bit 0 = rightmost.
class BitmapFont {
public:
  static constexpr int kGlyphWidth = 5;
  static constexpr int kGlyphHeight = 7;
  using GlyphRows = std::array<std::uint8_t, kGlyphHeight>;

  int glyphWidth() const { return kGlyphWidth; }
  int glyphHeight() const { return kGlyphHeight; }

  // Looks up a glyph by character. Returns an all-zero space glyph for any
  // character outside [A-Z], [a-z], [0-9].
  GlyphRows const& rowsFor(char ch) const;
};

BitmapFont const& defaultFont();

}  // namespace mosaic

#pragma once

#include "mosaic/Geometry.h"

#include <cstdint>
#include <string_view>

namespace mosaic {

class BitmapFont;

struct RGB {
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
};

// Thin RGB24 painter over an externally-owned byte buffer. The buffer must
// hold canvasW * canvasH * 3 bytes and outlive the Painter.
class Painter {
public:
  Painter(std::uint8_t* pixels, int canvasW, int canvasH);

  int width() const { return canvasW_; }
  int height() const { return canvasH_; }

  void fill(RGB color);
  void fillRect(Rect const& rect, RGB color);

  // Copies a packed RGB24 image into dst. dst.w/dst.h must equal srcW/srcH;
  // dst must lie fully within the canvas.
  void blit(Rect const& dst, std::uint8_t const* srcPixels, int srcW, int srcH);

  // Alpha-blends a packed RGBA image onto the canvas at dst. dst.w/dst.h must
  // match srcW/srcH. Areas outside the canvas are clipped. Blend formula:
  //   out = src.rgb * a + dst.rgb * (255 - a), with a in [0, 255].
  void blitRgba(Rect const& dst, std::uint8_t const* srcPixels, int srcW, int srcH);

  // Draws `text` so its bounding box is centered on (centerX, centerY).
  // `scale` magnifies each glyph pixel into a scale×scale square.
  void drawText(BitmapFont const& font, std::string_view text,
                int centerX, int centerY, RGB color, int scale = 1);

private:
  std::uint8_t* pixels_;
  int canvasW_;
  int canvasH_;

  void putPixel(int x, int y, RGB color);
  void drawGlyph(BitmapFont const& font, char ch, int topLeftX, int topLeftY,
                 RGB color, int scale);
};

}  // namespace mosaic

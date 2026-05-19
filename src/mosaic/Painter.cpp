#include "mosaic/Painter.h"

#include "mosaic/Font.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace mosaic {

Painter::Painter(std::uint8_t* pixels, int canvasW, int canvasH)
    : pixels_(pixels), canvasW_(canvasW), canvasH_(canvasH) {
  if (!pixels_) throw std::invalid_argument("Painter: pixels is null");
  if (canvasW_ <= 0 || canvasH_ <= 0) {
    throw std::invalid_argument("Painter: canvas dimensions must be positive");
  }
}

void Painter::putPixel(int x, int y, RGB c) {
  if (x < 0 || x >= canvasW_ || y < 0 || y >= canvasH_) return;
  std::uint8_t* p = pixels_ + (static_cast<std::size_t>(y) * canvasW_ + x) * 3;
  p[0] = c.r;
  p[1] = c.g;
  p[2] = c.b;
}

void Painter::fill(RGB c) {
  fillRect(Rect{0, 0, canvasW_, canvasH_}, c);
}

void Painter::fillRect(Rect const& r, RGB c) {
  int x0 = std::max(0, r.x);
  int y0 = std::max(0, r.y);
  int x1 = std::min(canvasW_, r.x + r.w);
  int y1 = std::min(canvasH_, r.y + r.h);
  for (int y = y0; y < y1; ++y) {
    std::uint8_t* row = pixels_ + (static_cast<std::size_t>(y) * canvasW_ + x0) * 3;
    for (int x = x0; x < x1; ++x) {
      row[0] = c.r;
      row[1] = c.g;
      row[2] = c.b;
      row += 3;
    }
  }
}

void Painter::blit(Rect const& dst, std::uint8_t const* srcPixels, int srcW, int srcH) {
  if (!srcPixels) throw std::invalid_argument("Painter::blit: src is null");
  if (dst.w != srcW || dst.h != srcH) {
    throw std::invalid_argument("Painter::blit: dst dims must match src dims");
  }
  if (dst.x < 0 || dst.y < 0 ||
      dst.x + dst.w > canvasW_ || dst.y + dst.h > canvasH_) {
    throw std::out_of_range("Painter::blit: dst out of canvas bounds");
  }
  std::size_t const bytesPerRow = static_cast<std::size_t>(srcW) * 3;
  for (int row = 0; row < srcH; ++row) {
    std::uint8_t const* srcRow = srcPixels + static_cast<std::size_t>(row) * bytesPerRow;
    std::uint8_t* dstRow = pixels_ +
        (static_cast<std::size_t>(dst.y + row) * canvasW_ + dst.x) * 3;
    std::memcpy(dstRow, srcRow, bytesPerRow);
  }
}

void Painter::blitRgba(Rect const& dst, std::uint8_t const* srcPixels, int srcW, int srcH) {
  if (!srcPixels) throw std::invalid_argument("Painter::blitRgba: src is null");
  if (dst.w != srcW || dst.h != srcH) {
    throw std::invalid_argument("Painter::blitRgba: dst dims must match src dims");
  }
  int const x0 = std::max(0, dst.x);
  int const y0 = std::max(0, dst.y);
  int const x1 = std::min(canvasW_, dst.x + dst.w);
  int const y1 = std::min(canvasH_, dst.y + dst.h);
  if (x0 >= x1 || y0 >= y1) return;
  int const sxOff = x0 - dst.x;
  int const syOff = y0 - dst.y;
  for (int y = y0; y < y1; ++y) {
    std::uint8_t const* sRow = srcPixels +
        (static_cast<std::size_t>(syOff + (y - y0)) * srcW + sxOff) * 4;
    std::uint8_t* dRow = pixels_ +
        (static_cast<std::size_t>(y) * canvasW_ + x0) * 3;
    for (int x = x0; x < x1; ++x) {
      std::uint8_t const a = sRow[3];
      if (a == 0) {
        // fully transparent, nothing to do
      } else if (a == 255) {
        dRow[0] = sRow[0]; dRow[1] = sRow[1]; dRow[2] = sRow[2];
      } else {
        unsigned const inv = 255u - a;
        dRow[0] = static_cast<std::uint8_t>((sRow[0] * a + dRow[0] * inv + 127u) / 255u);
        dRow[1] = static_cast<std::uint8_t>((sRow[1] * a + dRow[1] * inv + 127u) / 255u);
        dRow[2] = static_cast<std::uint8_t>((sRow[2] * a + dRow[2] * inv + 127u) / 255u);
      }
      sRow += 4;
      dRow += 3;
    }
  }
}

void Painter::drawGlyph(BitmapFont const& font, char ch, int topLeftX, int topLeftY,
                        RGB color, int scale) {
  auto const& rows = font.rowsFor(ch);
  int const gw = font.glyphWidth();
  int const gh = font.glyphHeight();
  for (int row = 0; row < gh; ++row) {
    std::uint8_t bits = rows[row];
    for (int col = 0; col < gw; ++col) {
      bool on = bits & (1u << (gw - 1 - col));
      if (!on) continue;
      Rect block{topLeftX + col * scale, topLeftY + row * scale, scale, scale};
      fillRect(block, color);
    }
  }
}

void Painter::drawText(BitmapFont const& font, std::string_view text,
                       int centerX, int centerY, RGB color, int scale) {
  if (text.empty() || scale <= 0) return;
  int const gw = font.glyphWidth() * scale;
  int const gh = font.glyphHeight() * scale;
  int const spacing = scale;
  int const n = static_cast<int>(text.size());
  int const textW = n * gw + (n - 1) * spacing;
  int const startX = centerX - textW / 2;
  int const startY = centerY - gh / 2;
  int x = startX;
  for (char ch : text) {
    drawGlyph(font, ch, x, startY, color, scale);
    x += gw + spacing;
  }
}

}  // namespace mosaic

#include "chess/PieceSprites.h"

#include "mosaic/Subprocess.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace chess {

namespace {

// Legibility tuning: translucent fill (GIF shows through) plus a solid
// contrasting halo (dark behind light pieces, light behind dark) so the
// silhouette reads on any background.
constexpr double kFillAlpha = 0.75;            // piece interior opacity (0..1)
constexpr double kHaloFrac  = 1.0 / 22.0;      // halo thickness ÷ sprite size
constexpr int    kHaloMinPx = 2;               // ...but at least this many px

struct Rgb { int r, g, b; };
constexpr Rgb kHaloUnderLight{ 16,  16,  16};  // behind white pieces
constexpr Rgb kHaloUnderDark { 240, 240, 240}; // behind black pieces

// Morphological dilation (max filter) of an 8-bit plane with a disc of radius r.
std::vector<std::uint8_t> dilateDisc(std::vector<std::uint8_t> const& src,
                                     int w, int h, int r) {
  std::vector<std::pair<int, int>> offs;
  for (int dy = -r; dy <= r; ++dy)
    for (int dx = -r; dx <= r; ++dx)
      if (dx * dx + dy * dy <= r * r) offs.emplace_back(dx, dy);
  std::vector<std::uint8_t> out(src.size(), 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      int m = 0;
      for (auto const& [dx, dy] : offs) {
        int const nx = x + dx, ny = y + dy;
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
        m = std::max(m, static_cast<int>(src[static_cast<std::size_t>(ny) * w + nx]));
      }
      out[static_cast<std::size_t>(y) * w + x] = static_cast<std::uint8_t>(m);
    }
  }
  return out;
}

// One 3x3 box-blur pass: softens the dilated halo's outer edge.
std::vector<std::uint8_t> boxBlur3(std::vector<std::uint8_t> const& src, int w, int h) {
  std::vector<std::uint8_t> out(src.size());
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      int sum = 0, n = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int const nx = x + dx, ny = y + dy;
          if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
          sum += src[static_cast<std::size_t>(ny) * w + nx];
          ++n;
        }
      out[static_cast<std::size_t>(y) * w + x] =
          static_cast<std::uint8_t>((sum + n / 2) / n);
    }
  }
  return out;
}

char colorChar(Color c)         { return c == Color::White ? 'w' : 'b'; }
char pieceLetterUpper(PieceType t) {
  switch (t) {
    case PieceType::King:   return 'K';
    case PieceType::Queen:  return 'Q';
    case PieceType::Rook:   return 'R';
    case PieceType::Bishop: return 'B';
    case PieceType::Knight: return 'N';
    case PieceType::Pawn:   return 'P';
    default: return '?';
  }
}

int pieceIndex(PieceType t) {
  switch (t) {
    case PieceType::Pawn:   return 0;
    case PieceType::Knight: return 1;
    case PieceType::Bishop: return 2;
    case PieceType::Rook:   return 3;
    case PieceType::Queen:  return 4;
    case PieceType::King:   return 5;
    default: throw std::invalid_argument("PieceSprites: invalid piece type");
  }
}

}  // namespace

int PieceSprites::slot(Color color, PieceType type) {
  return (color == Color::White ? 0 : 6) + pieceIndex(type);
}

std::string PieceSprites::filename(Color color, PieceType type) {
  return std::string{} + colorChar(color) + pieceLetterUpper(type) + ".png";
}

PieceSprites::PieceSprites(std::string assetsDir, int targetSize)
    : assetsDir_(std::move(assetsDir)), size_(targetSize) {
  if (size_ <= 0) throw std::invalid_argument("PieceSprites: targetSize must be positive");
  for (Color c : {Color::White, Color::Black}) {
    for (PieceType t : {PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                        PieceType::Rook, PieceType::Queen, PieceType::King}) {
      std::string path = assetsDir_ + "/" + filename(c, t);
      auto sprite = decode(path);
      addContrast(sprite, c);
      sprites_[slot(c, t)] = std::move(sprite);
    }
  }
}

std::vector<std::uint8_t> const& PieceSprites::spriteFor(Color color, PieceType type) const {
  return sprites_[slot(color, type)];
}

std::vector<std::uint8_t> PieceSprites::decode(std::string const& path) const {
  std::string scale = "scale=" + std::to_string(size_) + ":" + std::to_string(size_) +
                      ":flags=lanczos";
  mosaic::Subprocess proc({
      "ffmpeg", "-v", "error",
      "-i", path,
      "-vf", scale,
      "-pix_fmt", "rgba",
      "-f", "rawvideo",
      "-",
  });

  std::size_t const expected =
      static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_) * 4u;
  std::vector<std::uint8_t> out(expected);
  std::size_t got = proc.read(out.data(), expected);
  if (got != expected) {
    throw std::runtime_error(
        "PieceSprites: short read for " + path + " (got " + std::to_string(got) +
        " of " + std::to_string(expected) + " bytes)");
  }
  int rc = proc.wait();
  if (rc != 0) {
    throw std::runtime_error(
        "PieceSprites: ffmpeg exited " + std::to_string(rc) + " for " + path);
  }
  return out;
}

void PieceSprites::addContrast(std::vector<std::uint8_t>& rgba, Color color) const {
  int const n = size_ * size_;
  if (static_cast<int>(rgba.size()) != n * 4) return;  // unexpected; leave as-is

  // Pull out the silhouette's alpha plane, then grow it into a halo mask.
  std::vector<std::uint8_t> alpha(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) alpha[static_cast<std::size_t>(i)] = rgba[static_cast<std::size_t>(i) * 4 + 3];

  int const r = std::max(kHaloMinPx, static_cast<int>(std::lround(size_ * kHaloFrac)));
  std::vector<std::uint8_t> grown = boxBlur3(dilateDisc(alpha, size_, size_, r), size_, size_);

  Rgb const halo = (color == Color::White) ? kHaloUnderLight : kHaloUnderDark;

  for (int i = 0; i < n; ++i) {
    std::size_t const o = static_cast<std::size_t>(i) * 4;
    // Halo only fills the ring *around* the silhouette, not under it, so the
    // translucent fill keeps showing the GIF where the piece body is.
    double const ringCov = std::max(0, grown[static_cast<std::size_t>(i)] - alpha[static_cast<std::size_t>(i)]) / 255.0;
    double const pieceA  = (alpha[static_cast<std::size_t>(i)] / 255.0) * kFillAlpha;
    // Composite the (translucent) piece over the (opaque) halo, straight alpha.
    double const outA = pieceA + ringCov * (1.0 - pieceA);
    if (outA <= 0.0) { rgba[o] = rgba[o + 1] = rgba[o + 2] = rgba[o + 3] = 0; continue; }
    int const hc[3] = {halo.r, halo.g, halo.b};
    for (int c = 0; c < 3; ++c) {
      double const v = (rgba[o + c] * pieceA + hc[c] * ringCov * (1.0 - pieceA)) / outA;
      rgba[o + c] = static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0) + 0.5);
    }
    rgba[o + 3] = static_cast<std::uint8_t>(std::clamp(outA * 255.0, 0.0, 255.0) + 0.5);
  }
}

}  // namespace chess

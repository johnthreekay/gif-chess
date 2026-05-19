#include "mosaic/MosaicComposer.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace mosaic {

MosaicComposer::MosaicComposer(GridLayout layout,
                               std::vector<VideoSource> sources,
                               std::unique_ptr<CellAssignment> assignment,
                               std::unique_ptr<LabelRenderer> labels,
                               RGB background)
    : layout_(std::move(layout)),
      sources_(std::move(sources)),
      assignment_(std::move(assignment)),
      labels_(std::move(labels)),
      background_(background) {
  if (!assignment_) {
    throw std::invalid_argument("MosaicComposer: assignment must not be null");
  }
  int const cell = layout_.cellSize();
  for (std::size_t i = 0; i < sources_.size(); ++i) {
    auto const& s = sources_[i];
    if (s.width() != cell || s.height() != cell) {
      throw std::invalid_argument(
          "MosaicComposer: source #" + std::to_string(i) + " is " +
          std::to_string(s.width()) + "x" + std::to_string(s.height()) +
          " but layout cellSize is " + std::to_string(cell));
    }
  }
}

Frame MosaicComposer::composite(double timeMs) const {
  int const w = layout_.canvasWidth();
  int const h = layout_.canvasHeight();
  Frame canvas(static_cast<std::size_t>(w) * h * 3);
  Painter painter(canvas.data(), w, h);

  painter.fill(background_);

  for (int r = 0; r < layout_.rows(); ++r) {
    for (int c = 0; c < layout_.cols(); ++c) {
      VideoSource const* src = assignment_->sourceFor(r, c);
      if (!src) continue;
      Rect cell = layout_.cellRect(r, c);
      Frame const& frame = src->frameAt(timeMs);
      painter.blit(cell, frame.data(), src->width(), src->height());
    }
  }

  if (labels_) {
    labels_->drawLabels(painter, layout_);
  }

  return canvas;
}

}  // namespace mosaic

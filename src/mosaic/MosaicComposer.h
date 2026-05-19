#pragma once

#include "mosaic/CellAssignment.h"
#include "mosaic/GridLayout.h"
#include "mosaic/LabelRenderer.h"
#include "mosaic/Painter.h"
#include "mosaic/VideoSource.h"

#include <memory>
#include <vector>

namespace mosaic {

// Composes a single mosaic frame from a fixed set of VideoSources arranged by
// a CellAssignment. Owns the layout, source pool, assignment, and optional
// label renderer. Source dimensions must equal layout.cellSize(): validated
// in the ctor so composite() can blit without per-frame checks.
class MosaicComposer {
public:
  MosaicComposer(GridLayout layout,
                 std::vector<VideoSource> sources,
                 std::unique_ptr<CellAssignment> assignment,
                 std::unique_ptr<LabelRenderer> labels = nullptr,
                 RGB background = RGB{255, 255, 255});

  MosaicComposer(MosaicComposer const&) = delete;
  MosaicComposer& operator=(MosaicComposer const&) = delete;
  MosaicComposer(MosaicComposer&&) = default;
  MosaicComposer& operator=(MosaicComposer&&) = default;

  GridLayout const& layout() const { return layout_; }

  // Returns a freshly allocated canvas of layout.canvasWidth() *
  // canvasHeight() * 3 bytes of packed RGB24 with the mosaic painted in.
  Frame composite(double timeMs) const;

private:
  GridLayout layout_;
  std::vector<VideoSource> sources_;
  std::unique_ptr<CellAssignment> assignment_;
  std::unique_ptr<LabelRenderer> labels_;
  RGB background_;
};

}  // namespace mosaic

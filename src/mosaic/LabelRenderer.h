#pragma once

#include "mosaic/Painter.h"

namespace mosaic {

class BitmapFont;
class GridLayout;

class LabelRenderer {
public:
  virtual ~LabelRenderer() = default;
  virtual void drawLabels(Painter& painter, GridLayout const& layout) const = 0;
};

class MarginLabelRenderer : public LabelRenderer {
public:
  explicit MarginLabelRenderer(RGB color = RGB{0, 0, 0}, int scale = 2,
                               BitmapFont const* font = nullptr);

  void drawLabels(Painter& painter, GridLayout const& layout) const override;

private:
  RGB color_;
  int scale_;
  BitmapFont const* font_;
};

}  // namespace mosaic

#pragma once

#include <cstdint>
#include <vector>

namespace mosaic {

class VideoSource;

// Maps (row, col) cells to entries in a non-owning source pool. Subclasses
// implement sourceIndexFor(); sourceFor() is a non-virtual translation
// through `sources_`.
class CellAssignment {
public:
  CellAssignment(int rows, int cols, std::vector<VideoSource const*> sources);
  virtual ~CellAssignment() = default;

  CellAssignment(CellAssignment const&) = delete;
  CellAssignment& operator=(CellAssignment const&) = delete;

  int rows() const { return rows_; }
  int cols() const { return cols_; }
  std::size_t sourceCount() const { return sources_.size(); }

  // Returns the index into the source pool for the cell, or -1 if empty.
  virtual int sourceIndexFor(int row, int col) const = 0;

  // Convenience: index -> pointer (nullptr if empty or out of pool range).
  VideoSource const* sourceFor(int row, int col) const;

protected:
  int rows_;
  int cols_;
  std::vector<VideoSource const*> sources_;
};

class RepeatTile : public CellAssignment {
public:
  RepeatTile(int rows, int cols, std::vector<VideoSource const*> sources);
  int sourceIndexFor(int row, int col) const override;
};

class RandomFill : public CellAssignment {
public:
  // If allowRepeats is false, throws when rows*cols > sources.size().
  RandomFill(int rows, int cols, std::vector<VideoSource const*> sources,
             std::uint64_t seed, bool allowRepeats);
  int sourceIndexFor(int row, int col) const override;

private:
  std::vector<int> placement_;
};

class ManualPlacement : public CellAssignment {
public:
  // `placement` is row-major (size == rows*cols). Each entry is a source
  // index, or -1 to leave the cell empty.
  ManualPlacement(int rows, int cols, std::vector<VideoSource const*> sources,
                  std::vector<int> placement);
  int sourceIndexFor(int row, int col) const override;

private:
  std::vector<int> placement_;
};

}  // namespace mosaic

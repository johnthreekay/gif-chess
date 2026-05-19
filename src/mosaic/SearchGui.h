#pragma once

#include "mosaic/Downloader.h"
#include "mosaic/Library.h"
#include "mosaic/SearchProvider.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mosaic {

// SDL window for building a mosaic library: search Tenor/Giphy/Klipy, click
// thumbnails to add, or pick a local file via the system dialog. Mutates the
// Library it was handed. run() returns when the window closes, reporting any
// requested jump into preview/export/chess.
class SearchGui {
public:
  enum class Result { None, Preview, Export, Chess };

  SearchGui(Library& library,
            std::vector<std::unique_ptr<SearchProvider>>& providers,
            Downloader& downloader);

  Result run();

  // Where the user asked to export (set when run() returns Result::Export).
  std::string const& requestedExportPath() const { return exportPath_; }

private:
  struct ResultTile {
    SearchResult result;
    std::string localPath;
    std::vector<std::uint8_t> thumb;  // RGB24, kThumb x kThumb; empty until ready
    enum class State { Pending, Ready, Failed } state = State::Pending;
    bool added = false;
  };

  Library& library_;
  std::vector<std::unique_ptr<SearchProvider>>& providers_;
  Downloader& downloader_;

  std::string query_;
  std::size_t activeProvider_ = 0;
  std::vector<ResultTile> results_;
  std::string status_;
  std::string exportPath_ = "out.mp4";
  int page_ = 1;

  void doSearch();          // fresh search, page 1
  void goToPage(int page);  // re-run the current query on another page
  void loadOnePendingThumbnail();
  void addResult(std::size_t idx);
  void addLocalFileViaDialog();
  // Returns the chosen save path, or empty if the user cancelled.
  std::string pickSavePathViaDialog();
};

}  // namespace mosaic

#pragma once

#include "mosaic/Downloader.h"
#include "mosaic/Library.h"
#include "mosaic/SearchProvider.h"

#include <memory>
#include <string>
#include <vector>

namespace mosaic {

// Interactive REPL for composing a mosaic from local files, direct URLs, and
// provider search results. Drives the preview / export / chess game flows
// with whatever's currently in the library.
class Prompt {
public:
  Prompt();
  void run();

  // Opens the SDL search/browse window, then acts on whichever follow-up
  // (preview / export / chess) the user picked there, if any.
  void openBrowser();

private:
  Downloader downloader_;
  Library library_;
  std::vector<std::unique_ptr<SearchProvider>> providers_;
  std::vector<SearchResult> lastResults_;
  std::string lastSearchProvider_;
  std::string lastSearchQuery_;
  int searchPage_ = 1;

  // Stockfish strength. engineElo_ > 0 selects calibrated UCI_LimitStrength
  // (engineSkill_ ignored); otherwise the Skill Level knob (0=weakest,
  // 20=full) applies.
  int engineSkill_ = 5;
  int engineElo_ = 0;

  void printHelp() const;
  void printList() const;
  void printResults() const;

  void cmdAdd(std::vector<std::string> const& args);
  void cmdSearch(std::vector<std::string> const& args);
  void runSearchPage(int page);
  void cmdPick(std::vector<std::string> const& args);
  void cmdRemove(std::vector<std::string> const& args);
  void cmdPreview(std::vector<std::string> const& args);
  void cmdExport(std::vector<std::string> const& args);
  void cmdChess(std::vector<std::string> const& args);
  void cmdChessVideo(std::string const& videoPath, int squareSize);
  void cmdEngine(std::vector<std::string> const& args);
  void printEngineStrength() const;
  void cmdBrowse();

  SearchProvider* findProvider(std::string const& name);

  // Resolves a path-or-URL into a local file. If it's an http(s) URL, downloads
  // into the downloader's cache dir and returns the new path.
  struct AddedItem { std::string label; std::string path; };
  AddedItem materialize(std::string const& spec);
};

}  // namespace mosaic

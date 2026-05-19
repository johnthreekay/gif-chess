#include "mosaic/Prompt.h"

#include "chess/ChessLabelRenderer.h"
#include "chess/ChessOverlay.h"
#include "chess/Engine.h"
#include "chess/Game.h"
#include "chess/PieceSprites.h"
#include "chess/StockfishEngine.h"
#include "mosaic/AssetPath.h"
#include "mosaic/CellAssignment.h"
#include "mosaic/GridLayout.h"
#include "mosaic/LabelRenderer.h"
#include "mosaic/MosaicComposer.h"
#include "mosaic/MosaicExporter.h"
#include "mosaic/SDLRenderer.h"
#include "mosaic/SearchGui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mosaic {

namespace {

std::vector<std::string> tokenize(std::string const& line) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) { out.push_back(std::move(cur)); cur.clear(); }
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) out.push_back(std::move(cur));
  return out;
}

bool isUrl(std::string const& s) {
  return s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0;
}

std::string sanitizeForFilename(std::string s) {
  for (char& c : s) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) {
      c = '_';
    }
  }
  return s;
}

std::string basenameOf(std::string const& path) {
  auto slash = path.find_last_of('/');
  return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

std::string extOf(std::string const& url) {
  auto q = url.find('?');
  std::string u = (q == std::string::npos) ? url : url.substr(0, q);
  auto dot = u.find_last_of('.');
  auto slash = u.find_last_of('/');
  if (dot == std::string::npos) return ".bin";
  if (slash != std::string::npos && dot < slash) return ".bin";
  return u.substr(dot);
}

// Builds an N-cell mosaic composer from the library; throws if it has no
// usable sources.
struct ComposerBundle {
  MosaicComposer composer;
  int rows;
  int cols;
};

ComposerBundle buildComposer(Library const& lib, int rows, int cols, int cellSize,
                              std::unique_ptr<LabelRenderer> labels,
                              std::uint64_t seed) {
  auto loaded = lib.load(cellSize);
  for (auto const& f : loaded.failures) {
    std::cout << "  ! skipped: " << f << "\n";
  }
  if (loaded.sources.empty()) {
    throw std::runtime_error("library has no usable sources");
  }
  std::vector<VideoSource const*> ptrs;
  ptrs.reserve(loaded.sources.size());
  for (auto const& s : loaded.sources) ptrs.push_back(&s);
  int const n = static_cast<int>(ptrs.size());

  // Deal sources out evenly across cells, then shuffle so there's no fixed
  // positional pattern.
  int const totalCells = rows * cols;
  std::vector<int> placement(static_cast<std::size_t>(totalCells));
  for (int i = 0; i < totalCells; ++i) placement[static_cast<std::size_t>(i)] = i % n;
  std::mt19937_64 rng(seed);
  std::shuffle(placement.begin(), placement.end(), rng);

  GridLayout layout(rows, cols, cellSize, /*margin=*/24, LabelStyle::Margin);
  auto assignment = std::make_unique<ManualPlacement>(rows, cols, std::move(ptrs),
                                                       std::move(placement));

  MosaicComposer composer(std::move(layout), std::move(loaded.sources),
                          std::move(assignment), std::move(labels));
  return {std::move(composer), rows, cols};
}

// Smallest near-square grid that holds n cells: cols = ceil(sqrt(n)), rows so
// that rows*cols >= n. Ensures every library source gets at least one cell.
std::pair<int, int> autoGrid(std::size_t n) {
  if (n <= 1) return {1, 1};
  int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
  if (cols < 1) cols = 1;
  int rows = static_cast<int>((n + static_cast<std::size_t>(cols) - 1) /
                              static_cast<std::size_t>(cols));
  return {rows, cols};
}

std::unique_ptr<chess::Engine> makePromptEngine(int skill, int elo) {
  try {
    if (elo > 0)
      return std::make_unique<chess::StockfishEngine>(/*skill=*/-1, /*uciElo=*/elo);
    return std::make_unique<chess::StockfishEngine>(/*skill=*/skill);
  } catch (std::exception const& e) {
    std::cout << "  (stockfish unavailable: " << e.what()
              << " - using RandomEngine)\n";
    return std::make_unique<chess::RandomEngine>(/*seed=*/42);
  }
}

}  // namespace

Prompt::Prompt()
    : downloader_("cache"),
      providers_(makeDefaultProviders(downloader_)) {}

void Prompt::printHelp() const {
  std::cout <<
    "Commands:\n"
    "  browse                           open the search/add GUI window\n"
    "  add <path-or-url> ...            add files or download direct URLs\n"
    "  search <provider> <query>        search tenor / giphy / klipy (text mode)\n"
    "  next | prev | page <n>           page through the last search's results\n"
    "  pick <n> [<n>...]                add picks from the last search page\n"
    "  list                             show library\n"
    "  remove <index>                   drop an item\n"
    "  clear                            empty the library\n"
    "  preview [<cellSize>]             SDL preview (default 80)\n"
    "  export [<path>] [<cellSize>]     write MP4 (defaults: out.mp4, 100)\n"
    "  chess [<cellSize>]               interactive game over the library mosaic (default 80)\n"
    "  chess <video.mp4> [<sq>]         interactive game over a single video stretched on the board\n"
    "  engine                           show Stockfish strength\n"
    "  engine skill <0-20>              weaken Stockfish via Skill Level (default 5)\n"
    "  engine elo <1320-3190>           cap Stockfish to a rating (UCI_LimitStrength)\n"
    "  help                             this message\n"
    "  quit / exit                      leave\n";
  std::cout << "Providers loaded: ";
  if (providers_.empty()) std::cout << "(none)";
  for (std::size_t i = 0; i < providers_.size(); ++i) {
    std::cout << (i ? ", " : "") << providers_[i]->name();
  }
  std::cout << "\n";
  std::cout << "Tip: set TENOR_API_KEY / GIPHY_API_KEY in your env to enable search.\n";
}

void Prompt::printList() const {
  if (library_.empty()) { std::cout << "  (empty)\n"; return; }
  for (std::size_t i = 0; i < library_.size(); ++i) {
    auto const& it = library_.items()[i];
    std::cout << "  " << i << ": " << it.label << "  [" << it.path << "]\n";
  }
}

void Prompt::printResults() const {
  if (lastResults_.empty()) { std::cout << "  (no results yet - try `search`)\n"; return; }
  for (std::size_t i = 0; i < lastResults_.size(); ++i) {
    auto const& r = lastResults_[i];
    std::cout << "  " << i << ": [" << r.providerId << "] " << r.title
              << "  (" << r.id << ")\n";
  }
}

Prompt::AddedItem Prompt::materialize(std::string const& spec) {
  if (isUrl(spec)) {
    std::string base = sanitizeForFilename(basenameOf(spec));
    if (base.empty() || base.find('.') == std::string::npos) {
      base = "remote-" + std::to_string(library_.size()) + extOf(spec);
    }
    std::string path = downloader_.download(spec, base);
    return {basenameOf(spec), path};
  }
  if (!std::filesystem::exists(spec)) {
    throw std::runtime_error("no such file: " + spec);
  }
  return {basenameOf(spec), spec};
}

void Prompt::cmdAdd(std::vector<std::string> const& args) {
  if (args.empty()) { std::cout << "  usage: add <path-or-url> ...\n"; return; }
  for (auto const& spec : args) {
    try {
      auto item = materialize(spec);
      library_.add(item.label, item.path);
      std::cout << "  + [" << (library_.size() - 1) << "] " << item.label << "\n";
    } catch (std::exception const& e) {
      std::cout << "  ! " << spec << ": " << e.what() << "\n";
    }
  }
}

SearchProvider* Prompt::findProvider(std::string const& name) {
  for (auto& p : providers_) if (p->name() == name) return p.get();
  return nullptr;
}

void Prompt::cmdSearch(std::vector<std::string> const& args) {
  if (args.size() < 2) {
    std::cout << "  usage: search <provider> <query...>\n";
    std::cout << "  available: ";
    for (std::size_t i = 0; i < providers_.size(); ++i) {
      std::cout << (i ? ", " : "") << providers_[i]->name();
    }
    std::cout << "\n";
    return;
  }
  SearchProvider* p = findProvider(args[0]);
  if (!p) {
    std::cout << "  unknown provider '" << args[0] << "'. ";
    if (args[0] == "tenor")  std::cout << "Set TENOR_API_KEY to enable.";
    if (args[0] == "giphy")  std::cout << "Set GIPHY_API_KEY to enable.";
    std::cout << "\n";
    return;
  }
  std::string query;
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (i > 1) query += ' ';
    query += args[i];
  }
  lastSearchProvider_ = p->name();
  lastSearchQuery_ = query;
  runSearchPage(/*page=*/1);
}

void Prompt::runSearchPage(int page) {
  if (lastSearchQuery_.empty()) {
    std::cout << "  nothing searched yet - try `search <provider> <query>`\n";
    return;
  }
  SearchProvider* p = findProvider(lastSearchProvider_);
  if (!p) { std::cout << "  provider '" << lastSearchProvider_ << "' not available\n"; return; }
  if (page < 1) page = 1;
  try {
    auto hits = p->search(lastSearchQuery_, /*perPage=*/12, page);
    if (hits.empty() && page > 1) {
      std::cout << "  no more results (still on page " << searchPage_ << ")\n";
      return;  // keep lastResults_ and searchPage_ where they were
    }
    lastResults_ = std::move(hits);
    searchPage_ = page;
    if (lastResults_.empty()) {
      std::cout << "  no results for \"" << lastSearchQuery_ << "\" on " << p->name() << "\n";
    } else {
      std::cout << "  " << lastResults_.size() << " results, page " << searchPage_
                << " - `pick <n>` to add, `next`/`prev` to page\n";
      printResults();
    }
  } catch (std::exception const& e) {
    std::cout << "  search failed: " << e.what() << "\n";
  }
}

void Prompt::cmdPick(std::vector<std::string> const& args) {
  if (args.empty()) { std::cout << "  usage: pick <n> [<n>...]\n"; return; }
  if (lastResults_.empty()) { std::cout << "  no search results to pick from\n"; return; }
  for (auto const& a : args) {
    int idx = -1;
    try { idx = std::stoi(a); } catch (...) {}
    if (idx < 0 || idx >= static_cast<int>(lastResults_.size())) {
      std::cout << "  ! bad index '" << a << "'\n"; continue;
    }
    auto const& r = lastResults_[idx];
    try {
      std::string fname = sanitizeForFilename(r.providerId + "-" + r.id) + extOf(r.downloadUrl);
      std::string path = downloader_.download(r.downloadUrl, fname);
      std::string label = "[" + r.providerId + "] " + (r.title.empty() ? r.id : r.title);
      library_.add(label, path);
      std::cout << "  + [" << (library_.size() - 1) << "] " << label << "\n";
    } catch (std::exception const& e) {
      std::cout << "  ! pick " << idx << ": " << e.what() << "\n";
    }
  }
}

void Prompt::cmdRemove(std::vector<std::string> const& args) {
  if (args.empty()) { std::cout << "  usage: remove <index>\n"; return; }
  try {
    std::size_t idx = static_cast<std::size_t>(std::stoul(args[0]));
    library_.remove(idx);
    std::cout << "  - removed [" << idx << "]\n";
  } catch (std::exception const& e) {
    std::cout << "  ! " << e.what() << "\n";
  }
}

void Prompt::cmdPreview(std::vector<std::string> const& args) {
  if (library_.empty()) { std::cout << "  library is empty - add something first\n"; return; }
  int cellSize = 80;
  if (!args.empty()) try { cellSize = std::stoi(args[0]); } catch (...) {}

  auto [rows, cols] = autoGrid(library_.size());
  auto labels = std::make_unique<MarginLabelRenderer>(RGB{0, 0, 0}, /*scale=*/2);
  try {
    auto bundle = buildComposer(library_, rows, cols, cellSize, std::move(labels),
                                /*seed=*/42);
    SDLRenderer renderer(std::move(bundle.composer), /*targetFps=*/30, "mosaic preview");
    renderer.run();
  } catch (std::exception const& e) {
    std::cout << "  ! preview failed: " << e.what() << "\n";
  }
}

void Prompt::cmdExport(std::vector<std::string> const& args) {
  if (library_.empty()) { std::cout << "  library is empty - add something first\n"; return; }
  std::string outPath = "out.mp4";
  int cellSize = 100;
  if (args.size() >= 1) outPath = args[0];
  if (args.size() >= 2) try { cellSize = std::stoi(args[1]); } catch (...) {}

  auto [rows, cols] = autoGrid(library_.size());
  auto labels = std::make_unique<MarginLabelRenderer>(RGB{0, 0, 0}, /*scale=*/2);
  try {
    auto bundle = buildComposer(library_, rows, cols, cellSize, std::move(labels),
                                /*seed=*/42);
    MosaicExporter exporter(std::move(bundle.composer), outPath,
                            /*fps=*/30, /*durationSec=*/10.0);
    exporter.run();
  } catch (std::exception const& e) {
    std::cout << "  ! export failed: " << e.what() << "\n";
  }
}

void Prompt::cmdChessVideo(std::string const& videoPath, int squareSize) {
  int const margin = 24;
  int const boardPx = 8 * squareSize;
  try {
    std::vector<VideoSource> sources;
    sources.reserve(1);
    sources.emplace_back(videoPath, boardPx, boardPx);
    std::vector<VideoSource const*> ptrs{&sources.front()};

    // 1x1 mosaic: one cell == the whole board area, video stretched into it.
    GridLayout mosaicLayout(/*rows=*/1, /*cols=*/1, boardPx, margin, LabelStyle::Margin);
    auto assignment = std::make_unique<RepeatTile>(1, 1, std::move(ptrs));
    MosaicComposer composer(std::move(mosaicLayout), std::move(sources),
                            std::move(assignment), /*labels=*/nullptr);

    // Independent 8x8 layout for the chess board (same canvas dimensions).
    GridLayout chessLayout(8, 8, squareSize, margin, LabelStyle::Margin);
    chess::PieceSprites sprites(assetRoot() + "/pieces", squareSize);
    chess::ChessOverlay overlay(chessLayout, std::move(sprites));
    printEngineStrength();
    auto engine = makePromptEngine(engineSkill_, engineElo_);
    chess::Game game(std::move(composer), std::move(overlay), std::move(engine),
                     chess::Color::White, /*engineThinkMs=*/500, /*targetFps=*/30,
                     "gif-chess");
    game.run();
  } catch (std::exception const& e) {
    std::cout << "  ! chess (video bg) failed: " << e.what() << "\n";
  }
}

void Prompt::cmdChess(std::vector<std::string> const& args) {
  // `chess skill N` / `chess elo N` / `chess engine ...`: adjust strength only.
  if (!args.empty() && (args[0] == "skill" || args[0] == "elo")) {
    cmdEngine(args);
    return;
  }
  if (!args.empty() && (args[0] == "engine" || args[0] == "strength")) {
    cmdEngine(std::vector<std::string>(args.begin() + 1, args.end()));
    return;
  }
  // `chess <file>`: that video stretched over the whole board.
  if (!args.empty() && std::filesystem::exists(args[0])) {
    int sq = 80;
    if (args.size() >= 2) try { sq = std::stoi(args[1]); } catch (...) {}
    cmdChessVideo(args[0], sq);
    return;
  }
  // `chess [<cellSize>]`: tile the library across the 64 squares.
  if (library_.empty()) { std::cout << "  library is empty - add something first\n"; return; }
  int cellSize = 80;
  if (!args.empty()) try { cellSize = std::stoi(args[0]); } catch (...) {}

  try {
    auto bundle = buildComposer(library_, /*rows=*/8, /*cols=*/8, cellSize,
                                /*labels=*/nullptr, /*seed=*/42);
    chess::PieceSprites sprites(assetRoot() + "/pieces", cellSize);
    chess::ChessOverlay overlay(bundle.composer.layout(), std::move(sprites));
    printEngineStrength();
    auto engine = makePromptEngine(engineSkill_, engineElo_);
    chess::Game game(std::move(bundle.composer), std::move(overlay),
                     std::move(engine),
                     /*humanColor=*/chess::Color::White,
                     /*engineThinkMs=*/500,
                     /*targetFps=*/30,
                     "gif-chess");
    game.run();
  } catch (std::exception const& e) {
    std::cout << "  ! chess failed: " << e.what() << "\n";
  }
}

void Prompt::printEngineStrength() const {
  if (engineElo_ > 0)
    std::cout << "  engine: Stockfish, UCI_Elo ~" << engineElo_
              << " (LimitStrength)\n";
  else
    std::cout << "  engine: Stockfish, Skill Level " << engineSkill_
              << " (0 = weakest, 20 = full strength)\n";
}

// `engine` / `engine skill <0-20>` / `engine elo <1320-3190>` / `engine max`.
void Prompt::cmdEngine(std::vector<std::string> const& args) {
  if (args.empty()) { printEngineStrength(); return; }

  std::string const& mode = args[0];
  auto parseInt = [](std::string const& s, int& out) {
    try { out = std::stoi(s); return true; } catch (...) { return false; }
  };

  if (mode == "skill") {
    int v;
    if (args.size() < 2 || !parseInt(args[1], v)) {
      std::cout << "  usage: engine skill <0-20>\n"; return;
    }
    engineSkill_ = std::clamp(v, 0, 20);
    engineElo_ = 0;
  } else if (mode == "elo") {
    int v;
    if (args.size() < 2 || !parseInt(args[1], v)) {
      std::cout << "  usage: engine elo <1320-3190>\n"; return;
    }
    engineElo_ = std::clamp(v, 1320, 3190);
  } else if (mode == "max" || mode == "full") {
    engineSkill_ = 20;
    engineElo_ = 0;
  } else {
    std::cout << "  usage: engine                  show current strength\n"
                 "         engine skill <0-20>     weaken via Stockfish Skill Level\n"
                 "         engine elo <1320-3190>  target a rating (UCI_LimitStrength)\n"
                 "         engine max              full strength\n";
    return;
  }
  printEngineStrength();
  std::cout << "  (applies next time you start a game)\n";
}

void Prompt::cmdBrowse() {
  // After a preview/export/chess action, return to the browse window (library
  // persists). Esc in the window ends the loop.
  while (true) {
    SearchGui gui(library_, providers_, downloader_);
    SearchGui::Result r = gui.run();
    if      (r == SearchGui::Result::Preview) cmdPreview({});
    else if (r == SearchGui::Result::Export)  cmdExport({gui.requestedExportPath()});
    else if (r == SearchGui::Result::Chess)   cmdChess({});
    else /* None */                           return;
  }
}

void Prompt::openBrowser() { cmdBrowse(); }

void Prompt::run() {
  std::cout << "gif-chess prompt - type `help` for commands\n";
  printHelp();
  std::string line;
  while (true) {
    std::cout << "mosaic> " << std::flush;
    if (!std::getline(std::cin, line)) break;
    auto tokens = tokenize(line);
    if (tokens.empty()) continue;
    std::string cmd = tokens.front();
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    if      (cmd == "help" || cmd == "?")      printHelp();
    else if (cmd == "add")                     cmdAdd(args);
    else if (cmd == "search")                  cmdSearch(args);
    else if (cmd == "next" || cmd == "more")   runSearchPage(searchPage_ + 1);
    else if (cmd == "prev")                    runSearchPage(searchPage_ > 1 ? searchPage_ - 1 : 1);
    else if (cmd == "page") {
      int n = 1;
      if (!args.empty()) try { n = std::stoi(args[0]); } catch (...) {}
      runSearchPage(n);
    }
    else if (cmd == "pick")                    cmdPick(args);
    else if (cmd == "list" || cmd == "ls")     printList();
    else if (cmd == "results")                 printResults();
    else if (cmd == "remove" || cmd == "rm")   cmdRemove(args);
    else if (cmd == "clear")                   { library_.clear(); std::cout << "  cleared\n"; }
    else if (cmd == "browse" || cmd == "gui")  cmdBrowse();
    else if (cmd == "preview")                 cmdPreview(args);
    else if (cmd == "export")                  cmdExport(args);
    else if (cmd == "chess")                   cmdChess(args);
    else if (cmd == "engine" || cmd == "strength") cmdEngine(args);
    else if (cmd == "quit" || cmd == "exit")   break;
    else std::cout << "  unknown command: " << cmd << " (type `help`)\n";
  }
  std::cout << "bye\n";
}

}  // namespace mosaic

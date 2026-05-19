#include "mosaic/AssetPath.h"
#include "mosaic/CellAssignment.h"
#include "mosaic/Font.h"
#include "mosaic/GridLayout.h"
#include "mosaic/LabelRenderer.h"
#include "chess/ChessLabelRenderer.h"
#include "chess/ChessOverlay.h"
#include "chess/Engine.h"
#include "chess/Game.h"
#include "chess/GameState.h"
#include "chess/PieceSprites.h"
#include "chess/Piece.h"
#include "chess/StockfishEngine.h"
#include "mosaic/MosaicComposer.h"
#include "mosaic/MosaicExporter.h"
#include "mosaic/Painter.h"
#include "mosaic/Prompt.h"
#include "mosaic/SDLRenderer.h"
#include "mosaic/Subprocess.h"
#include "mosaic/VideoSource.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void writePng(std::uint8_t const* pixels, std::size_t bytes,
              int w, int h, std::string const& path) {
  std::string size = std::to_string(w) + "x" + std::to_string(h);
  mosaic::Subprocess proc({
      "ffmpeg", "-y", "-v", "error",
      "-f", "rawvideo", "-pix_fmt", "rgb24",
      "-s", size,
      "-i", "-",
      "-frames:v", "1",
      path,
  }, {.captureStdout = false, .captureStdin = true});
  proc.write(pixels, bytes);
  proc.closeStdin();
  int rc = proc.wait();
  if (rc != 0) throw std::runtime_error("ffmpeg PNG dump failed for " + path);
  std::cout << "  wrote " << path << " (" << size << ")\n";
}

void runStep1Demo(mosaic::VideoSource const& src) {
  std::cout << "[step 1] loaded " << src.path() << ": "
            << src.frameCount() << " frames @ " << src.nativeFps()
            << " fps, " << src.durationSec() << "s\n";
  for (double t : {0.0, 4500.0, 9000.0}) {
    auto const& f = src.frameAt(t);
    std::string name = "frame_t" + std::to_string(static_cast<int>(t)) + ".png";
    writePng(f.data(), f.size(), src.width(), src.height(), name);
  }
}

void runStep2Demo() {
  std::cout << "[step 2] empty labeled 4x5 grid\n";
  mosaic::GridLayout layout(/*rows=*/5, /*cols=*/4, /*cellSize=*/48, /*margin=*/20,
                            mosaic::LabelStyle::Margin);
  int const w = layout.canvasWidth();
  int const h = layout.canvasHeight();

  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 3);
  mosaic::Painter painter(pixels.data(), w, h);

  painter.fill(mosaic::RGB{255, 255, 255});

  mosaic::RGB const light{200, 200, 210};
  mosaic::RGB const dark { 90,  90, 105};
  for (int r = 0; r < layout.rows(); ++r) {
    for (int c = 0; c < layout.cols(); ++c) {
      painter.fillRect(layout.cellRect(r, c), ((r + c) & 1) ? dark : light);
    }
  }

  mosaic::MarginLabelRenderer labels(mosaic::RGB{0, 0, 0}, /*scale=*/2);
  labels.drawLabels(painter, layout);

  writePng(pixels.data(), pixels.size(), w, h, "step2_grid.png");
}

void printAssignment(char const* label, mosaic::CellAssignment const& a) {
  std::cout << "  " << label << ":\n";
  std::cout << "          ";
  for (int c = 0; c < a.cols(); ++c) {
    std::cout << " " << std::setw(2) << (c + 1);
  }
  std::cout << "\n";
  for (int r = 0; r < a.rows(); ++r) {
    std::cout << "       " << static_cast<char>('A' + r) << " ";
    for (int c = 0; c < a.cols(); ++c) {
      int idx = a.sourceIndexFor(r, c);
      if (idx < 0) std::cout << "  -";
      else         std::cout << " " << std::setw(2) << idx;
    }
    std::cout << "\n";
  }
}

void runStep3Demo(mosaic::VideoSource const& src) {
  std::cout << "[step 3] CellAssignment strategies (5 rows x 4 cols, 4 sources)\n";
  int const rows = 5;
  int const cols = 4;
  std::vector<mosaic::VideoSource const*> sources(4, &src);

  mosaic::RepeatTile rt(rows, cols, sources);
  printAssignment("RepeatTile", rt);

  mosaic::RandomFill rf(rows, cols, sources, /*seed=*/42, /*allowRepeats=*/true);
  printAssignment("RandomFill(seed=42, repeats)", rf);

  std::vector<int> placement = {
      0, -1, -1,  3,
     -1,  1, -1, -1,
     -1, -1,  2, -1,
      3, -1, -1,  0,
     -1,  1,  2, -1,
  };
  mosaic::ManualPlacement mp(rows, cols, sources, std::move(placement));
  printAssignment("ManualPlacement", mp);
}

void runStep4Demo(std::string const& clipPath) {
  std::cout << "[step 4] composite at t=2000ms (4 cols x 5 rows)\n";
  int const cellSize = 48;
  int const rows = 5;
  int const cols = 4;

  std::vector<mosaic::VideoSource> sources;
  sources.reserve(1);
  sources.emplace_back(clipPath, cellSize, cellSize);

  std::vector<mosaic::VideoSource const*> ptrs;
  ptrs.reserve(sources.size());
  for (auto const& s : sources) ptrs.push_back(&s);

  mosaic::GridLayout layout(rows, cols, cellSize, /*margin=*/20, mosaic::LabelStyle::Margin);
  auto assignment = std::make_unique<mosaic::RepeatTile>(rows, cols, std::move(ptrs));
  auto labels = std::make_unique<mosaic::MarginLabelRenderer>(mosaic::RGB{0, 0, 0}, /*scale=*/2);

  mosaic::MosaicComposer composer(std::move(layout), std::move(sources),
                                  std::move(assignment), std::move(labels));

  auto canvas = composer.composite(2000.0);
  writePng(canvas.data(), canvas.size(),
           composer.layout().canvasWidth(),
           composer.layout().canvasHeight(),
           "step4_composite.png");
}

// Grid sized so every source appears once: cols = ceil(sqrt(n)), rows so
// rows*cols >= n; sources dealt out evenly then shuffled across cells.
mosaic::MosaicComposer buildComposerFromPaths(std::vector<std::string> const& paths,
                                              int cellSize) {
  if (paths.empty()) throw std::runtime_error("no clips provided");
  std::vector<mosaic::VideoSource> sources;
  sources.reserve(paths.size());
  for (auto const& p : paths) {
    try {
      sources.emplace_back(p, cellSize, cellSize);
    } catch (std::exception const& e) {
      std::cerr << "  ! skipped " << p << ": " << e.what() << "\n";
    }
  }
  if (sources.empty()) throw std::runtime_error("no usable clips");

  int const n = static_cast<int>(sources.size());
  int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
  if (cols < 1) cols = 1;
  int const rows = (n + cols - 1) / cols;

  std::vector<mosaic::VideoSource const*> ptrs;
  ptrs.reserve(sources.size());
  for (auto const& s : sources) ptrs.push_back(&s);

  int const totalCells = rows * cols;
  std::vector<int> placement(static_cast<std::size_t>(totalCells));
  for (int i = 0; i < totalCells; ++i) placement[static_cast<std::size_t>(i)] = i % n;
  std::mt19937_64 rng(42);
  std::shuffle(placement.begin(), placement.end(), rng);

  mosaic::GridLayout layout(rows, cols, cellSize, /*margin=*/24, mosaic::LabelStyle::Margin);
  auto assignment = std::make_unique<mosaic::ManualPlacement>(rows, cols, std::move(ptrs),
                                                              std::move(placement));
  auto labels = std::make_unique<mosaic::MarginLabelRenderer>(mosaic::RGB{0, 0, 0}, /*scale=*/2);
  return mosaic::MosaicComposer(std::move(layout), std::move(sources),
                                std::move(assignment), std::move(labels));
}

void runPreview(std::vector<std::string> const& clips) {
  std::cout << "[preview] " << clips.size() << " clip(s)\n";
  mosaic::MosaicComposer composer = buildComposerFromPaths(clips, /*cellSize=*/90);
  mosaic::SDLRenderer renderer(std::move(composer), /*targetFps=*/30, "mosaic preview");
  renderer.run();
}

void runChessShow(std::string const& fenOrEmpty) {
  chess::GameState state = fenOrEmpty.empty()
      ? chess::GameState{}
      : chess::GameState::fromFen(fenOrEmpty);
  std::cout << "[chess] position\n" << state.asciiDump();
  std::cout << "  legal moves: " << state.legalMoves().size()
            << ", outcome: " << state.outcomeReason() << "\n";
}

void runChessMoves(std::string const& fenOrEmpty) {
  chess::GameState state = fenOrEmpty.empty()
      ? chess::GameState{}
      : chess::GameState::fromFen(fenOrEmpty);
  auto moves = state.legalMoves();
  std::cout << "[chess] " << moves.size() << " legal moves "
            << (state.sideToMove() == chess::Color::White ? "(white)" : "(black)")
            << ":\n ";
  int col = 0;
  for (auto const& m : moves) {
    std::cout << " " << m.toUci();
    if (++col % 10 == 0) std::cout << "\n ";
  }
  if (col % 10 != 0) std::cout << "\n";
}

void runChessPlay(std::vector<std::string> const& uciMoves) {
  chess::GameState state;
  std::cout << "[chess] playing " << uciMoves.size() << " move(s) from start\n";
  for (std::size_t i = 0; i < uciMoves.size(); ++i) {
    auto parsed = chess::Move::fromUci(uciMoves[i]);
    if (!parsed) throw std::runtime_error("bad UCI: " + uciMoves[i]);
    state.makeMove(*parsed);
    std::cout << "  " << (i + 1) << ". " << uciMoves[i]
              << (state.isCheck() ? " +" : "") << "\n";
  }
  std::cout << "\n" << state.asciiDump();
  std::cout << "  outcome: " << state.outcomeReason() << "\n";
}

void runChessScholarsMate() {
  runChessPlay({"e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g8f6", "h5f7"});
}

void runSelfPlay(chess::Engine& white, chess::Engine& black,
                 int timeLimitMs, int plyCap) {
  chess::GameState state;
  for (int ply = 0; ply < plyCap; ++ply) {
    if (state.outcome() != chess::GameState::Outcome::Ongoing) break;
    chess::Engine& mover =
        (state.sideToMove() == chess::Color::White) ? white : black;
    chess::Move m = mover.bestMove(state, timeLimitMs);
    state.makeMove(m);
    int const moveNo = state.fullmoveNumber() - (state.sideToMove() == chess::Color::White ? 1 : 0);
    char who = (state.sideToMove() == chess::Color::White) ? 'b' : 'w';
    std::cout << "  " << moveNo << who << ". " << m.toUci()
              << (state.isCheck() ? " +" : "") << "\n";
  }
  std::cout << "\n" << state.asciiDump();
  std::cout << "  outcome: " << state.outcomeReason() << "\n";
}

void runChessVsRandom(std::uint64_t seed) {
  std::cout << "[chess] RandomEngine self-play (seed=" << seed << ")\n";
  chess::RandomEngine white(seed);
  chess::RandomEngine black(seed ^ 0x9e3779b97f4a7c15ULL);
  runSelfPlay(white, black, /*timeLimitMs=*/0, /*plyCap=*/400);
}

void runChessSpritesDemo(int cellSize) {
  std::string const piecesDir = mosaic::assetRoot() + "/pieces";
  std::cout << "[chess sprites] cellSize=" << cellSize
            << ", loading " << piecesDir << "/*.png\n";
  chess::PieceSprites sprites(piecesDir, cellSize);

  mosaic::GridLayout layout(/*rows=*/8, /*cols=*/8, cellSize, /*margin=*/0,
                            mosaic::LabelStyle::None);
  int const w = layout.canvasWidth();
  int const h = layout.canvasHeight();
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 3);
  mosaic::Painter painter(pixels.data(), w, h);

  mosaic::RGB const lightSq{222, 213, 187};
  mosaic::RGB const darkSq { 95,  82,  68};
  for (int r = 0; r < 8; ++r) {
    for (int c = 0; c < 8; ++c) {
      // Light squares: where (file + rank) is odd. GridLayout row 0 = top
      // = rank 7, so rank = 7 - r.
      int const rank = 7 - r;
      bool const isLight = ((c + rank) & 1) != 0;
      painter.fillRect(layout.cellRect(r, c), isLight ? lightSq : darkSq);
    }
  }

  chess::GameState state;
  for (int rank = 0; rank < 8; ++rank) {
    for (int file = 0; file < 8; ++file) {
      auto const* p = state.board().at(chess::Position{file, rank});
      if (!p) continue;
      auto const& rgba = sprites.spriteFor(p->color(), p->type());
      // GridLayout row 0 is the top; chess rank 7 is the top -> row = 7 - rank.
      mosaic::Rect cell = layout.cellRect(7 - rank, file);
      painter.blitRgba(cell, rgba.data(), cellSize, cellSize);
    }
  }

  writePng(pixels.data(), pixels.size(), w, h, "step7e_sprites.png");
}

void runChessOverlayDemo(std::string const& clipPath, int cellSize) {
  std::cout << "[chess overlay] clip=" << clipPath << ", cellSize=" << cellSize << "\n";

  std::vector<mosaic::VideoSource> sources;
  sources.reserve(1);
  sources.emplace_back(clipPath, cellSize, cellSize);
  std::vector<mosaic::VideoSource const*> ptrs;
  ptrs.reserve(sources.size());
  for (auto const& s : sources) ptrs.push_back(&s);

  mosaic::GridLayout layout(/*rows=*/8, /*cols=*/8, cellSize, /*margin=*/24,
                            mosaic::LabelStyle::Margin);
  auto assignment = std::make_unique<mosaic::RepeatTile>(8, 8, std::move(ptrs));
  auto labels = std::make_unique<chess::ChessLabelRenderer>(mosaic::RGB{20, 20, 20},
                                                            /*scale=*/2);
  mosaic::MosaicComposer composer(std::move(layout), std::move(sources),
                                  std::move(assignment), std::move(labels));

  chess::PieceSprites sprites(mosaic::assetRoot() + "/pieces", cellSize);
  chess::ChessOverlay overlay(composer.layout(), std::move(sprites));
  chess::GameState state;

  mosaic::Frame canvas = composer.composite(/*timeMs=*/0.0);
  int const w = composer.layout().canvasWidth();
  int const h = composer.layout().canvasHeight();
  mosaic::Painter painter(canvas.data(), w, h);
  overlay.draw(painter, state.board());

  writePng(canvas.data(), canvas.size(), w, h, "step7f_overlay.png");
}

std::unique_ptr<chess::Engine> makeEngine(std::string const& kind, int skill) {
  if (kind == "random") {
    return std::make_unique<chess::RandomEngine>(/*seed=*/42);
  }
  try {
    return std::make_unique<chess::StockfishEngine>(skill);
  } catch (std::exception const& e) {
    std::cerr << "[engine] stockfish unavailable (" << e.what()
              << "); falling back to RandomEngine\n";
    return std::make_unique<chess::RandomEngine>(/*seed=*/42);
  }
}

void runInteractiveGame(std::string const& clipPath, std::string const& opponent,
                        int skill, int squareSize) {
  std::cout << "[game] booting clip=" << clipPath
            << ", opponent=" << opponent
            << ", squareSize=" << squareSize << "\n";

  int const margin = 24;
  int const boardPx = 8 * squareSize;

  std::vector<mosaic::VideoSource> sources;
  sources.reserve(1);
  sources.emplace_back(clipPath, boardPx, boardPx);
  std::vector<mosaic::VideoSource const*> ptrs{&sources.front()};

  // 1x1 mosaic: the one cell is the whole board, the clip stretched into it.
  mosaic::GridLayout mosaicLayout(/*rows=*/1, /*cols=*/1, boardPx, margin,
                                  mosaic::LabelStyle::Margin);
  auto assignment = std::make_unique<mosaic::RepeatTile>(1, 1, std::move(ptrs));
  mosaic::MosaicComposer composer(std::move(mosaicLayout), std::move(sources),
                                  std::move(assignment), /*labels=*/nullptr);

  mosaic::GridLayout chessLayout(8, 8, squareSize, margin, mosaic::LabelStyle::Margin);
  chess::PieceSprites sprites(mosaic::assetRoot() + "/pieces", squareSize);
  chess::ChessOverlay overlay(chessLayout, std::move(sprites));

  auto engine = makeEngine(opponent, skill);

  chess::Game game(std::move(composer), std::move(overlay), std::move(engine),
                   /*humanColor=*/chess::Color::White,
                   /*engineThinkMs=*/500,
                   /*targetFps=*/30,
                   "gif-chess");
  game.run();
}

void runChessVsStockfish(int skillLevel) {
  std::cout << "[chess] Stockfish (skill=" << skillLevel
            << ") as white vs RandomEngine as black\n";
  chess::StockfishEngine white(skillLevel);
  if (!white.name().empty()) std::cout << "  engine: " << white.name() << "\n";
  chess::RandomEngine black(/*seed=*/42);
  runSelfPlay(white, black, /*timeLimitMs=*/100, /*plyCap=*/300);
}

void runExport(std::vector<std::string> const& clips, std::string const& outputPath) {
  std::cout << "[export] " << clips.size() << " clip(s) -> " << outputPath << "\n";
  mosaic::MosaicComposer composer = buildComposerFromPaths(clips, /*cellSize=*/100);
  mosaic::MosaicExporter exporter(std::move(composer), outputPath,
                                  /*fps=*/30, /*durationSec=*/10.0);
  exporter.run();
}

void runAllPngDemos(std::string const& clipPath) {
  mosaic::VideoSource src(clipPath, 120, 120);
  runStep1Demo(src);
  runStep2Demo();
  runStep3Demo(src);
  runStep4Demo(clipPath);
}

void printUsage(char const* argv0) {
  std::cerr << "usage:\n"
            << "  " << argv0 << " prompt                         interactive REPL: add files/URLs, search GIFs, preview, export, play chess\n"
            << "  " << argv0 << " browse                         SDL window: search Tenor/Giphy/Klipy, click to add, pick local files\n"
            << "  " << argv0 << "                                PNG demos for steps 1-4\n"
            << "  " << argv0 << " <clip.mp4>                     PNG demos with a custom clip\n"
            << "  " << argv0 << " preview [<clip>...]            live SDL preview of one or more clips (grid auto-sized)\n"
            << "  " << argv0 << " export [<out>] [<clip>...]     write 10s 30fps MP4 (defaults: out.mp4, bundled clip)\n"
            << "  " << argv0 << " chess   [<fen>]                print chess position (default: starting position)\n"
            << "  " << argv0 << " chess moves [<fen>]            list legal moves\n"
            << "  " << argv0 << " chess play <uci>...            play moves from the starting position\n"
            << "  " << argv0 << " chess scholarsmate             built-in 4-move checkmate demo\n"
            << "  " << argv0 << " chess vsrandom [<seed>]        self-play with RandomEngine\n"
            << "  " << argv0 << " chess vsstockfish [<skill>]    Stockfish vs RandomEngine (needs stockfish on PATH)\n"
            << "  " << argv0 << " chess sprites [<cellSize>]     render starting position with PNG piece sprites\n"
            << "  " << argv0 << " chess overlay [<cellSize>]     render starting position over IShowSpeed mosaic\n"
            << "  " << argv0 << " chess game [<video.mp4>] [<opponent>] [<skill>] [<squareSize>]\n"
            << "                                     interactive game - video (if given) is stretched over the board;\n"
            << "                                     opponent {stockfish, random}, default stockfish\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGPIPE, SIG_IGN);

  std::string const defaultClip = "ishowspeed-speed.mp4";
  std::string subcommand;
  std::string clipPath = defaultClip;
  std::string outputPath = "out.mp4";
  std::string fenArg;
  std::vector<std::string> extraArgs;

  if (argc >= 2) {
    std::string arg1 = argv[1];
    if (arg1 == "-h" || arg1 == "--help") {
      printUsage(argv[0]);
      return 0;
    }
    if (arg1 == "prompt") {
      subcommand = "prompt";
    } else if (arg1 == "browse") {
      subcommand = "browse";
    } else if (arg1 == "preview") {
      subcommand = "preview";
      for (int i = 2; i < argc; ++i) extraArgs.push_back(argv[i]);
    } else if (arg1 == "export") {
      subcommand = "export";
      if (argc >= 3) outputPath = argv[2];
      for (int i = 3; i < argc; ++i) extraArgs.push_back(argv[i]);
    } else if (arg1 == "chess") {
      subcommand = "chess";
      if (argc >= 3) fenArg = argv[2];
      for (int i = 3; i < argc; ++i) extraArgs.push_back(argv[i]);
    } else {
      clipPath = arg1;
    }
  }

  try {
    if (subcommand == "prompt") {
      mosaic::Prompt prompt;
      prompt.run();
    } else if (subcommand == "browse") {
      mosaic::Prompt prompt;
      prompt.openBrowser();
    } else if (subcommand == "preview") {
      runPreview(extraArgs.empty() ? std::vector<std::string>{defaultClip} : extraArgs);
    } else if (subcommand == "export") {
      runExport(extraArgs.empty() ? std::vector<std::string>{defaultClip} : extraArgs, outputPath);
    } else if (subcommand == "chess") {
      if (fenArg == "moves") {
        runChessMoves(extraArgs.empty() ? "" : extraArgs.front());
      } else if (fenArg == "play") {
        runChessPlay(extraArgs);
      } else if (fenArg == "scholarsmate") {
        runChessScholarsMate();
      } else if (fenArg == "vsrandom") {
        std::uint64_t seed = 42;
        if (!extraArgs.empty()) seed = std::stoull(extraArgs.front());
        runChessVsRandom(seed);
      } else if (fenArg == "vsstockfish") {
        int skill = 5;
        if (!extraArgs.empty()) skill = std::stoi(extraArgs.front());
        runChessVsStockfish(skill);
      } else if (fenArg == "sprites") {
        int cellSize = 64;
        if (!extraArgs.empty()) cellSize = std::stoi(extraArgs.front());
        runChessSpritesDemo(cellSize);
      } else if (fenArg == "overlay") {
        int cellSize = 64;
        if (!extraArgs.empty()) cellSize = std::stoi(extraArgs.front());
        runChessOverlayDemo(clipPath, cellSize);
      } else if (fenArg == "game") {
        // chess game [<video.mp4>] [<opponent>] [<skill>] [<squareSize>]
        std::string clip = clipPath;
        std::vector<std::string> rest = extraArgs;
        if (!rest.empty() && std::filesystem::exists(rest[0])) {
          clip = rest[0];
          rest.erase(rest.begin());
        }
        std::string opp = "stockfish";
        int skill = 5;
        int squareSize = 80;
        if (rest.size() >= 1) opp = rest[0];
        if (rest.size() >= 2) skill = std::stoi(rest[1]);
        if (rest.size() >= 3) squareSize = std::stoi(rest[2]);
        runInteractiveGame(clip, opp, skill, squareSize);
      } else {
        runChessShow(fenArg);
      }
    } else {
      runAllPngDemos(clipPath);
    }
  } catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}

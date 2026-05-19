cmake_minimum_required(VERSION 3.20)
project(mosaic CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)

add_executable(mosaic
  src/main.cpp
  src/mosaic/Subprocess.cpp
  src/mosaic/VideoSource.cpp
  src/mosaic/Font.cpp
  src/mosaic/Painter.cpp
  src/mosaic/GridLayout.cpp
  src/mosaic/LabelRenderer.cpp
  src/mosaic/CellAssignment.cpp
  src/mosaic/MosaicComposer.cpp
  src/mosaic/SDLRenderer.cpp
  src/mosaic/SDLTypes.cpp
  src/mosaic/MosaicExporter.cpp
  src/mosaic/Json.cpp
  src/mosaic/Downloader.cpp
  src/mosaic/Library.cpp
  src/mosaic/SearchProvider.cpp
  src/mosaic/SearchGui.cpp
  src/mosaic/Prompt.cpp
  src/chess/Types.cpp
  src/chess/Piece.cpp
  src/chess/Board.cpp
  src/chess/GameState.cpp
  src/chess/Engine.cpp
  src/chess/StockfishEngine.cpp
  src/chess/PieceSprites.cpp
  src/chess/ChessLabelRenderer.cpp
  src/chess/ChessOverlay.cpp
  src/chess/Sounds.cpp
  src/chess/Game.cpp
)

target_include_directories(mosaic PRIVATE src)
target_link_libraries(mosaic PRIVATE SDL2::SDL2 Threads::Threads)

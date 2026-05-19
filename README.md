# gif-chess

A C++ tool that bakes N short video clips into a single gridded MP4, where
each cell plays its own clip on its own independent loop. Output is tuned to
feel like a GIF when embedded on Discord/Twitter/Slack: short, no audio, h264
main profile, yuv420p, `+faststart`.

On top of the standalone mosaic engine there's a **chess layer**:
an 8x8 board where you play against a (nerfable) Stockfish, with the mosaic
cells visible underneath the pieces.

This is an OOP-practice project. The mosaic engine and the chess layer are
deliberately separable: mosaic doesn't know chess exists; chess depends on
mosaic, not the reverse.

## Requirements

- Linux (Mac/Windows not supported)
- A C++20 compiler and CMake 3.20+.
- **SDL2** (live preview, the browser, and the interactive chess game).
- **ffmpeg** and **ffprobe** on `PATH`, invoked as subprocesses for decode
  and encode.
- **stockfish** on `PATH` only for the Stockfish chess modes. Everything
  else works without it. If stockfish is not installed, the engine will just play random moves instead.

## Build

```bash
cmake -B build
cmake --build build
```

This produces the binary at `build/mosaic`. It's not installed on `PATH`,
so run it by path:

```bash
./build/mosaic browse        # from the repo root
cd build && ./mosaic browse  # or from inside build/
```

Asset paths are resolved relative to the executable, so it works from any
working directory.

## Usage

In the table below, `mosaic` is shorthand for `./build/mosaic` (run from the
repo root).

```
mosaic prompt                      interactive REPL: add files/URLs, search GIFs, preview, export, play chess
mosaic browse                      SDL window: search Tenor/Giphy/Klipy, click to add, pick local files
mosaic                             PNG demos for build steps 1-4
mosaic <clip.mp4>                  PNG demos with a custom clip
mosaic preview [<clip>...]         live SDL preview of one or more clips (grid auto-sized)
mosaic export [<out>] [<clip>...]  write a 10s 30fps MP4 (defaults: out.mp4, bundled clip)

mosaic chess [<fen>]               print a chess position (default: starting position)
mosaic chess moves [<fen>]         list legal moves
mosaic chess play <uci>...         play moves from the starting position
mosaic chess scholarsmate          built-in 4-move checkmate demo
mosaic chess vsrandom [<seed>]     RandomEngine self-play
mosaic chess vsstockfish [<skill>] Stockfish vs RandomEngine
```

Online GIF search (`prompt` / `browse`) reads `TENOR_API_KEY` and
`GIPHY_API_KEY` from the environment. Each provider is enabled only if its
key is set.

## License

[ISC](LICENSE)

Bundled assets in assets/ are third-party under their own terms.

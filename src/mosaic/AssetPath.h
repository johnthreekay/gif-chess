#pragma once

#include <string>

namespace mosaic {

// Absolute path to the bundled assets directory, resolved independently of the
// current working directory (the binary normally lives in build/, so a
// cwd-relative "assets" only works when run from the repo root). Resolution
// order, first existing wins:
//   1. $GIF_CHESS_ASSETS, if set and non-empty
//   2. <exe dir>/assets, then <exe dir>/../assets
//   3. "assets" (cwd-relative; original behaviour, last resort)
// Callers append the subdir, e.g. assetRoot() + "/pieces".
std::string assetRoot();

}  // namespace mosaic

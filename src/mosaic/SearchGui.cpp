#include "mosaic/SearchGui.h"

#include "mosaic/Font.h"
#include "mosaic/Painter.h"
#include "mosaic/SDLTypes.h"
#include "mosaic/Subprocess.h"

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace mosaic {

namespace {

constexpr int kWinW = 760;
constexpr int kWinH = 720;
constexpr int kPad = 12;
constexpr int kTopH = 46;
constexpr int kStatusH = 26;
constexpr int kBtnH = 42;
constexpr int kCols = 4;
constexpr int kThumb = 150;
constexpr int kTitleH = 16;
constexpr int kFontScale = 2;

RGB constexpr kBg       {28, 30, 34};
RGB constexpr kPanel    {44, 46, 52};
RGB constexpr kPanelHi  {70, 90, 130};
RGB constexpr kText     {230, 230, 235};
RGB constexpr kTextDim  {150, 152, 160};
RGB constexpr kAdded    {90, 200, 120};
RGB constexpr kFailed   {200, 90, 90};
RGB constexpr kCursor   {230, 230, 235};

bool inside(int x, int y, Rect const& r) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void drawBorder(Painter& p, Rect const& r, RGB c, int th) {
  p.fillRect({r.x, r.y, r.w, th}, c);
  p.fillRect({r.x, r.y + r.h - th, r.w, th}, c);
  p.fillRect({r.x, r.y + th, th, r.h - 2 * th}, c);
  p.fillRect({r.x + r.w - th, r.y + th, th, r.h - 2 * th}, c);
}

// Painter::drawText centers on a point; this draws left-aligned at (x, centerY).
void drawTextLeft(Painter& p, std::string_view s, int x, int centerY, RGB c, int scale) {
  if (s.empty()) return;
  int const gw = BitmapFont::kGlyphWidth * scale;
  int const spacing = scale;
  int const n = static_cast<int>(s.size());
  int const textW = n * gw + (n - 1) * spacing;
  p.drawText(defaultFont(), s, x + textW / 2, centerY, c, scale);
}

std::string truncateToWidth(std::string s, int maxPx, int scale) {
  int const per = (BitmapFont::kGlyphWidth + 1) * scale;
  int const maxChars = std::max(1, maxPx / per);
  if (static_cast<int>(s.size()) <= maxChars) return s;
  if (maxChars <= 2) return s.substr(0, maxChars);
  return s.substr(0, static_cast<std::size_t>(maxChars) - 2) + "..";
}

std::string sanitizeForFilename(std::string s) {
  for (char& c : s) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) c = '_';
  }
  return s;
}

std::string basenameOf(std::string const& path) {
  auto slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string extOf(std::string const& url) {
  auto q = url.find('?');
  std::string u = (q == std::string::npos) ? url : url.substr(0, q);
  auto dot = u.find_last_of('.');
  auto slash = u.find_last_of('/');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return ".bin";
  return u.substr(dot);
}

// Decodes the first frame of `path` to a square RGB24 buffer, fit inside and
// padded with a dark background.
std::vector<std::uint8_t> decodeFirstFrameSquare(std::string const& path, int size) {
  std::string vf = "scale=" + std::to_string(size) + ":" + std::to_string(size) +
                   ":force_original_aspect_ratio=decrease,"
                   "pad=" + std::to_string(size) + ":" + std::to_string(size) +
                   ":(ow-iw)/2:(oh-ih)/2:color=0x1c1e22";
  Subprocess proc({
      "ffmpeg", "-v", "error",
      "-i", path,
      "-vf", vf,
      "-frames:v", "1",
      "-pix_fmt", "rgb24",
      "-f", "rawvideo", "-",
  });
  std::size_t const want = static_cast<std::size_t>(size) * size * 3u;
  std::vector<std::uint8_t> out(want);
  std::size_t got = proc.read(out.data(), want);
  int rc = proc.wait();
  if (rc != 0 || got != want) {
    throw std::runtime_error("decodeFirstFrame: ffmpeg failed for " + path);
  }
  return out;
}

}  // namespace

SearchGui::SearchGui(Library& library,
                     std::vector<std::unique_ptr<SearchProvider>>& providers,
                     Downloader& downloader)
    : library_(library), providers_(providers), downloader_(downloader) {
  status_ = providers_.empty()
      ? "no search providers - set TENOR_API_KEY / GIPHY_API_KEY / KLIPY_APP_KEY"
      : "type a query and press Enter";
}

void SearchGui::doSearch() { goToPage(1); }

void SearchGui::goToPage(int page) {
  if (providers_.empty()) { status_ = "no search providers configured"; return; }
  if (query_.empty()) { status_ = "type something to search for"; return; }
  if (page < 1) page = 1;
  SearchProvider* p = providers_[activeProvider_ % providers_.size()].get();
  status_ = "searching " + p->name() + " p" + std::to_string(page) + " for \"" + query_ + "\"...";
  std::vector<SearchResult> hits;
  try {
    hits = p->search(query_, /*perPage=*/12, page);
  } catch (std::exception const& e) {
    status_ = std::string("search failed: ") + e.what();
    return;
  }
  if (hits.empty() && page > 1) {
    status_ = "no more results - still on page " + std::to_string(page_);
    return;  // keep current results_ and page_
  }
  page_ = page;
  results_.clear();
  for (auto& h : hits) {
    ResultTile t;
    t.result = std::move(h);
    results_.push_back(std::move(t));
  }
  status_ = results_.empty()
      ? ("no results for \"" + query_ + "\" on " + p->name())
      : (std::to_string(results_.size()) + " results, page " + std::to_string(page_) +
         " - click to add");
}

void SearchGui::loadOnePendingThumbnail() {
  for (auto& t : results_) {
    if (t.state != ResultTile::State::Pending) continue;
    try {
      std::string fname = sanitizeForFilename(t.result.providerId + "-" + t.result.id) +
                          extOf(t.result.downloadUrl);
      t.localPath = downloader_.download(t.result.downloadUrl, fname);
      t.thumb = decodeFirstFrameSquare(t.localPath, kThumb);
      t.state = ResultTile::State::Ready;
    } catch (...) {
      t.state = ResultTile::State::Failed;
    }
    return;  // one per call
  }
}

void SearchGui::addResult(std::size_t idx) {
  if (idx >= results_.size()) return;
  ResultTile& t = results_[idx];
  if (t.added || t.localPath.empty()) return;
  std::string label = "[" + t.result.providerId + "] " +
                      (t.result.title.empty() ? t.result.id : t.result.title);
  library_.add(label, t.localPath);
  t.added = true;
  status_ = "added \"" + label + "\"  (library: " + std::to_string(library_.size()) + ")";
}

namespace {

std::string runZenity(std::vector<std::string> args) {
  Subprocess proc(std::move(args));
  std::string out;
  std::uint8_t buf[1024];
  while (std::size_t n = proc.read(buf, sizeof(buf))) out.append(reinterpret_cast<char*>(buf), n);
  int rc = proc.wait();
  if (rc != 0) return {};  // cancelled / error
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return out;
}

}  // namespace

void SearchGui::addLocalFileViaDialog() {
  // --multiple lets the user shift/ctrl-click several files; we ask zenity to
  // separate them with 0x1f (unit separator) since that never occurs in paths.
  std::string out = runZenity({
      "zenity", "--file-selection", "--multiple", "--separator=\x1f",
      "--title=Add clips to the mosaic",
      "--file-filter=Media | *.mp4 *.gif *.webm *.mov *.mkv *.png *.jpg *.jpeg",
      "--file-filter=All files | *",
  });
  if (out.empty()) { status_ = "file selection cancelled"; return; }

  int added = 0, skipped = 0;
  for (std::size_t start = 0; start <= out.size();) {
    std::size_t sep = out.find('\x1f', start);
    std::string path = (sep == std::string::npos) ? out.substr(start)
                                                  : out.substr(start, sep - start);
    start = (sep == std::string::npos) ? out.size() + 1 : sep + 1;
    if (path.empty()) continue;
    if (std::filesystem::exists(path)) { library_.add(basenameOf(path), path); ++added; }
    else ++skipped;
  }
  status_ = "added " + std::to_string(added) + " file(s)" +
            (skipped ? (", " + std::to_string(skipped) + " skipped") : "") +
            "  (library: " + std::to_string(library_.size()) + ")";
}

std::string SearchGui::pickSavePathViaDialog() {
  return runZenity({
      "zenity", "--file-selection", "--save", "--confirm-overwrite",
      "--title=Export mosaic MP4", "--filename=out.mp4",
      "--file-filter=MP4 | *.mp4", "--file-filter=All files | *",
  });
}

SearchGui::Result SearchGui::run() {
  SDLInit sdl;
  WindowPtr window{SDL_CreateWindow("gif-chess - find clips",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, kWinW, kWinH, SDL_WINDOW_SHOWN)};
  if (!window) throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
  RendererPtr renderer{SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED)};
  if (!renderer) throw std::runtime_error(std::string("SDL_CreateRenderer: ") + SDL_GetError());
  TexturePtr texture{SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGB24,
      SDL_TEXTUREACCESS_STREAMING, kWinW, kWinH)};
  if (!texture) throw std::runtime_error(std::string("SDL_CreateTexture: ") + SDL_GetError());

  SDL_StartTextInput();
  std::vector<std::uint8_t> canvas(static_cast<std::size_t>(kWinW) * kWinH * 3);

  // Static hit regions.
  Rect const providerBtn{kPad, 7, 110, 32};
  Rect const searchBox{kPad + 122, 7, kWinW - (kPad + 122) - kPad - 110 - kPad, 32};
  Rect const addFileBtn{kWinW - kPad - 110, 7, 110, 32};
  Rect const nextBtn{kWinW - kPad - 64, kTopH + 3, 64, kStatusH - 6};
  Rect const prevBtn{kWinW - kPad - 64 - 6 - 64, kTopH + 3, 64, kStatusH - 6};
  int const gridX0 = (kWinW - (kCols * kThumb + (kCols - 1) * kPad)) / 2;
  int const gridY0 = kTopH + kStatusH;
  int const cellH = kThumb + kTitleH + kPad;
  int const btnY = kWinH - kBtnH;
  Rect const previewBtn{kPad,                       btnY + 5, 150, kBtnH - 10};
  Rect const exportBtn {kPad + 162,                 btnY + 5, 150, kBtnH - 10};
  Rect const chessBtn  {kPad + 324,                 btnY + 5, 150, kBtnH - 10};

  Result result = Result::None;
  bool running = true;
  bool blink = true;
  Uint32 lastBlink = SDL_GetTicks();

  while (running) {
    Uint32 const frameStart = SDL_GetTicks();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) { running = false; break; }
      if (ev.type == SDL_TEXTINPUT) {
        for (char const* c = ev.text.text; *c; ++c) {
          if (static_cast<unsigned char>(*c) >= 0x20) query_ += *c;
        }
      } else if (ev.type == SDL_KEYDOWN) {
        switch (ev.key.keysym.sym) {
          case SDLK_ESCAPE: running = false; break;
          case SDLK_RETURN: case SDLK_KP_ENTER: doSearch(); break;
          case SDLK_BACKSPACE: if (!query_.empty()) query_.pop_back(); break;
          case SDLK_TAB:
            if (!providers_.empty()) activeProvider_ = (activeProvider_ + 1) % providers_.size();
            break;
          default: break;
        }
      } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        int mx = ev.button.x, my = ev.button.y;
        if (inside(mx, my, providerBtn)) {
          if (!providers_.empty()) activeProvider_ = (activeProvider_ + 1) % providers_.size();
        } else if (inside(mx, my, addFileBtn)) {
          addLocalFileViaDialog();
        } else if (inside(mx, my, prevBtn)) {
          goToPage(page_ - 1);
        } else if (inside(mx, my, nextBtn)) {
          goToPage(page_ + 1);
        } else if (inside(mx, my, previewBtn)) {
          if (!library_.empty()) { result = Result::Preview; running = false; }
          else status_ = "library is empty";
        } else if (inside(mx, my, exportBtn)) {
          if (library_.empty()) {
            status_ = "library is empty";
          } else {
            std::string chosen = pickSavePathViaDialog();
            if (chosen.empty()) {
              status_ = "export cancelled";
            } else {
              exportPath_ = chosen;
              result = Result::Export;
              running = false;
            }
          }
        } else if (inside(mx, my, chessBtn)) {
          if (!library_.empty()) { result = Result::Chess; running = false; }
          else status_ = "library is empty";
        } else {
          for (std::size_t i = 0; i < results_.size(); ++i) {
            int col = static_cast<int>(i) % kCols;
            int row = static_cast<int>(i) / kCols;
            Rect tile{gridX0 + col * (kThumb + kPad), gridY0 + row * cellH, kThumb, kThumb};
            if (inside(mx, my, tile)) { addResult(i); break; }
          }
        }
      }
    }
    if (!running) break;

    loadOnePendingThumbnail();

    if (SDL_GetTicks() - lastBlink > 500) { blink = !blink; lastBlink = SDL_GetTicks(); }

    // ---- render ----
    Painter p(canvas.data(), kWinW, kWinH);
    p.fill(kBg);

    // Provider button
    bool haveProv = !providers_.empty();
    p.fillRect(providerBtn, haveProv ? kPanelHi : kPanel);
    std::string provName = haveProv ? providers_[activeProvider_ % providers_.size()]->name()
                                    : "(no provider)";
    p.drawText(defaultFont(), provName, providerBtn.x + providerBtn.w / 2,
               providerBtn.y + providerBtn.h / 2, kText, kFontScale);

    // Search box
    p.fillRect(searchBox, kPanel);
    {
      std::string shown = query_;
      // keep the tail visible if too long
      int per = (BitmapFont::kGlyphWidth + 1) * kFontScale;
      int maxChars = (searchBox.w - 12) / per;
      if (static_cast<int>(shown.size()) > maxChars) shown = shown.substr(shown.size() - maxChars);
      int tx = searchBox.x + 6;
      drawTextLeft(p, shown.empty() ? std::string("search...") : shown,
                   tx, searchBox.y + searchBox.h / 2,
                   shown.empty() ? kTextDim : kText, kFontScale);
      if (blink && !shown.empty()) {
        int cw = static_cast<int>(shown.size()) * per;
        p.fillRect({tx + cw, searchBox.y + 6, 2, searchBox.h - 12}, kCursor);
      }
    }

    // Add file button
    p.fillRect(addFileBtn, kPanel);
    p.drawText(defaultFont(), "ADD FILE", addFileBtn.x + addFileBtn.w / 2,
               addFileBtn.y + addFileBtn.h / 2, kText, kFontScale);

    // Status line + pagination buttons
    drawTextLeft(p, status_, kPad, kTopH + kStatusH / 2, kTextDim, kFontScale);
    bool const canPage = !results_.empty() || page_ > 1;
    p.fillRect(prevBtn, (canPage && page_ > 1) ? kPanelHi : kPanel);
    p.drawText(defaultFont(), "<PREV", prevBtn.x + prevBtn.w / 2, prevBtn.y + prevBtn.h / 2,
               (canPage && page_ > 1) ? kText : kTextDim, kFontScale);
    p.fillRect(nextBtn, canPage ? kPanelHi : kPanel);
    p.drawText(defaultFont(), "NEXT>", nextBtn.x + nextBtn.w / 2, nextBtn.y + nextBtn.h / 2,
               canPage ? kText : kTextDim, kFontScale);

    // Results grid
    for (std::size_t i = 0; i < results_.size(); ++i) {
      int col = static_cast<int>(i) % kCols;
      int row = static_cast<int>(i) / kCols;
      int x = gridX0 + col * (kThumb + kPad);
      int y = gridY0 + row * cellH;
      if (y + kThumb > btnY) break;  // overflow guard
      Rect tile{x, y, kThumb, kThumb};
      ResultTile const& t = results_[i];
      if (t.state == ResultTile::State::Ready && t.thumb.size() == static_cast<std::size_t>(kThumb) * kThumb * 3) {
        p.blit(tile, t.thumb.data(), kThumb, kThumb);
      } else {
        p.fillRect(tile, kPanel);
        std::string msg = (t.state == ResultTile::State::Failed) ? "x" : "...";
        p.drawText(defaultFont(), msg, tile.x + tile.w / 2, tile.y + tile.h / 2,
                   (t.state == ResultTile::State::Failed) ? kFailed : kTextDim, kFontScale);
      }
      if (t.added) drawBorder(p, tile, kAdded, 4);
      std::string title = t.result.title.empty() ? t.result.id : t.result.title;
      drawTextLeft(p, truncateToWidth(title, kThumb, kFontScale), x, y + kThumb + kTitleH / 2,
                   t.added ? kAdded : kTextDim, kFontScale);
    }

    // Bottom bar
    p.fillRect({0, btnY, kWinW, kBtnH}, kPanel);
    for (auto const& [r, label] : std::initializer_list<std::pair<Rect, char const*>>{
             {previewBtn, "PREVIEW"}, {exportBtn, "EXPORT MP4"}, {chessBtn, "PLAY CHESS"}}) {
      p.fillRect(r, library_.empty() ? kPanel : kPanelHi);
      p.drawText(defaultFont(), label, r.x + r.w / 2, r.y + r.h / 2,
                 library_.empty() ? kTextDim : kText, kFontScale);
    }
    drawTextLeft(p, "lib " + std::to_string(library_.size()) + "  |  Esc done",
                 chessBtn.x + chessBtn.w + kPad, btnY + kBtnH / 2, kTextDim, kFontScale);

    SDL_UpdateTexture(texture.get(), nullptr, canvas.data(), kWinW * 3);
    SDL_RenderClear(renderer.get());
    SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);
    SDL_RenderPresent(renderer.get());

    Uint32 const elapsed = SDL_GetTicks() - frameStart;
    if (elapsed < 33) SDL_Delay(33 - elapsed);
  }

  SDL_StopTextInput();
  return result;
}

}  // namespace mosaic

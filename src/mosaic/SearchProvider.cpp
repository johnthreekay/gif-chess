#include "mosaic/SearchProvider.h"

#include "mosaic/Json.h"

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace mosaic {

namespace {

std::string envOrEmpty(char const* name) {
  char const* v = std::getenv(name);
  return v ? std::string(v) : std::string();
}

// Picks the smallest mp4 url available under Tenor's media_formats; falls
// back to gif if no mp4 form exists.
std::string pickTenorUrl(json::Value const& mediaFormats) {
  for (auto const* key : {"tinymp4", "nanomp4", "mp4", "loopedmp4"}) {
    if (auto const* v = mediaFormats.find(key)) {
      if (auto const* u = v->find("url"); u && u->isString()) return u->asString();
    }
  }
  for (auto const* key : {"tinygif", "nanogif", "gif"}) {
    if (auto const* v = mediaFormats.find(key)) {
      if (auto const* u = v->find("url"); u && u->isString()) return u->asString();
    }
  }
  return {};
}

std::string pickGiphyUrl(json::Value const& images) {
  for (auto const* key : {"fixed_height_small", "fixed_width_small", "preview", "original"}) {
    if (auto const* v = images.find(key)) {
      // Giphy stores mp4 url under "mp4" within the rendition object.
      if (auto const* m = v->find("mp4"); m && m->isString() && !m->asString().empty()) {
        return m->asString();
      }
    }
  }
  for (auto const* key : {"fixed_height_small", "fixed_width_small", "preview_gif", "original"}) {
    if (auto const* v = images.find(key)) {
      if (auto const* u = v->find("url"); u && u->isString()) return u->asString();
    }
  }
  return {};
}

}  // namespace

// ---------- Tenor ----------

TenorProvider::TenorProvider(std::string apiKey, Downloader& downloader)
    : apiKey_(std::move(apiKey)), downloader_(downloader) {
  if (apiKey_.empty()) throw std::invalid_argument("TenorProvider: api key required");
}

std::vector<SearchResult> TenorProvider::search(std::string_view query, int perPage, int page) {
  // Tenor v2 paginates via a `pos` cursor, not page numbers; we don't carry
  // the cursor across calls, so anything past the first page is unsupported.
  if (page > 1) return {};
  std::string url = "https://tenor.googleapis.com/v2/search?q=" + urlEncode(query) +
                    "&key=" + urlEncode(apiKey_) +
                    "&limit=" + std::to_string(perPage) +
                    "&media_filter=tinymp4,mp4,tinygif,gif" +
                    "&contentfilter=medium";
  std::string body = downloader_.fetch(url);
  json::Value root = json::parse(body);

  std::vector<SearchResult> out;
  auto const* results = root.find("results");
  if (!results || !results->isArray()) return out;
  for (auto const& item : results->asArray()) {
    SearchResult sr;
    sr.providerId = "tenor";
    if (auto const* id = item.find("id"); id && id->isString()) sr.id = id->asString();
    if (auto const* t  = item.find("content_description"); t && t->isString()) sr.title = t->asString();
    if (auto const* mf = item.find("media_formats")) sr.downloadUrl = pickTenorUrl(*mf);
    if (!sr.downloadUrl.empty()) out.push_back(std::move(sr));
  }
  return out;
}

// ---------- Giphy ----------

GiphyProvider::GiphyProvider(std::string apiKey, Downloader& downloader)
    : apiKey_(std::move(apiKey)), downloader_(downloader) {
  if (apiKey_.empty()) throw std::invalid_argument("GiphyProvider: api key required");
}

std::vector<SearchResult> GiphyProvider::search(std::string_view query, int perPage, int page) {
  int const offset = (page > 1) ? (page - 1) * perPage : 0;
  std::string url = "https://api.giphy.com/v1/gifs/search?api_key=" + urlEncode(apiKey_) +
                    "&q=" + urlEncode(query) +
                    "&limit=" + std::to_string(perPage) +
                    "&offset=" + std::to_string(offset) +
                    "&rating=pg-13";
  std::string body = downloader_.fetch(url);
  json::Value root = json::parse(body);

  std::vector<SearchResult> out;
  auto const* data = root.find("data");
  if (!data || !data->isArray()) return out;
  for (auto const& item : data->asArray()) {
    SearchResult sr;
    sr.providerId = "giphy";
    if (auto const* id = item.find("id"); id && id->isString()) sr.id = id->asString();
    if (auto const* t  = item.find("title"); t && t->isString()) sr.title = t->asString();
    if (auto const* im = item.find("images")) sr.downloadUrl = pickGiphyUrl(*im);
    if (!sr.downloadUrl.empty()) out.push_back(std::move(sr));
  }
  return out;
}

// ---------- Klipy ----------

// Klipy lists per-size renditions under file.{hd,md,sm,xs}.{gif,webp,jpg,webm,url}.
// Each size also exposes a top-level "url" (typically mp4). Prefer smaller +
// video-shaped renditions for download speed.
namespace {

std::string pickKlipyUrl(json::Value const& file) {
  for (auto const* size : {"xs", "sm", "md", "hd"}) {
    auto const* s = file.find(size);
    if (!s) continue;
    if (auto const* u = s->find("url"); u && u->isString() && !u->asString().empty()) {
      return u->asString();
    }
    for (auto const* fmt : {"webm", "gif", "webp"}) {
      if (auto const* f = s->find(fmt)) {
        if (auto const* u = f->find("url"); u && u->isString() && !u->asString().empty()) {
          return u->asString();
        }
      }
    }
  }
  return {};
}

}  // namespace

KlipyProvider::KlipyProvider(std::string appKey, Downloader& downloader,
                             std::string customerId, std::string locale,
                             std::string contentFilter)
    : appKey_(std::move(appKey)),
      downloader_(downloader),
      customerId_(std::move(customerId)),
      locale_(std::move(locale)),
      contentFilter_(std::move(contentFilter)) {
  if (appKey_.empty()) throw std::invalid_argument("KlipyProvider: app key required");
}

std::vector<SearchResult> KlipyProvider::search(std::string_view query, int perPage, int page) {
  if (page < 1) page = 1;
  std::string url = "https://api.klipy.com/v1/" + urlEncode(appKey_) +
                    "/gifs/search?q=" + urlEncode(query) +
                    "&page=" + std::to_string(page) +
                    "&per_page=" + std::to_string(perPage) +
                    "&customer_id=" + urlEncode(customerId_) +
                    "&locale=" + urlEncode(locale_) +
                    "&content_filter=" + urlEncode(contentFilter_);
  std::string body = downloader_.fetch(url);
  json::Value root = json::parse(body);

  std::vector<SearchResult> out;
  // Response shape: { "data": { "data": [ {slug, title, file: {...}}, ... ] } }
  auto const* outer = root.find("data");
  if (!outer) return out;
  auto const* arr = outer->find("data");
  if (!arr || !arr->isArray()) return out;
  for (auto const& item : arr->asArray()) {
    SearchResult sr;
    sr.providerId = "klipy";
    if (auto const* s = item.find("slug"); s && s->isString()) sr.id = s->asString();
    if (sr.id.empty()) {
      if (auto const* i = item.find("id"); i && i->isString()) sr.id = i->asString();
    }
    if (auto const* t = item.find("title"); t && t->isString()) sr.title = t->asString();
    if (auto const* f = item.find("file"); f) sr.downloadUrl = pickKlipyUrl(*f);
    if (!sr.downloadUrl.empty()) out.push_back(std::move(sr));
  }
  return out;
}

// ---------- Factory ----------

std::vector<std::unique_ptr<SearchProvider>> makeDefaultProviders(Downloader& downloader) {
  std::vector<std::unique_ptr<SearchProvider>> out;
  if (std::string k = envOrEmpty("TENOR_API_KEY"); !k.empty()) {
    out.push_back(std::make_unique<TenorProvider>(k, downloader));
  }
  if (std::string k = envOrEmpty("GIPHY_API_KEY"); !k.empty()) {
    out.push_back(std::make_unique<GiphyProvider>(k, downloader));
  }
  if (std::string k = envOrEmpty("KLIPY_APP_KEY"); !k.empty()) {
    std::string customer = envOrEmpty("KLIPY_CUSTOMER_ID");
    if (customer.empty()) customer = "gif-chess";
    out.push_back(std::make_unique<KlipyProvider>(k, downloader, customer));
  }
  return out;
}

}  // namespace mosaic

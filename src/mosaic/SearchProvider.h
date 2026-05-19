#pragma once

#include "mosaic/Downloader.h"

#include <memory>
#include <string>
#include <vector>

namespace mosaic {

struct SearchResult {
  std::string providerId;   // "tenor", "giphy", "klipy"
  std::string id;           // provider-specific id
  std::string title;        // short description
  std::string downloadUrl;  // direct URL to a small mp4 (preferred) or gif
};

class SearchProvider {
public:
  virtual ~SearchProvider() = default;
  virtual std::string name() const = 0;
  // `page` is 1-based. Providers that can't do offset/page pagination
  // (currently Tenor, which is cursor-based) return an empty vector for
  // page > 1.
  virtual std::vector<SearchResult> search(std::string_view query, int perPage, int page) = 0;
};

class TenorProvider : public SearchProvider {
public:
  explicit TenorProvider(std::string apiKey, Downloader& downloader);
  std::string name() const override { return "tenor"; }
  std::vector<SearchResult> search(std::string_view query, int perPage, int page) override;

private:
  std::string apiKey_;
  Downloader& downloader_;
};

class GiphyProvider : public SearchProvider {
public:
  explicit GiphyProvider(std::string apiKey, Downloader& downloader);
  std::string name() const override { return "giphy"; }
  std::vector<SearchResult> search(std::string_view query, int perPage, int page) override;

private:
  std::string apiKey_;
  Downloader& downloader_;
};

// Klipy GIF search via api.klipy.com/v1/{app_key}/gifs/search.
class KlipyProvider : public SearchProvider {
public:
  // appKey: the per-app key issued by KLIPY (https://docs.klipy.com).
  // customerId: per-user identifier Klipy requires for analytics; "gif-chess"
  //   is fine for a single-user tool.
  // locale: ISO 3166 alpha-2 (e.g. "us"). contentFilter: off/low/medium/high.
  KlipyProvider(std::string appKey, Downloader& downloader,
                std::string customerId = "gif-chess",
                std::string locale = "us",
                std::string contentFilter = "medium");

  std::string name() const override { return "klipy"; }
  std::vector<SearchResult> search(std::string_view query, int perPage, int page) override;

private:
  std::string appKey_;
  Downloader& downloader_;
  std::string customerId_;
  std::string locale_;
  std::string contentFilter_;
};

// Reads provider API keys from the environment and instantiates any with a
// key set. TENOR_API_KEY -> TenorProvider; GIPHY_API_KEY -> GiphyProvider.
// Klipy is always included as the stub.
std::vector<std::unique_ptr<SearchProvider>> makeDefaultProviders(Downloader& downloader);

}  // namespace mosaic

#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "config.hpp"
#include "http.hpp"
#include "model.hpp"

namespace nes {

class CatalogService {
 public:
  CatalogService(Config& config, HttpClient& http, std::filesystem::path cache_path);

  void load();
  void refresh();
  std::vector<Game> games() const;
  std::size_t total() const;
  std::optional<Game> find(std::string_view id) const;
  std::vector<Game> search(std::string_view query,
                           std::string_view license,
                           std::string_view source,
                           std::string_view mapper,
                           std::string_view sort,
                           std::size_t limit,
                           std::size_t offset) const;

  void enrich(Game& game);
  bool add_source(const std::string& name, const std::string& url, std::string* error);
  bool test_source(const std::string& url, std::size_t* entries, std::string* error);
  void refresh_custom_sources();

  std::string fetched_at() const;
  std::size_t mapper_entries() const;
  std::size_t screenshot_entries() const;
  std::string raw_base() const;
  Config& config();

 private:
  void save_cache() const;
  void apply_mapper_metadata(std::vector<Game>& games);
  void apply_screenshot_metadata(std::vector<Game>& games);
  std::vector<Game> fetch_github(std::string* error) const;
  std::vector<Game> fetch_homebrew_hub(std::string* error) const;
  std::vector<Game> fetch_pdroms(std::string* error) const;
  std::vector<Game> fetch_archive_org(std::string* error) const;
  std::vector<Game> fetch_custom(const CustomSource& source, std::string* error) const;
  void apply_readme_metadata(std::vector<Game>& games) const;

  Config& config_;
  HttpClient& http_;
  std::filesystem::path cache_path_;
  mutable std::mutex mutex_;
  std::vector<Game> games_;
  std::string fetched_at_;
  std::size_t mapper_entries_ = 0;
  std::size_t screenshot_entries_ = 0;
};

}  // namespace nes

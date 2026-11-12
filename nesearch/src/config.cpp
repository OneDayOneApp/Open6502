#include "config.hpp"

#include <nlohmann/json.hpp>

#include "util.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace nes {

Config Config::load(const fs::path& path) {
  Config config;
  config.source_path = path;
  config.download_dir = default_download_dir().string();

  const std::string raw = read_file(path);
  if (raw.empty()) return config;

  try {
    const json data = json::parse(raw);
    config.download_dir = data.value("download_dir", config.download_dir);
    config.jobs = data.value("jobs", config.jobs);
    config.timeout_seconds = data.value("timeout_seconds", config.timeout_seconds);
    config.proxy = data.value("proxy", config.proxy);
    config.ui_port = data.value("ui_port", config.ui_port);
    config.ui_open = data.value("ui_open", config.ui_open);
    if (data.contains("github") && data["github"].is_object()) {
      const auto& gh = data["github"];
      config.github_repo = gh.value("repo", config.github_repo);
      config.github_ref = gh.value("ref", config.github_ref);
      config.github_api_base = gh.value("api_base", config.github_api_base);
      config.github_raw_base = gh.value("raw_base", config.github_raw_base);
    }
    config.gem6502_path = data.value("gem6502_path", config.gem6502_path);
    config.nemu_path = data.value("nemu_path", config.nemu_path);
    config.homebrew_hub_enabled = data.value("homebrew_hub_enabled", config.homebrew_hub_enabled);
    config.pdroms_enabled = data.value("pdroms_enabled", config.pdroms_enabled);
    config.archive_org_enabled = data.value("archive_org_enabled", config.archive_org_enabled);
    config.archive_org_query = data.value("archive_org_query", config.archive_org_query);
    config.archive_org_limit = data.value("archive_org_limit", config.archive_org_limit);
    config.tuxnes_mapper_enabled = data.value("tuxnes_mapper_enabled", config.tuxnes_mapper_enabled);
    config.tuxnes_mapper_url = data.value("tuxnes_mapper_url", config.tuxnes_mapper_url);
    config.fogleman_screenshots_enabled = data.value("fogleman_screenshots_enabled", config.fogleman_screenshots_enabled);
    config.fogleman_screenshots_url = data.value("fogleman_screenshots_url", config.fogleman_screenshots_url);
    if (data.contains("sources") && data["sources"].is_array()) {
      for (const auto& item : data["sources"]) {
        if (!item.is_object()) continue;
        CustomSource source;
        source.id = item.value("id", "");
        source.name = item.value("name", "");
        source.url = item.value("url", "");
        source.enabled = item.value("enabled", true);
        if (!source.url.empty()) {
          if (source.id.empty()) source.id = slugify(source.name.empty() ? source.url : source.name);
          if (source.name.empty()) source.name = source.id;
          config.sources.push_back(std::move(source));
        }
      }
    }
  } catch (const std::exception&) {
    // A malformed config should not prevent the CLI from running.
  }

  config.download_dir = expand_user(config.download_dir).string();
  if (config.jobs < 1) config.jobs = 1;
  if (config.jobs > 16) config.jobs = 16;
  if (config.timeout_seconds < 5) config.timeout_seconds = 5;
  return config;
}

bool Config::save_current(std::string* error) const {
  return save(source_path.empty() ? default_path() : source_path, error);
}

bool Config::save(const fs::path& path, std::string* error) const {
  json data;
  data["schema"] = 1;
  data["download_dir"] = download_dir;
  data["jobs"] = jobs;
  data["timeout_seconds"] = timeout_seconds;
  data["proxy"] = proxy;
  data["ui_port"] = ui_port;
  data["ui_open"] = ui_open;
  data["gem6502_path"] = gem6502_path;
  data["nemu_path"] = nemu_path;
  data["homebrew_hub_enabled"] = homebrew_hub_enabled;
  data["pdroms_enabled"] = pdroms_enabled;
  data["archive_org_enabled"] = archive_org_enabled;
  data["archive_org_query"] = archive_org_query;
  data["archive_org_limit"] = archive_org_limit;
  data["tuxnes_mapper_enabled"] = tuxnes_mapper_enabled;
  data["tuxnes_mapper_url"] = tuxnes_mapper_url;
  data["fogleman_screenshots_enabled"] = fogleman_screenshots_enabled;
  data["fogleman_screenshots_url"] = fogleman_screenshots_url;
  data["github"] = {
      {"repo", github_repo},
      {"ref", github_ref},
      {"api_base", github_api_base},
      {"raw_base", github_raw_base},
  };
  data["sources"] = json::array();
  for (const auto& source : sources) {
    data["sources"].push_back({{"id", source.id},
                               {"name", source.name},
                               {"url", source.url},
                               {"enabled", source.enabled}});
  }
  return write_file_atomic(path, data.dump(2, ' ', false, json::error_handler_t::replace) + "\n", error);
}

fs::path Config::default_path() {
  if (const char* value = std::getenv("NESEARCH_CONFIG")) return fs::path(value);
  return config_dir() / "config.json";
}

fs::path Config::cache_path() {
  if (const char* value = std::getenv("NESEARCH_CACHE_DIR")) return fs::path(value) / "catalog.json";
  return cache_dir() / "catalog.json";
}

fs::path Config::library_path() {
  if (const char* value = std::getenv("NESEARCH_DATA_DIR")) return fs::path(value) / "library.json";
  return data_dir() / "library.json";
}

fs::path Config::log_path() {
  if (const char* value = std::getenv("NESEARCH_CACHE_DIR")) return fs::path(value) / "nesearch.log";
  return cache_dir() / "nesearch.log";
}

}  // namespace nes

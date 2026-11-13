#include "export.hpp"

#include <algorithm>
#include <map>
#include <string>

#include "util.hpp"

using json = nlohmann::json;

namespace nes {

json game_export_json(const Game& game) {
  const bool unknown = lower(game.license) == "unknown" || game.license.empty();
  return {
      {"id", game.id},
      {"title", game.title},
      {"author", game.author},
      {"license", game.license.empty() ? "Unknown" : game.license},
      {"license_kind", unknown ? "unknown" : "open"},
      {"description", game.description},
      {"page_url", game.page_url},
      {"source", game.source},
      {"source_id", game.source_id},
      {"file", game.file},
      {"url", game.url},
      {"sha256", game.sha256},
      {"screenshot", game.screenshot_url},
      {"size", game.size},
      {"size_label", human_size(game.size)},
      {"mapper", game.mapper >= 0 ? game.mapper_name : ""},
      {"mapper_name", game.mapper_name},
      {"mapper_num", game.mapper},
      {"prg_kb", game.prg_kb},
      {"chr_kb", game.chr_kb},
      {"mirroring", game.mirroring},
      {"battery", game.battery},
      {"year", game.year},
      {"date", game.date},
  };
}

json source_export_json(const Config& config, const CatalogService& catalog) {
  const auto games = catalog.games();
  std::map<std::string, std::size_t> counts;
  for (const auto& game : games) counts[game.source_id] += 1;

  json sources = json::array();
  sources.push_back({{"id", "github:" + config.github_repo},
                     {"name", "GitHub"},
                     {"type", "github"},
                     {"url", "https://github.com/" + config.github_repo},
                     {"enabled", true},
                     {"entries", counts["github:" + config.github_repo]},
                     {"last_sync", catalog.fetched_at()},
                     {"status", "online"}});
  if (config.homebrew_hub_enabled) {
    sources.push_back({{"id", "homebrew-hub"},
                       {"name", "Homebrew Hub"},
                       {"type", "homebrew"},
                       {"url", "https://hh.gbdev.io"},
                       {"enabled", true},
                       {"entries", counts["homebrew-hub"]},
                       {"last_sync", catalog.fetched_at()},
                       {"status", "online"}});
  }
  if (config.pdroms_enabled) {
    sources.push_back({{"id", "pdroms"},
                       {"name", "PDRoms"},
                       {"type", "pdroms"},
                       {"url", "https://pdroms.de"},
                       {"enabled", true},
                       {"entries", counts["pdroms"]},
                       {"last_sync", catalog.fetched_at()},
                       {"status", "online"}});
  }
  if (config.archive_org_enabled) {
    sources.push_back({{"id", "archive-org"},
                       {"name", "Internet Archive"},
                       {"type", "archive"},
                       {"url", "https://archive.org"},
                       {"enabled", true},
                       {"entries", counts["archive-org"]},
                       {"last_sync", catalog.fetched_at()},
                       {"status", counts["archive-org"] > 0 ? "online" : "offline"}});
  }
  if (config.tuxnes_mapper_enabled) {
    sources.push_back({{"id", "tuxnes-mapper"},
                       {"name", "TuxNES Mapper List"},
                       {"type", "metadata"},
                       {"url", config.tuxnes_mapper_url},
                       {"enabled", true},
                       {"entries", catalog.mapper_entries()},
                       {"last_sync", catalog.fetched_at()},
                       {"status", catalog.mapper_entries() > 0 ? "online" : "offline"}});
  }
  if (config.fogleman_screenshots_enabled) {
    sources.push_back({{"id", "fogleman-screens"},
                       {"name", "Fogleman Title Screens"},
                       {"type", "metadata"},
                       {"url", config.fogleman_screenshots_url},
                       {"enabled", true},
                       {"entries", catalog.screenshot_entries()},
                       {"last_sync", catalog.fetched_at()},
                       {"status", catalog.screenshot_entries() > 0 ? "online" : "offline"}});
  }
  for (const auto& source : config.sources) {
    sources.push_back({{"id", source.id},
                       {"name", source.name},
                       {"type", "json"},
                       {"url", source.url},
                       {"enabled", source.enabled},
                       {"entries", counts["custom:" + source.id]},
                       {"last_sync", catalog.fetched_at()},
                       {"status", source.enabled ? "online" : "disabled"}});
  }
  return sources;
}

json catalog_export_json(const Config& config, const CatalogService& catalog) {
  json games = json::array();
  for (const auto& game : catalog.games()) games.push_back(game_export_json(game));
  const auto sources = source_export_json(config, catalog);

  return {
      {"schema", 1},
      {"generator", std::string("nesearch ") + NESEARCH_VERSION},
      {"generated_at", now_iso8601()},
      {"fetched_at", catalog.fetched_at()},
      {"totals", {{"games", games.size()}, {"sources", sources.size()}}},
      {"sources", sources},
      {"games", std::move(games)},
  };
}

}  // namespace nes

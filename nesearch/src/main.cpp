#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "catalog.hpp"
#include "config.hpp"
#include "download.hpp"
#include "http.hpp"
#include "library.hpp"
#include "sha256.hpp"
#include "ui_server.hpp"
#include "util.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

struct Globals {
  bool json = false;
  bool verbose = false;
  bool quiet = false;
  bool offline = false;
  bool help = false;
  bool version = false;
  std::string config_path;
  std::string proxy;
  int timeout = -1;
};

void print_help() {
  std::cout << R"(nesearch — search and download NES homebrew

Usage:
  nesearch                         Open the local web UI (TTY/desktop)
  nesearch ui [--port N] [--no-open] [--ui-dir DIR]
  nesearch update
  nesearch search [query] [--all] [--limit N] [--json]
                  [--license all|open|unknown] [--source NAME]
                  [--mapper all|NROM|MMC1|CNROM|other] [--sort relevance|title|size|newest]
  nesearch info <id> [--json]
  nesearch download <id>... [-o DIR] [--force] [--jobs N] [--json]
  nesearch list [--json]
  nesearch verify [--json]
  nesearch sources [--json]
  nesearch config [path|show|init]
  nesearch --help | --version

Global:
  --config PATH     configuration file
  --proxy URL       HTTP(S) proxy
  --timeout SEC     network timeout
  --json            machine-readable output
  --offline         use cached catalog only
  --verbose         verbose diagnostics
  --quiet           errors only
)";
}

std::string make_token() {
  std::random_device device;
  std::mt19937_64 generator(device());
  std::ostringstream out;
  for (int i = 0; i < 4; ++i) {
    out << std::hex << std::setw(8) << std::setfill('0') << static_cast<std::uint32_t>(generator());
  }
  return out.str();
}

json game_json(const nes::Game& game) {
  return {
      {"id", game.id},
      {"title", game.title},
      {"author", game.author},
      {"license", game.license},
      {"source", game.source},
      {"source_id", game.source_id},
      {"size", game.size},
      {"size_label", nes::human_size(game.size)},
      {"mapper", game.mapper_name},
      {"mapper_num", game.mapper},
      {"prg_kb", game.prg_kb},
      {"chr_kb", game.chr_kb},
      {"battery", game.battery},
      {"year", game.year},
      {"description", game.description},
      {"page_url", game.page_url},
      {"url", game.url},
      {"file", game.file},
      {"sha256", game.sha256},
  };
}

std::string display_title(const nes::Game& game) {
  return game.title.empty() ? game.id : game.title;
}

int cmd_update(nes::Config& config, nes::CatalogService& catalog, const Globals& globals) {
  try {
    catalog.refresh();
    config.save_current();
    if (globals.json) {
      std::cout << json({{"ok", true}, {"games", catalog.total()}, {"fetched_at", catalog.fetched_at()}}).dump(2, ' ', false, json::error_handler_t::replace)
                << "\n";
    } else {
      std::cout << "Updated " << catalog.total() << " entries (" << catalog.fetched_at() << ")\n";
    }
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "nesearch: error: " << exception.what() << "\n";
    return 3;
  }
}

int cmd_search(nes::CatalogService& catalog, std::vector<std::string> args, const Globals& globals) {
  std::string license = "all";
  std::string source = "all";
  std::string mapper = "all";
  std::string sort = "relevance";
  std::size_t limit = 50;
  bool all = false;
  std::vector<std::string> query_parts;

  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--all") {
      all = true;
    } else if (arg == "--limit" && i + 1 < args.size()) {
      limit = static_cast<std::size_t>(std::stoul(args[++i]));
    } else if (arg == "--license" && i + 1 < args.size()) {
      license = args[++i];
    } else if (arg == "--source" && i + 1 < args.size()) {
      source = args[++i];
    } else if (arg == "--mapper" && i + 1 < args.size()) {
      mapper = args[++i];
    } else if (arg == "--sort" && i + 1 < args.size()) {
      sort = args[++i];
    } else if (!arg.empty() && arg[0] != '-') {
      query_parts.push_back(arg);
    }
  }

  std::string query;
  for (const auto& part : query_parts) {
    if (!query.empty()) query += " ";
    query += part;
  }
  if (query.empty() && !all) {
    // Listing everything is useful and matches the UI's empty search.
    all = true;
  }

  const auto games = catalog.search(query, license, source, mapper, sort, limit, 0);
  const auto total = catalog.search(query, license, source, mapper, sort, 0, 0).size();
  if (globals.json) {
    json list = json::array();
    for (const auto& game : games) list.push_back(game_json(game));
    std::cout << json({{"query", query}, {"count", games.size()}, {"total", total}, {"games", list}}).dump(2, ' ', false, json::error_handler_t::replace)
              << "\n";
    return 0;
  }

  if (games.empty()) {
    std::cout << "No games found";
    if (!query.empty()) std::cout << " for \"" << query << "\"";
    std::cout << ".\n";
    return 0;
  }

  std::cout << "ID                        TITLE                          AUTHOR                    SIZE      MAPPER\n";
  std::cout << "----------------------------------------------------------------------------------------------------\n";
  for (const auto& game : games) {
    std::string id = game.id;
    std::string title = display_title(game);
    std::string author = game.author.empty() ? "-" : game.author;
    if (id.size() > 24) id = id.substr(0, 24);
    if (title.size() > 29) title = title.substr(0, 29);
    if (author.size() > 24) author = author.substr(0, 24);
    std::cout << id << std::string(26 - id.size(), ' ') << title
              << std::string(31 - title.size(), ' ') << author << std::string(26 - author.size(), ' ')
              << nes::human_size(game.size) << std::string(10 - std::min<std::size_t>(9, nes::human_size(game.size).size()), ' ')
              << (game.mapper_name.empty() ? "-" : game.mapper_name) << "\n";
  }
  std::cout << "\n" << games.size() << " shown of " << total << " (source: " << catalog.config().github_repo
            << ")\n";
  return 0;
}

int cmd_info(nes::CatalogService& catalog, const std::string& id, const Globals& globals) {
  auto game = catalog.find(id);
  if (!game) {
    std::cerr << "nesearch: error: game not found: " << id << "\n";
    return 4;
  }
  catalog.enrich(*game);
  if (globals.json) {
    std::cout << game_json(*game).dump(2, ' ', false, json::error_handler_t::replace) << "\n";
    return 0;
  }
  std::cout << display_title(*game) << "\n";
  std::cout << "  ID          " << game->id << "\n";
  std::cout << "  Author      " << (game->author.empty() ? "-" : game->author) << "\n";
  std::cout << "  Size        " << nes::human_size(game->size) << " (" << game->size << " bytes)\n";
  std::cout << "  License     " << game->license << "\n";
  std::cout << "  Source      " << game->source << " (" << game->source_id << ")\n";
  std::cout << "  Mapper      " << (game->mapper_name.empty() ? "-" : game->mapper_name) << "\n";
  if (!game->description.empty()) std::cout << "  Description " << game->description << "\n";
  if (!game->page_url.empty()) std::cout << "  Page        " << game->page_url << "\n";
  std::cout << "  URL         " << game->url << "\n";
  return 0;
}

int cmd_download(nes::DownloadManager& downloads, std::vector<std::string> args, const Globals& globals) {
  std::string output;
  bool force = false;
  std::vector<std::string> ids;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if ((arg == "-o" || arg == "--output") && i + 1 < args.size()) {
      output = args[++i];
    } else if (arg == "--force") {
      force = true;
    } else if (arg == "--jobs" && i + 1 < args.size()) {
      ++i;  // jobs are applied before constructing the manager
    } else if (!arg.empty() && arg[0] != '-') {
      ids.push_back(arg);
    }
  }
  if (ids.empty()) {
    std::cerr << "nesearch: error: at least one game id is required\n";
    return 2;
  }

  try {
    downloads.enqueue(ids, output, force);
  } catch (const std::exception& exception) {
    std::cerr << "nesearch: error: " << exception.what() << "\n";
    return 6;
  }

  while (downloads.has_active()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
  const auto results = downloads.snapshot();
  int failed = 0;
  for (const auto& item : results) {
    if (item.state == "done") {
      if (!globals.json) std::cout << "saved  " << item.path << "  (" << nes::human_size(item.total) << ")\n";
    } else if (item.state == "paused") {
      if (!globals.json) std::cout << "paused " << item.id << "\n";
    } else if (item.state == "failed") {
      ++failed;
      std::cerr << "nesearch: error: " << item.id << ": " << item.error << "\n";
    }
  }
  if (globals.json) {
    json list = json::array();
    for (const auto& item : results) {
      list.push_back({{"id", item.id},
                      {"state", item.state},
                      {"path", item.path},
                      {"error", item.error},
                      {"size", item.total}});
    }
    std::cout << json({{"downloads", list}}).dump(2, ' ', false, json::error_handler_t::replace) << "\n";
  }
  return failed ? 8 : 0;
}

int cmd_list(nes::LibraryService& library, const Globals& globals) {
  const auto entries = library.list();
  if (globals.json) {
    json list = json::array();
    for (const auto& entry : entries) {
      list.push_back({{"id", entry.id},
                      {"title", entry.title},
                      {"path", entry.path},
                      {"size", entry.size},
                      {"sha256", entry.sha256},
                      {"mapper", entry.mapper_name}});
    }
    std::cout << json({{"entries", list}, {"count", list.size()}}).dump(2, ' ', false, json::error_handler_t::replace) << "\n";
    return 0;
  }
  if (entries.empty()) {
    std::cout << "Library is empty. Run `nesearch download <id>` first.\n";
    return 0;
  }
  std::cout << "ID                        TITLE                          SIZE      MAPPER    FILE\n";
  for (const auto& entry : entries) {
    std::cout << entry.id.substr(0, 24) << std::string(26 - std::min<std::size_t>(24, entry.id.size()), ' ')
              << entry.title.substr(0, 29) << std::string(31 - std::min<std::size_t>(29, entry.title.size()), ' ')
              << nes::human_size(entry.size) << std::string(10, ' ')
              << (entry.mapper_name.empty() ? "-" : entry.mapper_name) << std::string(10, ' ') << entry.path
              << "\n";
  }
  return 0;
}

int cmd_verify(nes::LibraryService& library, const Globals& globals) {
  const auto entries = library.list();
  int bad = 0;
  json results = json::array();
  for (const auto& entry : entries) {
    std::string status = "ok";
    std::error_code ec;
    if (!fs::exists(entry.path, ec)) {
      status = "missing";
    } else if (fs::file_size(entry.path, ec) != entry.size) {
      status = "modified";
    } else if (!entry.sha256.empty() && nes::Sha256::hash_file(entry.path, nullptr) != entry.sha256) {
      status = "modified";
    }
    if (status != "ok") ++bad;
    results.push_back({{"id", entry.id}, {"path", entry.path}, {"status", status}});
    if (!globals.json) std::cout << status << "  " << entry.id << "  " << entry.path << "\n";
  }
  if (globals.json) std::cout << json({{"results", results}}).dump(2, ' ', false, json::error_handler_t::replace) << "\n";
  return bad ? 5 : 0;
}

int cmd_sources(nes::CatalogService& catalog, const Globals& globals) {
  const auto games = catalog.games();
  std::map<std::string, std::size_t> counts;
  for (const auto& game : games) counts[game.source_id] += 1;

  if (globals.json) {
    json list = json::array();
    list.push_back({{"id", "github:" + catalog.config().github_repo},
                    {"name", "GitHub"},
                    {"type", "github"},
                    {"entries", counts["github:" + catalog.config().github_repo]}});
    if (catalog.config().homebrew_hub_enabled) {
      list.push_back({{"id", "homebrew-hub"}, {"name", "Homebrew Hub"}, {"type", "homebrew"},
                      {"entries", counts["homebrew-hub"]}});
    }
    if (catalog.config().pdroms_enabled) {
      list.push_back({{"id", "pdroms"}, {"name", "PDRoms"}, {"type", "pdroms"},
                      {"entries", counts["pdroms"]}});
    }
    if (catalog.config().archive_org_enabled) {
      list.push_back({{"id", "archive-org"}, {"name", "Internet Archive"}, {"type", "archive"},
                      {"entries", counts["archive-org"]}});
    }
    if (catalog.config().tuxnes_mapper_enabled) {
      list.push_back({{"id", "tuxnes-mapper"}, {"name", "TuxNES Mapper List"}, {"type", "metadata"},
                      {"entries", catalog.mapper_entries()}});
    }
    if (catalog.config().fogleman_screenshots_enabled) {
      list.push_back({{"id", "fogleman-screens"}, {"name", "Fogleman Title Screens"}, {"type", "metadata"},
                      {"entries", catalog.screenshot_entries()}});
    }
    for (const auto& source : catalog.config().sources) {
      list.push_back({{"id", source.id},
                      {"name", source.name},
                      {"type", "json"},
                      {"url", source.url},
                      {"enabled", source.enabled},
                      {"entries", counts["custom:" + source.id]}});
    }
    std::cout << json({{"sources", list}}).dump(2, ' ', false, json::error_handler_t::replace) << "\n";
    return 0;
  }
  std::cout << "GitHub  (" << catalog.config().github_repo << ")  " << counts["github:" + catalog.config().github_repo] << " entries\n";
  if (catalog.config().homebrew_hub_enabled) {
    std::cout << "HUB     Homebrew Hub  " << counts["homebrew-hub"] << " entries\n";
  }
  if (catalog.config().pdroms_enabled) {
    std::cout << "PDROMS  PDRoms  " << counts["pdroms"] << " entries\n";
  }
  if (catalog.config().archive_org_enabled) {
    std::cout << "ARCHIVE Internet Archive  " << counts["archive-org"] << " entries\n";
  }
  if (catalog.config().tuxnes_mapper_enabled) {
    std::cout << "META    TuxNES Mapper List  " << catalog.mapper_entries() << " records\n";
  }
  if (catalog.config().fogleman_screenshots_enabled) {
    std::cout << "META    Fogleman Title Screens  " << catalog.screenshot_entries() << " screenshots\n";
  }
  for (const auto& source : catalog.config().sources) {
    std::cout << "JSON    " << source.name << "  " << counts["custom:" + source.id] << " entries  "
              << (source.enabled ? "enabled" : "disabled") << "\n";
  }
  return 0;
}

int cmd_config(const nes::Config& config, const std::vector<std::string>& args) {
  const std::string action = args.empty() ? "show" : args[0];
  if (action == "path") {
    std::cout << (config.source_path.empty() ? nes::Config::default_path() : config.source_path).string() << "\n";
    return 0;
  }
  if (action == "init") {
    std::string error;
    if (!config.save_current(&error)) {
      std::cerr << "nesearch: error: " << error << "\n";
      return 6;
    }
    std::cout << "Wrote " << nes::Config::default_path() << "\n";
    return 0;
  }
  std::cout << "config   " << (config.source_path.empty() ? nes::Config::default_path() : config.source_path).string() << "\n";
  std::cout << "cache    " << nes::Config::cache_path().string() << "\n";
  std::cout << "library  " << nes::Config::library_path().string() << "\n";
  std::cout << "download " << nes::expand_user(config.download_dir).string() << "\n";
  std::cout << "repo     " << config.github_repo << "@" << config.github_ref << "\n";
  std::cout << "jobs     " << config.jobs << "\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Globals globals;
  std::vector<std::string> tokens;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      globals.help = true;
    } else if (arg == "--version" || arg == "-V") {
      globals.version = true;
    } else if (arg == "--json") {
      globals.json = true;
    } else if (arg == "--verbose" || arg == "-v") {
      globals.verbose = true;
    } else if (arg == "--quiet" || arg == "-q") {
      globals.quiet = true;
    } else if (arg == "--offline") {
      globals.offline = true;
    } else if (arg == "--config" && i + 1 < argc) {
      globals.config_path = argv[++i];
    } else if (arg == "--proxy" && i + 1 < argc) {
      globals.proxy = argv[++i];
    } else if (arg == "--timeout" && i + 1 < argc) {
      globals.timeout = std::stoi(argv[++i]);
    } else {
      tokens.push_back(arg);
    }
  }

  if (globals.version) {
    std::cout << "nesearch " << NESEARCH_VERSION << "\n";
    return 0;
  }
  if (globals.help) {
    print_help();
    return 0;
  }

  const fs::path config_path = globals.config_path.empty() ? nes::Config::default_path()
                                                           : nes::expand_user(globals.config_path);
  nes::Config config = nes::Config::load(config_path);
  if (!globals.proxy.empty()) config.proxy = globals.proxy;
  if (globals.timeout > 0) config.timeout_seconds = globals.timeout;

  const std::string command = tokens.empty() ? "" : tokens[0];
  std::vector<std::string> rest;
  if (!tokens.empty()) rest.assign(tokens.begin() + 1, tokens.end());

  if (command.empty()) {
#ifdef _WIN32
    const bool tty = _isatty(_fileno(stdin));
#else
    const bool tty = isatty(fileno(stdin)) != 0;
#endif
    const char* headless = std::getenv("NESEARCH_HEADLESS");
    if (!tty || (headless && *headless)) {
      print_help();
      return 0;
    }
  }

  if (command == "config") {
    return cmd_config(config, rest);
  }

  nes::HttpClient http(config.proxy, config.timeout_seconds);
  nes::CatalogService catalog(config, http, nes::Config::cache_path());
  catalog.load();
  nes::LibraryService library(nes::Config::library_path());
  library.load();

  if (command == "update") {
    return cmd_update(config, catalog, globals);
  }
  if (command == "search") {
    return cmd_search(catalog, rest, globals);
  }
  if (command == "info") {
    if (rest.empty()) {
      std::cerr << "nesearch: error: info requires a game id\n";
      return 2;
    }
    return cmd_info(catalog, rest[0], globals);
  }
  if (command == "list") {
    return cmd_list(library, globals);
  }
  if (command == "verify") {
    return cmd_verify(library, globals);
  }
  if (command == "sources") {
    return cmd_sources(catalog, globals);
  }

  if (command == "ui" || command.empty()) {
    int port = config.ui_port;
    bool open_browser = config.ui_open;
    const char* no_open_env = std::getenv("NESEARCH_UI_NO_OPEN");
    if (no_open_env && *no_open_env && std::string(no_open_env) != "0") open_browser = false;
    std::string ui_dir = NESEARCH_UI_DIR;
    for (std::size_t i = 0; i < rest.size(); ++i) {
      if (rest[i] == "--port" && i + 1 < rest.size()) {
        port = std::stoi(rest[++i]);
      } else if (rest[i] == "--no-open") {
        open_browser = false;
      } else if (rest[i] == "--ui-dir" && i + 1 < rest.size()) {
        ui_dir = rest[++i];
      }
    }
    if (!globals.offline && catalog.total() == 0) {
      try {
        catalog.refresh();
      } catch (const std::exception& exception) {
        std::cerr << "nesearch: warning: " << exception.what() << "\n";
      }
    }
    nes::DownloadManager downloads(catalog, config, library);
    try {
      const char* forced_token = std::getenv("NESEARCH_UI_TOKEN");
      std::string token = (forced_token && *forced_token) ? forced_token : make_token();
      nes::UiServer server(config, catalog, downloads, library, token, ui_dir);
      server.run(port, open_browser);
    } catch (const std::exception& exception) {
      std::cerr << "nesearch: error: " << exception.what() << "\n";
      return 1;
    }
    return 0;
  }

  if (command == "download") {
    for (std::size_t i = 0; i + 1 < rest.size(); ++i) {
      if (rest[i] == "--jobs") config.jobs = std::clamp(std::stoi(rest[i + 1]), 1, 16);
    }
    nes::DownloadManager downloads(catalog, config, library);
    return cmd_download(downloads, rest, globals);
  }

  std::cerr << "nesearch: error: unknown command: " << command << "\n";
  print_help();
  return 2;
}

#include "ui_server.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

#include <nlohmann/json.hpp>

#include "httplib.h"
#include "sha256.hpp"
#include "util.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace nes {

namespace {

httplib::Server* g_server = nullptr;

void stop_server(int) {
  if (g_server) g_server->stop();
}

void open_path(const std::string& path) {
#ifdef __APPLE__
  const std::string command = "open \"" + path + "\" >/dev/null 2>&1 &";
#elif defined(_WIN32)
  const std::string command = "explorer \"" + path + "\"";
#else
  const std::string command = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
#endif
  std::system(command.c_str());
}

void open_browser(const std::string& url) {
#ifdef __APPLE__
  const std::string command = "open \"" + url + "\" >/dev/null 2>&1 &";
#elif defined(_WIN32)
  const std::string command = "start \"\" \"" + url + "\"";
#else
  const std::string command = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
#endif
  std::system(command.c_str());
}

json game_json(const Game& game) {
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

json download_json(const DownloadInfo& info) {
  return {
      {"id", info.id},
      {"title", info.title},
      {"state", info.state},
      {"error", info.error},
      {"path", info.path},
      {"received", info.received},
      {"total", info.total},
      {"speed", info.speed},
      {"progress", info.progress},
  };
}

json library_json(const LibraryEntry& entry) {
  return {
      {"id", entry.id},
      {"title", entry.title},
      {"path", entry.path},
      {"source", entry.source},
      {"sha256", entry.sha256},
      {"size", entry.size},
      {"size_label", human_size(entry.size)},
      {"mapper", entry.mapper_name},
      {"mapper_num", entry.mapper},
      {"downloaded_at", entry.downloaded_at},
  };
}

void send_json(httplib::Response& res, const json& value, int status = 200) {
  res.status = status;
  res.set_content(value.dump(-1, ' ', false, json::error_handler_t::replace), "application/json; charset=utf-8");
}

void send_error(httplib::Response& res, int status, const std::string& message) {
  send_json(res, {{"error", message}, {"status", status}}, status);
}

bool authorized(const httplib::Request& req, const std::string& token) {
  if (req.get_header_value("X-NESearch-Token") == token) return true;
  const std::string cookie = req.get_header_value("Cookie");
  return cookie.find("nesearch_token=" + token) != std::string::npos;
}

}  // namespace

UiServer::UiServer(Config& config, CatalogService& catalog, DownloadManager& downloads,
                   LibraryService& library, std::string token, std::string ui_dir)
    : config_(config),
      catalog_(catalog),
      downloads_(downloads),
      library_(library),
      token_(std::move(token)),
      ui_dir_(std::move(ui_dir)),
      emulators_(config) {}

void UiServer::run(int port, bool open_browser_flag) {
  httplib::Server server;
  g_server = &server;
  std::signal(SIGINT, stop_server);
  std::signal(SIGTERM, stop_server);

  auto require_auth = [this](const httplib::Request& req, httplib::Response& res) {
    if (authorized(req, token_)) return true;
    send_error(res, 401, "invalid or missing UI token");
    return false;
  };

  server.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
    bool authed = authorized(req, token_);
    if (req.has_param("token")) {
      if (req.get_param_value("token") != token_) {
        res.status = 403;
        res.set_content("Invalid nesearch UI token. Restart `nesearch ui` and open the printed URL.", "text/plain");
        return;
      }
      res.set_header("Set-Cookie", "nesearch_token=" + token_ + "; HttpOnly; SameSite=Strict; Path=/");
      authed = true;
    }
    if (!authed) {
      res.set_content("Open the URL printed by `nesearch ui` (it includes the one-time token).", "text/plain");
      return;
    }
    const std::string body = read_file(fs::path(ui_dir_) / "mockup.html");
    if (body.empty()) {
      send_error(res, 404, "UI assets not found: " + ui_dir_);
      return;
    }
    res.set_content(body, "text/html; charset=utf-8");
  });

  server.Get("/api/v1/status", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    const auto downloads = downloads_.snapshot();
    std::size_t active = 0;
    for (const auto& item : downloads) {
      if (item.state == "queued" || item.state == "downloading" || item.state == "verifying") ++active;
    }
    send_json(res, {{"version", NESEARCH_VERSION},
                    {"games", catalog_.total()},
                    {"sources", 1 + (config_.homebrew_hub_enabled ? 1 : 0) +
                                    (config_.pdroms_enabled ? 1 : 0) +
                                    (config_.archive_org_enabled ? 1 : 0) +
                                    (config_.tuxnes_mapper_enabled ? 1 : 0) +
                                    (config_.fogleman_screenshots_enabled ? 1 : 0) + config_.sources.size()},
                    {"active_downloads", active},
                    {"fetched_at", catalog_.fetched_at()},
                    {"download_dir", expand_user(config_.download_dir).string()}});
  });

  server.Get("/api/v1/emulators", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    json list = json::array();
    for (const auto& item : emulators_.list()) {
      list.push_back({{"id", item.id}, {"name", item.name}, {"path", item.path}, {"available", item.available}});
    }
    send_json(res, {{"emulators", list}});
  });

  server.Post("/api/v1/play", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    try {
      const json body = json::parse(req.body);
      const std::string id = body.value("id", "");
      const std::string emulator = body.value("emulator", "");
      if (id.empty() || (emulator != "gem6502" && emulator != "nemu")) {
        send_error(res, 400, "id and a valid emulator (gem6502|nemu) are required");
        return;
      }
      if (id.find('/') != std::string::npos || id.find('\\') != std::string::npos) {
        send_error(res, 400, "invalid game id");
        return;
      }
      fs::path rom;
      if (const auto entry = library_.find(id)) {
        rom = entry->path;
      } else {
        rom = expand_user(config_.download_dir) / (id + ".nes");
      }
      if (!fs::exists(rom)) {
        send_error(res, 409, "download the game before playing: " + id);
        return;
      }
      std::string error;
      if (!emulators_.launch(emulator, rom, &error)) {
        send_error(res, 409, error);
        return;
      }
      send_json(res, {{"ok", true}, {"emulator", emulator}, {"rom", rom.string()}});
    } catch (const std::exception& exception) {
      send_error(res, 400, exception.what());
    }
  });

  server.Get("/api/v1/games", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    std::string query = req.has_param("q") ? req.get_param_value("q") : "";
    std::string license = req.has_param("license") ? req.get_param_value("license") : "all";
    std::string source = req.has_param("source") ? req.get_param_value("source") : "all";
    std::string mapper = req.has_param("mapper") ? req.get_param_value("mapper") : "all";
    std::string sort = req.has_param("sort") ? req.get_param_value("sort") : "relevance";
    std::size_t limit = 500;
    std::size_t offset = 0;
    try {
      if (req.has_param("limit")) limit = static_cast<std::size_t>(std::stoul(req.get_param_value("limit")));
      if (req.has_param("offset")) offset = static_cast<std::size_t>(std::stoul(req.get_param_value("offset")));
    } catch (...) {
    }
    const auto all = catalog_.search(query, license, source, mapper, sort, 0, 0);
    const auto page = catalog_.search(query, license, source, mapper, sort, limit, offset);
    json games = json::array();
    for (const auto& game : page) games.push_back(game_json(game));
    send_json(res, {{"games", games}, {"total", all.size()}, {"limit", limit}, {"offset", offset}});
  });

  server.Get(R"(/api/v1/games/([^/]+))", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    const std::string id = req.matches[1].str();
    auto game = catalog_.find(id);
    if (!game) {
      send_error(res, 404, "game not found: " + id);
      return;
    }
    catalog_.enrich(*game);
    send_json(res, {{"game", game_json(*game)}});
  });

  server.Get("/api/v1/downloads", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    json list = json::array();
    for (const auto& item : downloads_.snapshot()) list.push_back(download_json(item));
    send_json(res, {{"downloads", list}});
  });

  server.Post("/api/v1/downloads", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    try {
      const json body = json::parse(req.body);
      std::vector<std::string> ids;
      for (const auto& id : body.value("ids", json::array())) ids.push_back(id.get<std::string>());
      if (ids.empty()) {
        send_error(res, 400, "ids is required");
        return;
      }
      const std::string output = body.value("output", "");
      const bool force = body.value("force", false);
      downloads_.enqueue(ids, output, force);
      json list = json::array();
      for (const auto& item : downloads_.snapshot()) list.push_back(download_json(item));
      send_json(res, {{"downloads", list}});
    } catch (const std::exception& exception) {
      send_error(res, 400, exception.what());
    }
  });

  server.Post(R"(/api/v1/downloads/([^/]+)/(pause|resume|remove))",
              [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
                if (!require_auth(req, res)) return;
                const std::string id = req.matches[1].str();
                const std::string action = req.matches[2];
                if (action == "pause") downloads_.pause(id);
                else if (action == "resume") downloads_.resume(id);
                else if (action == "remove") downloads_.remove(id);
                json list = json::array();
                for (const auto& item : downloads_.snapshot()) list.push_back(download_json(item));
                send_json(res, {{"downloads", list}});
              });

  server.Get("/api/v1/library", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    json entries = json::array();
    for (const auto& entry : library_.list()) entries.push_back(library_json(entry));
    send_json(res, {{"entries", entries}, {"count", entries.size()}});
  });

  server.Post("/api/v1/library/verify", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    json results = json::array();
    for (const auto& entry : library_.list()) {
      const fs::path path(entry.path);
      std::error_code ec;
      std::string status = "ok";
      if (!fs::exists(path, ec)) {
        status = "missing";
      } else if (fs::file_size(path, ec) != entry.size) {
        status = "modified";
      } else if (!entry.sha256.empty() && Sha256::hash_file(path, nullptr) != entry.sha256) {
        status = "modified";
      }
      results.push_back({{"id", entry.id}, {"status", status}});
    }
    send_json(res, {{"results", results}});
  });

  server.Get("/api/v1/sources", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    const auto games = catalog_.games();
    std::map<std::string, std::size_t> counts;
    for (const auto& game : games) counts[game.source_id] += 1;

    json sources = json::array();
    sources.push_back({{"id", "github:" + config_.github_repo},
                       {"name", "GitHub"},
                       {"type", "github"},
                       {"url", "https://github.com/" + config_.github_repo},
                       {"enabled", true},
                       {"entries", counts["github:" + config_.github_repo]},
                       {"last_sync", catalog_.fetched_at()},
                       {"status", "online"}});
    if (config_.homebrew_hub_enabled) {
      sources.push_back({{"id", "homebrew-hub"},
                         {"name", "Homebrew Hub"},
                         {"type", "homebrew"},
                         {"url", "https://hh.gbdev.io"},
                         {"enabled", true},
                         {"entries", counts["homebrew-hub"]},
                         {"last_sync", catalog_.fetched_at()},
                         {"status", "online"}});
    }
    if (config_.pdroms_enabled) {
      sources.push_back({{"id", "pdroms"},
                         {"name", "PDRoms"},
                         {"type", "pdroms"},
                         {"url", "https://pdroms.de"},
                         {"enabled", true},
                         {"entries", counts["pdroms"]},
                         {"last_sync", catalog_.fetched_at()},
                         {"status", "online"}});
    }
    if (config_.archive_org_enabled) {
      sources.push_back({{"id", "archive-org"},
                         {"name", "Internet Archive"},
                         {"type", "archive"},
                         {"url", "https://archive.org"},
                         {"enabled", true},
                         {"entries", counts["archive-org"]},
                         {"last_sync", catalog_.fetched_at()},
                         {"status", counts["archive-org"] > 0 ? "online" : "offline"}});
    }
    if (config_.tuxnes_mapper_enabled) {
      sources.push_back({{"id", "tuxnes-mapper"},
                         {"name", "TuxNES Mapper List"},
                         {"type", "metadata"},
                         {"url", config_.tuxnes_mapper_url},
                         {"enabled", true},
                         {"entries", catalog_.mapper_entries()},
                         {"last_sync", catalog_.fetched_at()},
                         {"status", catalog_.mapper_entries() > 0 ? "online" : "offline"}});
    }
    if (config_.fogleman_screenshots_enabled) {
      sources.push_back({{"id", "fogleman-screens"},
                         {"name", "Fogleman Title Screens"},
                         {"type", "metadata"},
                         {"url", config_.fogleman_screenshots_url},
                         {"enabled", true},
                         {"entries", catalog_.screenshot_entries()},
                         {"last_sync", catalog_.fetched_at()},
                         {"status", catalog_.screenshot_entries() > 0 ? "online" : "offline"}});
    }
    for (const auto& source : config_.sources) {
      sources.push_back({{"id", source.id},
                         {"name", source.name},
                         {"type", "json"},
                         {"url", source.url},
                         {"enabled", source.enabled},
                         {"entries", counts["custom:" + source.id]},
                         {"last_sync", catalog_.fetched_at()},
                         {"status", source.enabled ? "online" : "disabled"}});
    }
    send_json(res, {{"sources", sources}, {"count", sources.size()}});
  });

  server.Post("/api/v1/sources/test", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    try {
      const json body = json::parse(req.body);
      std::string error;
      std::size_t entries = 0;
      if (!catalog_.test_source(body.value("url", ""), &entries, &error)) {
        send_error(res, 400, error);
        return;
      }
      send_json(res, {{"ok", true}, {"entries", entries}});
    } catch (const std::exception& exception) {
      send_error(res, 400, exception.what());
    }
  });

  server.Post("/api/v1/sources", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    try {
      const json body = json::parse(req.body);
      std::string error;
      if (!catalog_.add_source(body.value("name", ""), body.value("url", ""), &error)) {
        send_error(res, 400, error);
        return;
      }
      send_json(res, {{"ok", true}});
    } catch (const std::exception& exception) {
      send_error(res, 400, exception.what());
    }
  });

  server.Post("/api/v1/open", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    std::string path = expand_user(config_.download_dir).string();
    try {
      const json body = json::parse(req.body);
      const std::string id = body.value("id", "");
      if (!id.empty()) {
        const auto entry = library_.find(id);
        if (!entry) {
          send_error(res, 404, "library entry not found: " + id);
          return;
        }
        path = fs::path(entry->path).parent_path().string();
      }
    } catch (...) {
    }
    if (!fs::exists(path)) {
      send_error(res, 404, "path does not exist: " + path);
      return;
    }
    open_path(path);
    send_json(res, {{"ok", true}, {"path", path}});
  });

  server.Post("/api/v1/update", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    try {
      catalog_.refresh();
      send_json(res, {{"ok", true}, {"games", catalog_.total()}, {"fetched_at", catalog_.fetched_at()}});
    } catch (const std::exception& exception) {
      send_error(res, 502, exception.what());
    }
  });

  server.Get("/api/v1/config", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    send_json(res, {{"download_dir", config_.download_dir},
                    {"jobs", config_.jobs},
                    {"proxy", config_.proxy},
                    {"ui_port", config_.ui_port},
                    {"archive_org_enabled", config_.archive_org_enabled},
                    {"archive_org_query", config_.archive_org_query},
                    {"archive_org_limit", config_.archive_org_limit}});
  });

  server.Put("/api/v1/config", [this, &require_auth](const httplib::Request& req, httplib::Response& res) {
    if (!require_auth(req, res)) return;
    try {
      const json body = json::parse(req.body);
      config_.download_dir = body.value("download_dir", config_.download_dir);
      config_.jobs = std::clamp(body.value("jobs", config_.jobs), 1, 16);
      config_.proxy = body.value("proxy", config_.proxy);
      config_.archive_org_enabled = body.value("archive_org_enabled", config_.archive_org_enabled);
      config_.archive_org_query = body.value("archive_org_query", config_.archive_org_query);
      config_.archive_org_limit = std::clamp(body.value("archive_org_limit", config_.archive_org_limit), 1, 50);
      std::string error;
      if (!config_.save_current(&error)) {
        send_error(res, 500, error);
        return;
      }
      send_json(res, {{"ok", true}});
    } catch (const std::exception& exception) {
      send_error(res, 400, exception.what());
    }
  });

  if (!server.set_mount_point("/", ui_dir_)) {
    std::cerr << "nesearch: UI directory not found: " << ui_dir_ << "\n";
  }

  int actual_port = port;
  if (port <= 0) {
    actual_port = server.bind_to_any_port("127.0.0.1");
  } else if (!server.bind_to_port("127.0.0.1", port)) {
    std::cerr << "nesearch: port " << port << " is busy; using a random local port\n";
    actual_port = server.bind_to_any_port("127.0.0.1");
  }
  if (actual_port <= 0) throw std::runtime_error("cannot allocate a local UI port");

  const std::string url = "http://127.0.0.1:" + std::to_string(actual_port) + "/?token=" + token_;
  std::cout << "nesearch UI  " << url << std::endl;
  std::cout << "Press Ctrl-C to stop." << std::endl;
  if (open_browser_flag) open_browser(url);
  server.listen_after_bind();
  g_server = nullptr;
}

}  // namespace nes

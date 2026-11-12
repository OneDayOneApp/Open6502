#include "catalog.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <thread>

#include <nlohmann/json.hpp>

#include "sha256.hpp"
#include "util.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace nes {

namespace {

json game_to_json(const Game& game) {
  return {
      {"id", game.id},
      {"title", game.title},
      {"author", game.author},
      {"license", game.license},
      {"description", game.description},
      {"page_url", game.page_url},
      {"source", game.source},
      {"source_id", game.source_id},
      {"file", game.file},
      {"url", game.url},
      {"sha256", game.sha256},
      {"screenshot", game.screenshot_url},
      {"size", game.size},
      {"mapper", game.mapper},
      {"mapper_name", game.mapper_name},
      {"prg_kb", game.prg_kb},
      {"chr_kb", game.chr_kb},
      {"mirroring", game.mirroring},
      {"battery", game.battery},
      {"year", game.year},
      {"date", game.date},
  };
}

Game game_from_json(const json& data) {
  Game game;
  game.id = data.value("id", "");
  game.title = data.value("title", game.id);
  game.author = data.value("author", "");
  game.license = data.value("license", "Unknown");
  game.description = data.value("description", "");
  game.page_url = data.value("page_url", "");
  game.source = data.value("source", "GitHub");
  game.source_id = data.value("source_id", "github");
  game.file = data.value("file", game.id + ".nes");
  game.url = data.value("url", "");
  game.sha256 = data.value("sha256", "");
  game.screenshot_url = data.value("screenshot", "");
  game.size = data.value("size", 0ULL);
  game.mapper = data.value("mapper", -1);
  game.mapper_name = data.value("mapper_name", "");
  game.prg_kb = data.value("prg_kb", 0);
  game.chr_kb = data.value("chr_kb", 0);
  game.mirroring = data.value("mirroring", "");
  game.battery = data.value("battery", false);
  game.year = data.value("year", 0);
  game.date = data.value("date", "");
  return game;
}

std::string url_encode_path(std::string_view text) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  for (unsigned char ch : text) {
    if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == '/') {
      out.push_back(static_cast<char>(ch));
    } else {
      out.push_back('%');
      out.push_back(hex[ch >> 4]);
      out.push_back(hex[ch & 0x0F]);
    }
  }
  return out;
}

bool is_http_url(const std::string& url) {
  return starts_with(url, "https://") || starts_with(url, "http://");
}

std::string mapper_name_for(int mapper, std::string label) {
  label = trim(label);
  const std::size_t paren = label.find('(');
  if (paren != std::string::npos) label = trim(label.substr(0, paren));
  if (!label.empty() && label != "----" && label != "-none assigned-") return label;
  switch (mapper) {
    case 0: return "NROM";
    case 1: return "MMC1";
    case 2: return "UNROM";
    case 3: return "CNROM";
    case 4: return "MMC3";
    case 5: return "MMC5";
    case 6: return "FFE";
    case 7: return "AOROM";
    case 9: return "MMC2";
    case 10: return "MMC4";
    case 11: return "Color Dreams";
    case 15: return "100-in-1";
    case 16: return "Bandai";
    case 18: return "Jaleco";
    case 19: return "Namcot 106";
    case 21: return "VRC4";
    case 22: return "VRC2a";
    case 23: return "VRC2b";
    case 24: return "VRC6";
    case 25: return "VRC4";
    case 26: return "VRC6";
    case 32: return "Irem G-101";
    case 33: return "Taito TC0190";
    case 34: return "BNROM";
    case 64: return "RAMBO-1";
    case 65: return "Irem H-3001";
    case 66: return "GxROM";
    case 67: return "Sunsoft-3";
    case 68: return "Sunsoft-4";
    case 69: return "Sunsoft-5";
    case 70: return "74161/32";
    case 71: return "Camerica";
    case 79: return "AVE";
    case 85: return "VRC7";
    case 228: return "Action 52";
    case 232: return "Quattro";
    default: return mapper >= 0 ? "Mapper " + std::to_string(mapper) : "";
  }
}

std::string mapper_base_title(std::string title) {
  static const std::regex region_re(
      R"(\s*\((?:J|U|E|PAL|Japan|USA|Europe|World|Hacked|Unl|Unlicensed|Disk Conv\.?|Rev\.? ?[A-Z0-9]+)\)\s*$)",
      std::regex::icase);
  std::smatch match;
  while (std::regex_search(title, match, region_re)) {
    title = trim(title.substr(0, static_cast<std::size_t>(match.position())));
  }
  return title;
}

std::string metadata_title_key(std::string title) {
  title = mapper_base_title(std::move(title));
  std::string lowered = lower(title);
  for (const std::string suffix : {", the", ", a", ", an"}) {
    if (ends_with(lowered, suffix)) {
      title = trim(title.substr(0, title.size() - suffix.size()));
      lowered = lower(title);
      break;
    }
  }
  for (const std::string prefix : {"the ", "a ", "an "}) {
    if (starts_with(lowered, prefix)) {
      title = trim(title.substr(prefix.size()));
      break;
    }
  }
  return normalize_title(title);
}

std::string archive_license(std::string url) {
  if (url.empty()) return "Unknown";
  const std::string lowered = lower(url);
  if (lowered.find("publicdomain/mark") != std::string::npos) return "Public Domain Mark";
  if (lowered.find("publicdomain/zero") != std::string::npos) return "CC0 1.0";
  const std::size_t licenses = lowered.find("/licenses/");
  if (licenses != std::string::npos) {
    std::string code = url.substr(licenses + 10);
    while (!code.empty() && code.back() == '/') code.pop_back();
    for (char& ch : code) {
      if (ch == '/') ch = ' ';
    }
    if (starts_with(lower(code), "by-nc-sa")) code = "CC BY-NC-SA " + trim(code.substr(8));
    else if (starts_with(lower(code), "by-nc")) code = "CC BY-NC " + trim(code.substr(5));
    else if (starts_with(lower(code), "by-sa")) code = "CC BY-SA " + trim(code.substr(5));
    else if (starts_with(lower(code), "by")) code = "CC BY " + trim(code.substr(2));
    return trim(code);
  }
  return url;
}


std::string github_token() {
  const char* token = std::getenv("GITHUB_TOKEN");
  return token ? token : "";
}

}  // namespace

CatalogService::CatalogService(Config& config, HttpClient& http, fs::path cache_path)
    : config_(config), http_(http), cache_path_(std::move(cache_path)) {}

void CatalogService::load() {
  std::lock_guard lock(mutex_);
  const std::string raw = read_file(cache_path_);
  if (raw.empty()) return;
  try {
    const json data = json::parse(raw);
    fetched_at_ = data.value("fetched_at", "");
    mapper_entries_ = data.value("mapper_entries", 0);
    screenshot_entries_ = data.value("screenshot_entries", 0);
    games_.clear();
    if (data.contains("games") && data["games"].is_array()) {
      for (const auto& item : data["games"]) {
        Game game = game_from_json(item);
        if (!game.id.empty() && !game.url.empty()) games_.push_back(std::move(game));
      }
    }
  } catch (const std::exception&) {
    games_.clear();
    fetched_at_.clear();
  }
}

void CatalogService::save_cache() const {
  json data;
  data["schema"] = 1;
  data["fetched_at"] = fetched_at_;
  data["mapper_entries"] = mapper_entries_;
  data["screenshot_entries"] = screenshot_entries_;
  data["games"] = json::array();
  for (const auto& game : games_) data["games"].push_back(game_to_json(game));
  std::string error;
  if (!write_file_atomic(cache_path_, data.dump(2, ' ', false, json::error_handler_t::replace) + "\n", &error)) {
    // Cache failures should not take down a running UI.
  }
}

std::string CatalogService::raw_base() const {
  return config_.github_raw_base + "/" + config_.github_repo + "/" + config_.github_ref + "/";
}

std::vector<Game> CatalogService::fetch_github(std::string* error) const {
  std::vector<Game> games;
  const std::string url = config_.github_api_base + "/repos/" + config_.github_repo + "/git/trees/" +
                          config_.github_ref + "?recursive=1";
  std::vector<std::string> headers = {"Accept: application/vnd.github+json"};
  const std::string token = github_token();
  if (!token.empty()) headers.push_back("Authorization: Bearer " + token);

  const HttpResponse response = http_.get(url, headers);
  if (!response.ok()) {
    if (error) {
      *error = "GitHub tree HTTP " + std::to_string(response.status) + ": " +
               truncate_middle(response.body.empty() ? response.error : response.body, 240);
    }
    return games;
  }

  try {
    const json tree = json::parse(response.body);
    const std::string base = raw_base();
    std::set<std::string> used;
    std::set<std::string> images;
    for (const auto& item : tree.value("tree", json::array())) {
      const std::string path = item.value("path", "");
      if (item.value("type", "") == "blob" && ends_with(path, ".png")) images.insert(path);
    }
    for (const auto& item : tree.value("tree", json::array())) {
      const std::string path = item.value("path", "");
      if (item.value("type", "") != "blob" || !ends_with(path, ".nes")) continue;
      const std::string stem = fs::path(path).stem().string();
      std::string id = slugify(stem);
      if (id.empty()) continue;
      if (!used.insert(id).second) {
        id += "-2";
        used.insert(id);
      }
      Game game;
      game.id = id;
      game.title = title_from_slug(stem);
      game.author = "";
      game.license = "Unknown";
      game.source = "GitHub";
      game.source_id = "github:" + config_.github_repo;
      game.file = path;
      game.url = base + url_encode_path(path);
      game.size = item.value("size", 0ULL);
      const std::string image_path = fs::path(path).stem().string() + ".png";
      if (images.count(image_path)) game.screenshot_url = base + url_encode_path(image_path);
      games.push_back(std::move(game));
    }
  } catch (const std::exception& exception) {
    if (error) *error = std::string("cannot parse GitHub tree: ") + exception.what();
    games.clear();
    return games;
  }

  apply_readme_metadata(games);
  return games;
}

void CatalogService::apply_readme_metadata(std::vector<Game>& games) const {
  const std::string url = raw_base() + "README.md";
  const HttpResponse response = http_.get(url);
  if (!response.ok()) return;

  struct Meta {
    std::string title;
    std::string author;
  };
  std::map<std::string, Meta> by_title;
  const std::regex line_re(R"(^\s*-\s*<b>(.*?)</b>\s+by\s+(.+?)\s*$)", std::regex::icase);
  for (const std::string& line : split_lines(response.body)) {
    std::smatch match;
    if (!std::regex_match(line, match, line_re)) continue;
    Meta meta{html_unescape(strip_tags(match[1].str())), html_unescape(strip_tags(match[2].str()))};
    by_title[normalize_title(meta.title)] = meta;
  }

  for (auto& game : games) {
    const auto it = by_title.find(normalize_title(game.title));
    if (it != by_title.end()) {
      game.title = it->second.title;
      game.author = it->second.author;
    }
  }
}

std::vector<Game> CatalogService::fetch_homebrew_hub(std::string* error) const {
  std::vector<Game> games;
  const std::string search_base = "https://hh3.gbdev.io/api/search?platform=NES&page_elements=10&page=";
  const std::string raw = config_.github_raw_base + "/nesdev-org/homebrew-db/master/entries/";
  std::set<std::string> used;

  json entries = json::array();
  int page_total = 1;
  for (int page = 1; page <= page_total && page <= 20; ++page) {
    const HttpResponse response = http_.get(search_base + std::to_string(page));
    if (!response.ok()) {
      if (page == 1 && error) {
        *error = "Homebrew Hub search HTTP " + std::to_string(response.status);
      }
      break;
    }
    try {
      const json data = json::parse(response.body);
      page_total = data.value("page_total", 1);
      for (const auto& entry : data.value("entries", json::array())) entries.push_back(entry);
    } catch (const std::exception& exception) {
      if (page == 1 && error) *error = std::string("Homebrew Hub JSON: ") + exception.what();
      break;
    }
  }
  if (entries.empty()) return games;

  std::vector<json> details(entries.size());
  std::atomic<std::size_t> next_entry{0};
  const auto detail_worker = [&]() {
    while (true) {
      const std::size_t index = next_entry.fetch_add(1);
      if (index >= entries.size()) break;
      const std::string slug = entries[index].value("slug", "");
      if (slug.empty()) continue;
      const HttpResponse detail = http_.get("https://hh3.gbdev.io/api/entry/" + slug + ".json");
      if (!detail.ok()) continue;
      try {
        json parsed = json::parse(detail.body);
        if (parsed.is_object()) details[index] = std::move(parsed);
      } catch (const std::exception&) {
      }
    }
  };
  std::vector<std::thread> detail_threads;
  const std::size_t detail_count = std::min<std::size_t>(8, entries.size());
  for (std::size_t i = 0; i < detail_count; ++i) detail_threads.emplace_back(detail_worker);
  for (auto& thread : detail_threads) thread.join();

  for (std::size_t index = 0; index < entries.size(); ++index) {
    const auto& search_entry = entries[index];
    const std::string slug = search_entry.value("slug", "");
    if (slug.empty()) continue;
    json entry = details[index].is_object() ? details[index] : search_entry;

    std::string filename;
    std::string sha256;
    for (const auto& file : entry.value("files", json::array())) {
      const std::string name = file.value("filename", "");
      if (!ends_with(lower(name), ".nes")) continue;
      if (filename.empty() || file.value("default", false)) {
        filename = name;
        if (file.contains("hash") && file["hash"].is_object()) {
          sha256 = file["hash"].value("sha256", "");
        }
        if (file.value("default", false)) break;
      }
    }
    if (filename.empty()) continue;

    std::string id = slugify(slug);
    if (id.empty()) continue;
    if (!used.insert(id).second) id += "-hh";
    used.insert(id);

    Game game;
    game.id = id;
    game.title = entry.value("title", title_from_slug(id));
    game.author = entry.value("developer", "");
    game.license = entry.value("gameLicense", "Unknown");
    if (game.license.empty()) game.license = "Unknown";
    game.description = entry.value("description", "");
    game.page_url = entry.value("repository", entry.value("website", ""));
    game.source = "Homebrew Hub";
    game.source_id = "homebrew-hub";
    game.file = filename;
    game.url = raw + url_encode_path(slug) + "/" + url_encode_path(filename);
    game.sha256 = sha256;
    const auto screenshots = entry.value("screenshots", json::array());
    if (!screenshots.empty() && screenshots[0].is_string()) {
      game.screenshot_url = raw + url_encode_path(slug) + "/" + url_encode_path(screenshots[0].get<std::string>());
    }
    game.date = entry.value("date", "");
    if (game.date.size() >= 4) {
      try { game.year = std::stoi(game.date.substr(0, 4)); } catch (const std::exception&) {}
    }
    games.push_back(std::move(game));
  }
  return games;
}

std::vector<Game> CatalogService::fetch_pdroms(std::string* error) const {
  std::vector<Game> games;
  const std::string rss_url = "https://pdroms.de/rss/files/nintendo-entertainment-system-nes-famicom/";
  const HttpResponse response = http_.get(rss_url);
  if (!response.ok()) {
    if (error) *error = "PDRoms RSS HTTP " + std::to_string(response.status);
    return games;
  }

  const std::regex item_re(R"(<item>([\s\S]*?)</item>)", std::regex::icase);
  const std::regex title_re(R"(<title>([\s\S]*?)</title>)", std::regex::icase);
  const std::regex link_re(R"(<link>([\s\S]*?)</link>)", std::regex::icase);
  const std::regex desc_re(R"(<description>([\s\S]*?)</description>)", std::regex::icase);
  std::set<std::string> used;

  struct RssItem { std::string title; std::string link; std::string description; };
  std::vector<RssItem> items;
  for (auto it = std::sregex_iterator(response.body.begin(), response.body.end(), item_re);
       it != std::sregex_iterator(); ++it) {
    const std::string block = (*it)[1].str();
    std::smatch match;
    RssItem item;
    if (std::regex_search(block, match, title_re)) item.title = html_unescape(strip_tags(match[1].str()));
    if (std::regex_search(block, match, link_re)) item.link = trim(html_unescape(strip_tags(match[1].str())));
    if (std::regex_search(block, match, desc_re)) item.description = html_unescape(strip_tags(match[1].str()));
    if (!item.link.empty()) items.push_back(std::move(item));
  }

  std::vector<std::string> pages(items.size());
  std::atomic<std::size_t> next_page{0};
  const auto page_worker = [&]() {
    while (true) {
      const std::size_t index = next_page.fetch_add(1);
      if (index >= items.size()) break;
      const HttpResponse item = http_.get(items[index].link);
      if (item.ok()) pages[index] = item.body;
    }
  };
  std::vector<std::thread> page_threads;
  const std::size_t page_count = std::min<std::size_t>(6, items.size());
  for (std::size_t i = 0; i < page_count; ++i) page_threads.emplace_back(page_worker);
  for (auto& thread : page_threads) thread.join();

  for (std::size_t index = 0; index < items.size(); ++index) {
    const RssItem& rss = items[index];
    const std::string& title = rss.title;
    const std::string& link = rss.link;
    const std::string& description = rss.description;
    if (pages[index].empty()) continue;
    const std::regex jsonld_re(R"(<script[^>]*type=["']application/ld\+json["'][^>]*>([\s\S]*?)</script>)",
                               std::regex::icase);
    std::smatch json_match;
    if (!std::regex_search(pages[index], json_match, jsonld_re)) continue;

    json data;
    try {
      data = json::parse(json_match[1].str());
    } catch (const std::exception&) {
      continue;
    }
    json application;
    if (data.is_object() && data.value("@type", "") == "SoftwareApplication") {
      application = data;
    } else if (data.is_array()) {
      for (const auto& candidate : data) {
        if (candidate.is_object() && candidate.value("@type", "") == "SoftwareApplication") {
          application = candidate;
          break;
        }
      }
    }
    if (application.is_null() || application.empty()) continue;

    const std::string download_url = application.value("downloadUrl", "");
    if (download_url.empty()) continue;
    std::string id = slugify(fs::path(link).filename().string());
    if (id.empty()) id = slugify(application.value("name", title));
    if (id.empty()) continue;
    if (!used.insert(id).second) id += "-pd";
    used.insert(id);

    Game game;
    game.id = id;
    game.title = application.value("name", title);
    if (game.title.empty()) game.title = title_from_slug(id);
    if (application.contains("author")) {
      const auto& author = application["author"];
      game.author = author.is_object() ? author.value("name", "") : author.get<std::string>();
    }
    game.license = "Unknown";
    game.description = application.value("description", description);
    game.page_url = link;
    game.source = "PDRoms";
    game.source_id = "pdroms";
    game.file = id + ".zip";
    game.url = download_url;
    game.screenshot_url = application.value("screenshot", application.value("image", ""));
    game.date = application.value("datePublished", "");
    if (game.date.size() >= 4) {
      try { game.year = std::stoi(game.date.substr(0, 4)); } catch (const std::exception&) {}
    }
    if (application.contains("fileSize")) {
      try {
        if (application["fileSize"].is_number()) game.size = application["fileSize"].get<std::uint64_t>();
        else game.size = std::stoull(application["fileSize"].get<std::string>());
      } catch (const std::exception&) {}
    }
    games.push_back(std::move(game));
  }
  return games;
}

std::vector<Game> CatalogService::fetch_archive_org(std::string* error) const {
  std::vector<Game> games;
  if (!config_.archive_org_enabled) return games;

  // Archive.org is often slow or unreachable; use a short timeout so a broken
  // source does not stall the rest of the catalog refresh.
  HttpClient archive_http(config_.proxy, std::min(config_.timeout_seconds, 8));
  const int limit = std::max(1, std::min(config_.archive_org_limit, 50));
  const std::string query = url_encode_path(config_.archive_org_query);
  const std::string url = "https://archive.org/advancedsearch.php?q=" + query +
                          "&fl[]=identifier&fl[]=title&fl[]=creator&fl[]=date&fl[]=description"
                          "&fl[]=licenseurl&rows=" + std::to_string(limit) + "&page=1&output=json";
  const HttpResponse response = archive_http.get(url);
  if (!response.ok()) {
    if (error) *error = "Internet Archive search HTTP " + std::to_string(response.status) +
                        (response.error.empty() ? "" : ": " + response.error);
    return games;
  }

  json docs = json::array();
  try {
    const json data = json::parse(response.body);
    if (data.contains("response") && data["response"].is_object()) {
      docs = data["response"].value("docs", json::array());
    }
  } catch (const std::exception& exception) {
    if (error) *error = std::string("Internet Archive JSON: ") + exception.what();
    return games;
  }

  std::set<std::string> used;
  for (const auto& doc : docs) {
    const std::string identifier = doc.value("identifier", "");
    if (identifier.empty()) continue;

    const HttpResponse meta_response = archive_http.get("https://archive.org/metadata/" + url_encode_path(identifier));
    if (!meta_response.ok()) {
      if (error) *error = "Internet Archive metadata HTTP " + std::to_string(meta_response.status);
      break;
    }

    json metadata;
    try {
      metadata = json::parse(meta_response.body);
    } catch (const std::exception& exception) {
      if (error) *error = std::string("Internet Archive metadata JSON: ") + exception.what();
      break;
    }
    if (!metadata.contains("files") || !metadata["files"].is_array()) continue;

    const json info = metadata.contains("metadata") && metadata["metadata"].is_object()
                          ? metadata["metadata"] : json::object();
    auto first_text = [](const json& value) -> std::string {
      if (value.is_string()) return value.get<std::string>();
      if (value.is_array() && !value.empty() && value[0].is_string()) return value[0].get<std::string>();
      return "";
    };
    std::string title = first_text(doc.value("title", json()));
    if (title.empty()) title = first_text(info.value("title", json()));
    if (title.empty()) title = identifier;
    std::string author = first_text(doc.value("creator", json()));
    if (author.empty()) author = first_text(info.value("creator", json()));
    std::string description = first_text(doc.value("description", json()));
    if (description.empty()) description = first_text(info.value("description", json()));
    std::string license_url = doc.value("licenseurl", "");
    if (license_url.empty()) license_url = info.value("licenseurl", "");
    const std::string license = archive_license(license_url);
    std::string date = doc.value("date", "");
    if (date.empty()) date = info.value("date", "");
    const std::string page_url = "https://archive.org/details/" + identifier;
    const std::string thumbnail = "https://archive.org/services/img/" + url_encode_path(identifier);
    std::size_t file_index = 0;
    std::size_t rom_count = 0;
    for (const auto& file : metadata["files"]) {
      const std::string name = file.value("name", "");
      if (!ends_with(lower(name), ".nes") || file.value("private", false)) continue;
      ++rom_count;
    }

    for (const auto& file : metadata["files"]) {
      const std::string name = file.value("name", "");
      if (!ends_with(lower(name), ".nes") || file.value("private", false)) continue;
      std::string id = slugify(identifier);
      if (id.empty()) id = "archive-item";
      if (rom_count > 1) id += "-" + slugify(fs::path(name).stem().string());
      if (!used.insert(id).second) id += "-" + std::to_string(file_index + 2);
      used.insert(id);

      Game game;
      game.id = id;
      game.title = rom_count > 1 ? title + " - " + fs::path(name).stem().string() : title;
      game.author = author;
      game.license = license;
      game.description = description;
      game.page_url = page_url;
      game.source = "Internet Archive";
      game.source_id = "archive-org";
      game.file = name;
      game.url = "https://archive.org/download/" + url_encode_path(identifier) + "/" + url_encode_path(name);
      game.sha256 = file.value("sha256", "");
      game.screenshot_url = thumbnail;
      game.date = date;
      if (game.date.size() >= 4) {
        try { game.year = std::stoi(game.date.substr(0, 4)); } catch (const std::exception&) {}
      }
      try {
        if (file.contains("size")) {
          if (file["size"].is_number()) game.size = file["size"].get<std::uint64_t>();
          else game.size = std::stoull(file["size"].get<std::string>());
        }
      } catch (const std::exception&) {}
      games.push_back(std::move(game));
      ++file_index;
    }
  }
  return games;
}

std::size_t CatalogService::screenshot_entries() const {
  std::lock_guard lock(mutex_);
  return screenshot_entries_;
}

void CatalogService::apply_screenshot_metadata(std::vector<Game>& games) {
  if (!config_.fogleman_screenshots_enabled) return;
  const HttpResponse response = http_.get(config_.fogleman_screenshots_url);
  if (!response.ok()) {
    screenshot_entries_ = 0;
    return;
  }

  std::string base = config_.fogleman_screenshots_url;
  if (!base.empty() && base.back() != '/') base.push_back('/');
  std::map<std::string, std::string> screenshots;
  const std::regex item_re(R"re(<img\s+src="([^"]+\.png)"\s*><p>(.*?)</p>)re", std::regex::icase);
  for (auto it = std::sregex_iterator(response.body.begin(), response.body.end(), item_re);
       it != std::sregex_iterator(); ++it) {
    const std::string image = (*it)[1].str();
    const std::string title = html_unescape(strip_tags((*it)[2].str()));
    const std::string key = metadata_title_key(title);
    if (key.empty() || screenshots.count(key)) continue;
    screenshots.emplace(key, is_http_url(image) ? image : base + image);
  }

  screenshot_entries_ = screenshots.size();
  for (auto& game : games) {
    if (!game.screenshot_url.empty() && game.source_id != "archive-org") continue;
    const std::string key = metadata_title_key(game.title);
    const auto it = screenshots.find(key);
    if (it != screenshots.end()) game.screenshot_url = it->second;
  }
}

std::size_t CatalogService::mapper_entries() const {
  std::lock_guard lock(mutex_);
  return mapper_entries_;
}

void CatalogService::apply_mapper_metadata(std::vector<Game>& games) {
  if (!config_.tuxnes_mapper_enabled) return;
  const HttpResponse response = http_.get(config_.tuxnes_mapper_url);
  if (!response.ok()) {
    if (std::getenv("NESEARCH_DEBUG")) {
      std::cerr << "tuxnes: HTTP " << response.status << " error=" << response.error
                << " body=" << response.body.substr(0, 120) << "\n";
    }
    mapper_entries_ = 0;
    return;
  }

  struct Meta {
    int mapper = -1;
    std::string name;
    int prg_kb = 0;
    int chr_kb = 0;
    std::string mirroring;
  };
  std::map<std::string, Meta> metadata;
  const std::regex row_re(R"(^(.*?)(\d+)k\s+PRG\s*/\s*(\d+)k\s+CHR\s+([HVJ4])\s+(.+?)\s*$)",
                          std::regex::icase);
  const std::regex number_re(R"(\((\d+)\)\s*$)");

  bool in_table = false;
  for (std::string line : split_lines(response.body)) {
    std::replace(line.begin(), line.end(), '\t', ' ');
    if (!in_table) {
      if (line.find("Title") != std::string::npos && line.find("Mapper") != std::string::npos) in_table = true;
      continue;
    }
    if (line.find("--MISC STUFF--") != std::string::npos) break;

    std::smatch match;
    if (!std::regex_match(line, match, row_re)) continue;
    const std::string title = trim(match[1].str());
    if (title.empty()) continue;
    const int prg = std::stoi(match[2].str());
    const int chr = std::stoi(match[3].str());
    const std::string mirroring = match[4].str();
    const std::string label = trim(match[5].str());

    int mapper_num = -1;
    std::smatch number_match;
    if (std::regex_search(label, number_match, number_re)) {
      try { mapper_num = std::stoi(number_match[1].str()); } catch (const std::exception&) {}
    }
    if (mapper_num < 0) continue;

    const std::string key = metadata_title_key(title);
    if (key.empty() || metadata.count(key)) continue;
    Meta meta;
    meta.mapper = mapper_num;
    meta.name = mapper_name_for(mapper_num, label);
    meta.prg_kb = prg;
    meta.chr_kb = chr;
    meta.mirroring = mirroring;
    metadata.emplace(key, std::move(meta));
  }

  mapper_entries_ = metadata.size();
  if (std::getenv("NESEARCH_DEBUG")) std::cerr << "tuxnes: parsed " << metadata.size() << " mapper rows\n";
  for (auto& game : games) {
    if (game.mapper >= 0 || !game.mapper_name.empty()) continue;
    const std::string key = metadata_title_key(game.title);
    const auto it = metadata.find(key);
    if (it == metadata.end()) continue;
    game.mapper = it->second.mapper;
    game.mapper_name = it->second.name;
    game.prg_kb = it->second.prg_kb;
    game.chr_kb = it->second.chr_kb;
    game.mirroring = it->second.mirroring;
    if (game.size == 0 && (game.prg_kb > 0 || game.chr_kb > 0)) {
      game.size = 16 + (static_cast<std::uint64_t>(game.prg_kb) + static_cast<std::uint64_t>(game.chr_kb)) * 1024;
    }
  }
}

std::vector<Game> CatalogService::fetch_custom(const CustomSource& source, std::string* error) const {
  std::vector<Game> games;
  std::string body;
  if (is_http_url(source.url)) {
    const HttpResponse response = http_.get(source.url);
    if (!response.ok()) {
      if (error) *error = "custom source " + source.name + " HTTP " + std::to_string(response.status);
      return games;
    }
    body = response.body;
  } else {
    body = read_file(expand_user(source.url));
    if (body.empty()) {
      if (error) *error = "cannot read custom source " + source.url;
      return games;
    }
  }

  try {
    const json data = json::parse(body);
    const json list = data.is_array() ? data : data.value("games", json::array());
    for (const auto& item : list) {
      if (!item.is_object()) continue;
      Game game;
      game.title = item.value("title", "");
      game.id = slugify(item.value("id", game.title));
      if (game.id.empty()) continue;
      game.title = game.title.empty() ? title_from_slug(game.id) : game.title;
      game.author = item.value("author", "");
      game.license = item.value("license", "Unknown");
      game.description = item.value("description", "");
      game.page_url = item.value("page_url", "");
      game.source = source.name;
      game.source_id = "custom:" + source.id;
      game.file = item.value("file", game.id + ".nes");
      game.sha256 = item.value("sha256", "");
      game.screenshot_url = item.value("screenshot", item.value("image", ""));
      game.size = item.value("size", 0ULL);
      game.mapper = item.value("mapper", -1);
      game.mapper_name = item.value("mapper_name", "");
      game.year = item.value("year", 0);

      if (item.contains("assets") && item["assets"].is_array() && !item["assets"].empty()) {
        const auto& asset = item["assets"][0];
        game.url = asset.value("url", "");
        game.file = asset.value("filename", game.file);
        game.size = asset.value("size", game.size);
        game.sha256 = asset.value("sha256", game.sha256);
      }
      if (game.url.empty()) game.url = item.value("url", "");
      if (!is_http_url(game.url)) continue;
      games.push_back(std::move(game));
    }
  } catch (const std::exception& exception) {
    if (error) *error = "custom source " + source.name + ": " + exception.what();
    games.clear();
  }
  return games;
}

void CatalogService::refresh() {
  std::string error;
  std::vector<Game> merged;
  auto append = [&](std::vector<Game> games, const std::string& suffix) {
    for (auto& game : games) {
      if (std::any_of(merged.begin(), merged.end(), [&](const Game& existing) { return existing.id == game.id; })) {
        game.id += "-" + suffix;
      }
      merged.push_back(std::move(game));
    }
  };

  std::string github_error;
  std::vector<Game> github = fetch_github(&github_error);
  append(std::move(github), "gh");

  if (config_.homebrew_hub_enabled) {
    std::string hub_error;
    auto hub = fetch_homebrew_hub(&hub_error);
    if (!hub_error.empty()) error = hub_error;
    append(std::move(hub), "hh");
  }
  if (config_.pdroms_enabled) {
    std::string pd_error;
    auto pd = fetch_pdroms(&pd_error);
    if (!pd_error.empty()) error = pd_error;
    append(std::move(pd), "pd");
  }
  if (config_.archive_org_enabled) {
    std::string archive_error;
    auto archive = fetch_archive_org(&archive_error);
    if (!archive_error.empty()) error = archive_error;
    append(std::move(archive), "ia");
  }
  for (const auto& source : config_.sources) {
    if (!source.enabled) continue;
    std::string source_error;
    std::vector<Game> custom = fetch_custom(source, &source_error);
    if (!source_error.empty()) {
      // Keep the rest of the catalog usable; the source page will show its status.
      continue;
    }
    append(std::move(custom), source.id);
  }

  if (merged.empty()) {
    throw std::runtime_error(error.empty() ? "all catalog sources returned no entries" : error);
  }

  apply_mapper_metadata(merged);
  apply_screenshot_metadata(merged);

  std::lock_guard lock(mutex_);
  games_ = std::move(merged);
  fetched_at_ = now_iso8601();
  save_cache();
}

void CatalogService::refresh_custom_sources() {
  std::vector<Game> base;
  {
    std::lock_guard lock(mutex_);
    for (const auto& game : games_) {
      if (!starts_with(game.source_id, "custom:")) base.push_back(game);
    }
  }
  for (const auto& source : config_.sources) {
    if (!source.enabled) continue;
    std::string error;
    std::vector<Game> custom = fetch_custom(source, &error);
    if (!error.empty()) continue;
    for (auto& game : custom) {
      if (std::any_of(base.begin(), base.end(), [&](const Game& existing) { return existing.id == game.id; })) {
        game.id += "-" + source.id;
      }
      base.push_back(std::move(game));
    }
  }
  std::lock_guard lock(mutex_);
  games_ = std::move(base);
  fetched_at_ = now_iso8601();
  save_cache();
}

std::vector<Game> CatalogService::games() const {
  std::lock_guard lock(mutex_);
  return games_;
}

std::size_t CatalogService::total() const {
  std::lock_guard lock(mutex_);
  return games_.size();
}

std::optional<Game> CatalogService::find(std::string_view id) const {
  std::lock_guard lock(mutex_);
  for (const auto& game : games_) {
    if (game.id == id) return game;
  }
  return std::nullopt;
}

int relevance(const Game& game, const std::string& query) {
  if (query.empty()) return 0;
  const std::string id = lower(game.id);
  const std::string title = lower(game.title);
  const std::string author = lower(game.author);
  if (id == query) return 100;
  if (starts_with(title, query)) return 80;
  if (title.find(query) != std::string::npos) return 60;
  if (author.find(query) != std::string::npos) return 40;
  if (lower(game.description).find(query) != std::string::npos) return 20;
  return 10;
}

std::vector<Game> CatalogService::search(std::string_view query,
                                         std::string_view license,
                                         std::string_view source,
                                         std::string_view mapper,
                                         std::string_view sort,
                                         std::size_t limit,
                                         std::size_t offset) const {
  const std::string q = lower(trim(query));
  std::vector<std::string> tokens;
  if (!q.empty()) {
    std::size_t start = 0;
    while (start < q.size()) {
      std::size_t end = q.find(' ', start);
      if (end == std::string::npos) end = q.size();
      if (end > start) tokens.push_back(q.substr(start, end - start));
      start = end + 1;
    }
  }

  std::vector<Game> result;
  {
    std::lock_guard lock(mutex_);
    result.reserve(games_.size());
    for (const auto& game : games_) {
      if (license == "open" && lower(game.license) == "unknown") continue;
      if (license == "unknown" && lower(game.license) != "unknown") continue;
      if (source != "all" && !source.empty() && game.source != source) continue;
      if (mapper == "NROM" && game.mapper_name != "NROM") continue;
      if (mapper == "MMC1" && game.mapper_name != "MMC1") continue;
      if (mapper == "CNROM" && game.mapper_name != "CNROM") continue;
      if (mapper == "other" && (game.mapper_name == "NROM" || game.mapper_name == "MMC1" ||
                                game.mapper_name == "CNROM" || game.mapper_name.empty())) {
        continue;
      }
      if (!tokens.empty()) {
        const std::string haystack = lower(game.id + " " + game.title + " " + game.author + " " +
                                          game.description + " " + game.license + " " + game.source + " " +
                                          game.mapper_name);
        bool match = true;
        for (const auto& token : tokens) {
          if (haystack.find(token) == std::string::npos) {
            match = false;
            break;
          }
        }
        if (!match) continue;
      }
      result.push_back(game);
    }
  }

  if (sort == "title") {
    std::sort(result.begin(), result.end(), [](const Game& a, const Game& b) { return lower(a.title) < lower(b.title); });
  } else if (sort == "size") {
    std::sort(result.begin(), result.end(), [](const Game& a, const Game& b) { return a.size > b.size; });
  } else if (sort == "newest") {
    std::sort(result.begin(), result.end(), [](const Game& a, const Game& b) { return a.year > b.year; });
  } else if (!q.empty()) {
    std::sort(result.begin(), result.end(), [&](const Game& a, const Game& b) {
      const int ra = relevance(a, q);
      const int rb = relevance(b, q);
      if (ra != rb) return ra > rb;
      return lower(a.title) < lower(b.title);
    });
  } else {
    std::sort(result.begin(), result.end(), [](const Game& a, const Game& b) { return lower(a.title) < lower(b.title); });
  }

  if (offset >= result.size()) return {};
  const std::size_t end = (limit == 0) ? result.size() : std::min(result.size(), offset + limit);
  return std::vector<Game>(result.begin() + static_cast<std::ptrdiff_t>(offset),
                           result.begin() + static_cast<std::ptrdiff_t>(end));
}

void CatalogService::enrich(Game& game) {
  if (!starts_with(game.source_id, "github:")) return;
  if (!game.description.empty() && lower(game.license) != "unknown") return;

  const std::string stem = fs::path(game.file).stem().string();
  const std::string url = raw_base() + url_encode_path(stem) + ".txt";
  const HttpResponse response = http_.get(url);
  if (!response.ok()) return;

  const auto lines = split_lines(response.body);
  std::string title;
  std::string author;
  std::string description;
  std::string page_url;
  int seen_text = 0;
  const std::regex date_re(R"(^\d{4}[/-]\d{1,2}[/-]\d{1,2}$)");
  for (const auto& raw_line : lines) {
    const std::string line = trim(raw_line);
    if (line.empty() || std::regex_match(line, date_re)) continue;
    if (title.empty()) {
      title = line;
      continue;
    }
    if (author.empty() && starts_with(lower(line), "by ")) {
      author = trim(line.substr(3));
      continue;
    }
    if (starts_with(line, "http")) {
      if (page_url.empty() && line.find("youtube") == std::string::npos) page_url = line;
      continue;
    }
    if (line.size() > 3 && line[0] == '=') break;
    if (seen_text < 6) {
      if (!description.empty()) description += " ";
      description += line;
      ++seen_text;
    }
  }
  if (!title.empty()) game.title = title;
  if (!author.empty()) game.author = author;
  if (!description.empty()) game.description = description;
  if (!page_url.empty()) game.page_url = page_url;
  const std::string license = detect_license(response.body);
  if (license != "Unknown") game.license = license;

  std::lock_guard lock(mutex_);
  for (auto& item : games_) {
    if (item.id == game.id) {
      if (!game.description.empty()) item.description = game.description;
      if (!game.author.empty()) item.author = game.author;
      if (!game.page_url.empty()) item.page_url = game.page_url;
      if (license != "Unknown") item.license = license;
    }
  }
  save_cache();
}

bool CatalogService::test_source(const std::string& url, std::size_t* entries, std::string* error) {
  CustomSource source;
  source.id = "test";
  source.name = "Test source";
  source.url = trim(url);
  if (!is_http_url(source.url) && !fs::exists(expand_user(source.url))) {
    if (error) *error = "source must be an HTTPS URL or an existing local file";
    return false;
  }
  const auto games = fetch_custom(source, error);
  if (error && !error->empty()) return false;
  if (games.empty()) {
    if (error) *error = "catalog is empty or does not follow schema v1";
    return false;
  }
  if (entries) *entries = games.size();
  return true;
}

bool CatalogService::add_source(const std::string& name, const std::string& url, std::string* error) {
  const std::string clean_name = trim(name);
  const std::string clean_url = trim(url);
  if (clean_name.empty() || clean_url.empty()) {
    if (error) *error = "name and url are required";
    return false;
  }
  if (!is_http_url(clean_url) && !fs::exists(expand_user(clean_url))) {
    if (error) *error = "source must be an HTTPS URL or an existing local file";
    return false;
  }

  CustomSource source;
  source.name = clean_name;
  source.id = slugify(clean_name);
  source.url = clean_url;
  source.enabled = true;

  std::string fetch_error;
  std::vector<Game> custom = fetch_custom(source, &fetch_error);
  if (!fetch_error.empty()) {
    if (error) *error = fetch_error;
    return false;
  }

  {
    std::lock_guard lock(mutex_);
    for (const auto& existing : config_.sources) {
      if (existing.id == source.id) {
        if (error) *error = "source already exists: " + source.id;
        return false;
      }
    }
    config_.sources.push_back(source);
  }
  if (!config_.save_current(error)) return false;

  std::lock_guard lock(mutex_);
  for (auto& game : custom) {
    if (std::any_of(games_.begin(), games_.end(), [&](const Game& existing) { return existing.id == game.id; })) {
      game.id += "-" + source.id;
    }
    games_.push_back(std::move(game));
  }
  fetched_at_ = now_iso8601();
  save_cache();
  return true;
}

std::string CatalogService::fetched_at() const {
  std::lock_guard lock(mutex_);
  return fetched_at_;
}

Config& CatalogService::config() { return config_; }

}  // namespace nes

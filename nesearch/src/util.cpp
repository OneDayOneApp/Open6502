#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace nes {

std::string trim(std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
  std::size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
  return std::string(text.substr(begin, end - begin));
}

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

std::vector<std::string> split_lines(std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = text.find('\n', start);
    std::string line(text.substr(start, end == std::string_view::npos ? text.size() - start : end - start));
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(std::move(line));
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return lines;
}

std::string replace_all(std::string text, std::string_view from, std::string_view to) {
  if (from.empty()) return text;
  std::size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
  return text;
}

std::string html_unescape(std::string text) {
  text = replace_all(std::move(text), "&amp;", "&");
  text = replace_all(std::move(text), "&lt;", "<");
  text = replace_all(std::move(text), "&gt;", ">");
  text = replace_all(std::move(text), "&quot;", "\"");
  text = replace_all(std::move(text), "&#39;", "'");
  text = replace_all(std::move(text), "&apos;", "'");
  text = replace_all(std::move(text), "&nbsp;", " ");
  return text;
}

std::string strip_tags(std::string text) {
  std::string out;
  bool in_tag = false;
  for (char ch : text) {
    if (ch == '<') {
      in_tag = true;
    } else if (ch == '>') {
      in_tag = false;
    } else if (!in_tag) {
      out.push_back(ch);
    }
  }
  return out;
}

fs::path home_dir() {
#ifdef _WIN32
  if (const char* home = std::getenv("USERPROFILE")) return fs::path(home);
  const char* drive = std::getenv("HOMEDRIVE");
  const char* path = std::getenv("HOMEPATH");
  if (drive && path) return fs::path(std::string(drive) + path);
#else
  if (const char* home = std::getenv("HOME")) return fs::path(home);
#endif
  return fs::current_path();
}

fs::path expand_user(std::string_view raw) {
  std::string value(raw);
  if (!value.empty() && value[0] == '~' && (value.size() == 1 || value[1] == '/' || value[1] == '\\')) {
    value = (home_dir() / value.substr(value.size() > 1 ? 2 : 1)).string();
  }
  return fs::path(value);
}

fs::path config_dir() {
#ifdef _WIN32
  if (const char* appdata = std::getenv("APPDATA")) return fs::path(appdata) / "nesearch";
#elif defined(__APPLE__)
  return home_dir() / "Library" / "Application Support" / "nesearch";
#else
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) return fs::path(xdg) / "nesearch";
  if (const char* home = std::getenv("HOME")) return fs::path(home) / ".config" / "nesearch";
#endif
  return home_dir() / ".nesearch";
}

fs::path cache_dir() {
#ifdef _WIN32
  if (const char* local = std::getenv("LOCALAPPDATA")) return fs::path(local) / "nesearch" / "Cache";
#elif defined(__APPLE__)
  return home_dir() / "Library" / "Caches" / "nesearch";
#else
  if (const char* xdg = std::getenv("XDG_CACHE_HOME")) return fs::path(xdg) / "nesearch";
  if (const char* home = std::getenv("HOME")) return fs::path(home) / ".cache" / "nesearch";
#endif
  return home_dir() / ".nesearch" / "cache";
}

fs::path data_dir() {
#ifdef _WIN32
  if (const char* local = std::getenv("LOCALAPPDATA")) return fs::path(local) / "nesearch";
#elif defined(__APPLE__)
  return home_dir() / "Library" / "Application Support" / "nesearch";
#else
  if (const char* xdg = std::getenv("XDG_DATA_HOME")) return fs::path(xdg) / "nesearch";
  if (const char* home = std::getenv("HOME")) return fs::path(home) / ".local" / "share" / "nesearch";
#endif
  return home_dir() / ".nesearch" / "data";
}

fs::path default_download_dir() {
#ifdef _WIN32
  if (const char* profile = std::getenv("USERPROFILE")) return fs::path(profile) / "Downloads" / "nesearch";
#else
  if (const char* home = std::getenv("HOME")) {
    fs::path downloads = fs::path(home) / "Downloads";
    if (fs::exists(downloads)) return downloads / "nesearch";
    return fs::path(home) / "nesearch";
  }
#endif
  return fs::current_path() / "nesearch-roms";
}

bool ensure_dir(const fs::path& path, std::string* error) {
  std::error_code ec;
  if (fs::exists(path, ec)) {
    if (fs::is_directory(path, ec)) return true;
    if (error) *error = path.string() + " exists but is not a directory";
    return false;
  }
  fs::create_directories(path, ec);
  if (ec) {
    if (error) *error = "cannot create " + path.string() + ": " + ec.message();
    return false;
  }
  return true;
}

std::string read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

bool write_file_atomic(const fs::path& path, std::string_view data, std::string* error) {
  std::error_code ec;
  if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
  std::random_device rd;
  const auto suffix = std::to_string(rd()) + std::to_string(rd());
  fs::path temp = path;
  temp += ".tmp." + suffix;
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) {
      if (error) *error = "cannot write " + temp.string();
      return false;
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    if (!out) {
      if (error) *error = "write failed: " + temp.string();
      return false;
    }
  }
#ifdef _WIN32
  std::error_code remove_ec;
  fs::remove(path, remove_ec);
#endif
  fs::rename(temp, path, ec);
  if (ec) {
    if (error) *error = "rename failed: " + ec.message();
    std::error_code ignored;
    fs::remove(temp, ignored);
    return false;
  }
  return true;
}

std::string now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buffer;
}

std::string slugify(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  bool last_dash = false;
  for (unsigned char ch : text) {
    if (std::isalnum(ch)) {
      out.push_back(static_cast<char>(std::tolower(ch)));
      last_dash = false;
    } else if (!out.empty() && !last_dash) {
      out.push_back('-');
      last_dash = true;
    }
  }
  while (!out.empty() && out.back() == '-') out.pop_back();
  return out;
}

std::string normalize_title(std::string_view text) {
  std::string out;
  for (unsigned char ch : text) {
    if (std::isalnum(ch)) out.push_back(static_cast<char>(std::tolower(ch)));
  }
  return out;
}

std::string title_from_slug(std::string_view slug) {
  std::string out;
  bool cap = true;
  for (char ch : slug) {
    if (ch == '-' || ch == '_') {
      out.push_back(' ');
      cap = true;
    } else {
      out.push_back(cap ? static_cast<char>(std::toupper(static_cast<unsigned char>(ch))) : ch);
      cap = false;
    }
  }
  return trim(out);
}

std::string human_size(std::uint64_t bytes) {
  constexpr double kib = 1024.0;
  constexpr double mib = kib * 1024.0;
  char buffer[64];
  if (bytes >= static_cast<std::uint64_t>(mib)) {
    std::snprintf(buffer, sizeof(buffer), "%.1f MiB", static_cast<double>(bytes) / mib);
  } else if (bytes >= 1024) {
    std::snprintf(buffer, sizeof(buffer), "%.0f KiB", static_cast<double>(bytes) / kib);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
  }
  return buffer;
}

std::string truncate_middle(std::string_view text, std::size_t max_length) {
  if (text.size() <= max_length) return std::string(text);
  if (max_length < 8) return std::string(text.substr(0, max_length));
  const std::size_t head = max_length / 2 - 1;
  const std::size_t tail = max_length - head - 1;
  return std::string(text.substr(0, head)) + "\xE2\x80\xA6" + std::string(text.substr(text.size() - tail));
}

std::string detect_license(std::string_view text) {
  const std::string low = lower(text);
  if (low.find("lgpl") != std::string::npos || low.find("lesser general public license") != std::string::npos) {
    return "LGPL-3.0";
  }
  if (low.find("gnu general public license") != std::string::npos || low.find("gpl-3") != std::string::npos ||
      low.find("gplv3") != std::string::npos) {
    return "GPL-3.0";
  }
  if (low.find("gpl-2") != std::string::npos || low.find("gplv2") != std::string::npos) return "GPL-2.0";
  if (low.find("mit license") != std::string::npos) return "MIT";
  if (low.find("wtfpl") != std::string::npos) return "WTFPL";
  if (low.find("cc0") != std::string::npos || low.find("public domain") != std::string::npos) return "Public Domain";
  if (low.find("bsd") != std::string::npos) return "BSD";
  if (low.find("zlib") != std::string::npos) return "Zlib";
  return "Unknown";
}

std::string file_to_hex(const fs::path& path, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "cannot open " + path.string();
    return {};
  }
  std::ostringstream out;
  char buffer[4096];
  while (in) {
    in.read(buffer, sizeof(buffer));
    const std::streamsize count = in.gcount();
    for (std::streamsize i = 0; i < count; ++i) {
      out << std::hex << std::setw(2) << std::setfill('0')
          << static_cast<unsigned int>(static_cast<unsigned char>(buffer[i]));
    }
  }
  return out.str();
}

}  // namespace nes

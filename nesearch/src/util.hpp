#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace nes {

std::string trim(std::string_view text);
std::string lower(std::string_view text);
bool starts_with(std::string_view text, std::string_view prefix);
bool ends_with(std::string_view text, std::string_view suffix);
std::vector<std::string> split_lines(std::string_view text);
std::string replace_all(std::string text, std::string_view from, std::string_view to);
std::string html_unescape(std::string text);
std::string strip_tags(std::string text);

std::filesystem::path home_dir();
std::filesystem::path expand_user(std::string_view raw);
std::filesystem::path config_dir();
std::filesystem::path cache_dir();
std::filesystem::path data_dir();
std::filesystem::path default_download_dir();

bool ensure_dir(const std::filesystem::path& path, std::string* error = nullptr);
std::string read_file(const std::filesystem::path& path);
bool write_file_atomic(const std::filesystem::path& path, std::string_view data, std::string* error = nullptr);

std::string now_iso8601();
std::string slugify(std::string_view text);
std::string normalize_title(std::string_view text);
std::string title_from_slug(std::string_view slug);
std::string human_size(std::uint64_t bytes);
std::string truncate_middle(std::string_view text, std::size_t max_length);
std::string detect_license(std::string_view text);
std::string file_to_hex(const std::filesystem::path& path, std::string* error = nullptr);

}  // namespace nes

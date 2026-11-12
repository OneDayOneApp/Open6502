#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "model.hpp"

namespace nes {

struct Config {
  std::string download_dir;
  int jobs = 4;
  int timeout_seconds = 30;
  std::string proxy;
  std::string github_repo = "retrobrews/nes-games";
  std::string github_ref = "master";
  std::string github_api_base = "https://api.github.com";
  std::string github_raw_base = "https://raw.githubusercontent.com";
  std::string gem6502_path;
  std::string nemu_path;
  bool homebrew_hub_enabled = true;
  bool pdroms_enabled = true;
  bool archive_org_enabled = true;
  std::string archive_org_query = "(collection:softwarelibrary_nes OR collection:nes-homebrew) AND mediatype:software";
  int archive_org_limit = 12;
  bool tuxnes_mapper_enabled = true;
  std::string tuxnes_mapper_url = "https://tuxnes.sourceforge.net/nesmapper.txt";
  bool fogleman_screenshots_enabled = true;
  std::string fogleman_screenshots_url = "https://www.michaelfogleman.com/static/nes/";
  int ui_port = 8420;
  bool ui_open = true;
  std::vector<CustomSource> sources;
  std::filesystem::path source_path;

  static Config load(const std::filesystem::path& path);
  bool save(const std::filesystem::path& path, std::string* error = nullptr) const;
  bool save_current(std::string* error = nullptr) const;

  static std::filesystem::path default_path();
  static std::filesystem::path cache_path();
  static std::filesystem::path library_path();
  static std::filesystem::path log_path();
};

}  // namespace nes

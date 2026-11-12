#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nes {

struct Game {
  std::string id;
  std::string title;
  std::string author;
  std::string license;
  std::string description;
  std::string page_url;
  std::string source;
  std::string source_id;
  std::string file;
  std::string url;
  std::string sha256;
  std::string screenshot_url;
  std::uint64_t size = 0;
  int mapper = -1;
  std::string mapper_name;
  int prg_kb = 0;
  int chr_kb = 0;
  std::string mirroring;
  bool battery = false;
  int year = 0;
  std::string date;

  bool valid() const { return !id.empty() && !url.empty(); }
};

struct RomInfo {
  bool valid = false;
  int mapper = -1;
  std::string mapper_name;
  int prg_kb = 0;
  int chr_kb = 0;
  std::string mirroring;
  bool battery = false;
  bool trainer = false;
  bool nes2 = false;
  std::string error;
};

struct LibraryEntry {
  std::string id;
  std::string title;
  std::string path;
  std::string source;
  std::string sha256;
  std::string downloaded_at;
  std::uint64_t size = 0;
  int mapper = -1;
  std::string mapper_name;
};

struct DownloadInfo {
  std::string id;
  std::string title;
  std::string state;  // queued | downloading | paused | verifying | done | failed
  std::string error;
  std::string path;
  std::uint64_t received = 0;
  std::uint64_t total = 0;
  double speed = 0.0;
  int progress = 0;
};

struct CustomSource {
  std::string id;
  std::string name;
  std::string url;
  bool enabled = true;
};

}  // namespace nes

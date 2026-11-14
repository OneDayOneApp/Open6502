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

// A GitHub repository (optionally a branch + subdirectory) scanned for .nes
// files through the tree API. Multiple sources may coexist; each one becomes
// its own entry in `nesearch sources`.
struct GithubSource {
  std::string name;
  std::string repo;  // "owner/name"
  std::string ref = "master";
  std::string path;  // optional subdirectory inside the repo, e.g. "games"
  bool enabled = true;
};

inline std::string github_source_id(const GithubSource& source) {
  std::string id = "github:" + source.repo;
  if (!source.path.empty()) id += "/" + source.path;
  return id;
}

}  // namespace nes

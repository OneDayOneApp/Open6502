#include "library.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "util.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace nes {

LibraryService::LibraryService(fs::path path) : path_(std::move(path)) {}

void LibraryService::load() {
  std::lock_guard lock(mutex_);
  entries_.clear();
  const std::string raw = read_file(path_);
  if (raw.empty()) return;
  try {
    const json data = json::parse(raw);
    for (const auto& item : data.value("entries", json::array())) {
      LibraryEntry entry;
      entry.id = item.value("id", "");
      entry.title = item.value("title", entry.id);
      entry.path = item.value("path", "");
      entry.source = item.value("source", "");
      entry.sha256 = item.value("sha256", "");
      entry.downloaded_at = item.value("downloaded_at", "");
      entry.size = item.value("size", 0ULL);
      entry.mapper = item.value("mapper", -1);
      entry.mapper_name = item.value("mapper_name", "");
      if (!entry.id.empty() && !entry.path.empty()) entries_.push_back(std::move(entry));
    }
  } catch (const std::exception&) {
    entries_.clear();
  }
}

void LibraryService::save() const {
  json data;
  data["schema"] = 1;
  data["entries"] = json::array();
  for (const auto& entry : entries_) {
    data["entries"].push_back({
        {"id", entry.id},
        {"title", entry.title},
        {"path", entry.path},
        {"source", entry.source},
        {"sha256", entry.sha256},
        {"downloaded_at", entry.downloaded_at},
        {"size", entry.size},
        {"mapper", entry.mapper},
        {"mapper_name", entry.mapper_name},
    });
  }
  std::string error;
  write_file_atomic(path_, data.dump(2, ' ', false, json::error_handler_t::replace) + "\n", &error);
}

std::vector<LibraryEntry> LibraryService::list() const {
  std::lock_guard lock(mutex_);
  return entries_;
}

std::optional<LibraryEntry> LibraryService::find(std::string_view id) const {
  std::lock_guard lock(mutex_);
  for (const auto& entry : entries_) {
    if (entry.id == id) return entry;
  }
  return std::nullopt;
}

void LibraryService::add(const LibraryEntry& entry) {
  std::lock_guard lock(mutex_);
  for (auto& existing : entries_) {
    if (existing.id == entry.id) {
      existing = entry;
      save();
      return;
    }
  }
  entries_.push_back(entry);
  save();
}

void LibraryService::remove(std::string_view id) {
  std::lock_guard lock(mutex_);
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                [&](const LibraryEntry& entry) { return entry.id == id; }),
                 entries_.end());
  save();
}

std::size_t LibraryService::size() const {
  std::lock_guard lock(mutex_);
  return entries_.size();
}

}  // namespace nes

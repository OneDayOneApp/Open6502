#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "model.hpp"

namespace nes {

class LibraryService {
 public:
  explicit LibraryService(std::filesystem::path path);

  void load();
  std::vector<LibraryEntry> list() const;
  std::optional<LibraryEntry> find(std::string_view id) const;
  void add(const LibraryEntry& entry);
  void remove(std::string_view id);
  std::size_t size() const;

 private:
  void save() const;

  std::filesystem::path path_;
  mutable std::mutex mutex_;
  std::vector<LibraryEntry> entries_;
};

}  // namespace nes

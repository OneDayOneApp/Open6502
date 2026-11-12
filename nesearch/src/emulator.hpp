#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config.hpp"

namespace nes {

struct EmulatorInfo {
  std::string id;
  std::string name;
  std::string path;
  bool available = false;
};

class EmulatorService {
 public:
  explicit EmulatorService(const Config& config);

  std::vector<EmulatorInfo> list() const;
  bool launch(const std::string& id, const std::filesystem::path& rom, std::string* error) const;

 private:
  std::filesystem::path resolve(const std::string& id) const;

  const Config& config_;
};

}  // namespace nes

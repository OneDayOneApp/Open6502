#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace nes {

class Sha256 {
 public:
  Sha256();
  void update(const void* data, std::size_t length);
  std::string hex();
  static std::string hash(std::string_view data);
  static std::string hash_file(const std::filesystem::path& path, std::string* error = nullptr);

 private:
  void transform(const std::uint8_t* block);
  std::uint32_t state_[8];
  std::uint64_t bit_length_;
  std::uint8_t buffer_[64];
  std::size_t buffer_length_;
};

}  // namespace nes

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace nes {

struct HttpResponse {
  long status = 0;
  std::string body;
  std::string error;
  std::map<std::string, std::string> headers;

  bool ok() const { return status >= 200 && status < 300; }
};

struct DownloadResult {
  bool ok = false;
  long status = 0;
  std::uint64_t bytes = 0;
  std::string error;
};

using ProgressCallback = std::function<void(std::uint64_t received, std::uint64_t total)>;

class HttpClient {
 public:
  HttpClient(std::string proxy, int timeout_seconds);

  HttpResponse get(const std::string& url,
                   const std::vector<std::string>& headers = {}) const;

  DownloadResult download(const std::string& url,
                          const std::filesystem::path& part_path,
                          bool resume,
                          const ProgressCallback& progress,
                          const std::atomic<bool>& cancel) const;

 private:
  std::string proxy_;
  int timeout_seconds_;
};

}  // namespace nes

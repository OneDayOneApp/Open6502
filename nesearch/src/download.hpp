#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "catalog.hpp"
#include "config.hpp"
#include "http.hpp"
#include "library.hpp"
#include "model.hpp"

namespace nes {

class DownloadManager {
 public:
  DownloadManager(CatalogService& catalog, Config& config, LibraryService& library);
  ~DownloadManager();

  void enqueue(const std::vector<std::string>& ids, const std::string& output_dir, bool force);
  std::vector<DownloadInfo> snapshot() const;
  void pause(const std::string& id);
  void resume(const std::string& id);
  void remove(const std::string& id);
  bool has_active() const;

 private:
  struct Task {
    Game game;
    std::filesystem::path output_dir;
    bool force = false;
    std::atomic<bool> cancel{false};
    std::string state = "queued";
    std::string error;
    std::string path;
    std::uint64_t received = 0;
    std::uint64_t total = 0;
    double speed = 0.0;
  };

  void worker();
  void process(const std::shared_ptr<Task>& task);
  void set_state(const std::shared_ptr<Task>& task, const std::string& state);
  std::shared_ptr<Task> find_task(const std::string& id) const;

  CatalogService& catalog_;
  Config& config_;
  LibraryService& library_;
  HttpClient http_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::shared_ptr<Task>> queue_;
  std::vector<std::shared_ptr<Task>> tasks_;
  std::vector<std::thread> workers_;
  bool stopping_ = false;

};

}  // namespace nes

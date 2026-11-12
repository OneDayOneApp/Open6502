#include "download.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include "sha256.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

namespace nes {

namespace {

struct ParsedRom {
  RomInfo info;
  std::uint64_t file_size = 0;
};

bool zip_archive(const Game& game) {
  return ends_with(lower(game.file), ".zip") || ends_with(lower(game.url), ".zip");
}

bool extract_zip_rom(const fs::path& archive, const fs::path& output, std::string* error) {
  const fs::path temp = output.parent_path() / (".extract-" + output.stem().string());
  std::error_code ec;
  fs::remove_all(temp, ec);
  fs::create_directories(temp, ec);
  if (ec) {
    if (error) *error = "cannot create extraction directory: " + ec.message();
    return false;
  }
#ifdef _WIN32
  const std::string command = "tar -xf \"" + archive.string() + "\" -C \"" + temp.string() + "\" >NUL 2>NUL";
#else
  const std::string command = "unzip -o -q \"" + archive.string() + "\" -d \"" + temp.string() + "\" >/dev/null 2>&1";
#endif
  if (std::system(command.c_str()) != 0) {
    fs::remove_all(temp, ec);
    if (error) *error = "cannot extract ZIP archive (unzip/tar failed)";
    return false;
  }

  fs::path rom;
  for (fs::recursive_directory_iterator it(temp, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file() && ends_with(lower(it->path().extension().string()), ".nes")) {
      rom = it->path();
      break;
    }
  }
  if (rom.empty()) {
    fs::remove_all(temp, ec);
    if (error) *error = "ZIP archive contains no .nes file";
    return false;
  }
  fs::remove(output, ec);
  fs::rename(rom, output, ec);
  if (ec) {
    fs::copy_file(rom, output, fs::copy_options::overwrite_existing, ec);
  }
  fs::remove_all(temp, ec);
  if (ec) {
    if (error) *error = "cannot extract ROM: " + ec.message();
    return false;
  }
  return true;
}

std::string mapper_name(int mapper) {
  switch (mapper) {
    case 0: return "NROM";
    case 1: return "MMC1";
    case 2: return "UxROM";
    case 3: return "CNROM";
    case 4: return "MMC3";
    case 5: return "MMC5";
    case 7: return "AxROM";
    case 9: return "MMC2";
    case 10: return "MMC4";
    case 11: return "Color Dreams";
    case 34: return "BNROM";
    case 66: return "GxROM";
    case 69: return "Sunsoft FME-7";
    case 71: return "Camerica";
    case 79: return "NINA";
    default: return "Mapper " + std::to_string(mapper);
  }
}

ParsedRom parse_ines(const fs::path& path) {
  ParsedRom parsed;
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    parsed.info.error = "cannot open file";
    return parsed;
  }
  std::error_code ec;
  parsed.file_size = fs::file_size(path, ec);
  std::uint8_t header[16] = {};
  in.read(reinterpret_cast<char*>(header), sizeof(header));
  if (in.gcount() != 16) {
    parsed.info.error = "file is shorter than an iNES header";
    return parsed;
  }
  if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
    parsed.info.error = "invalid iNES magic";
    return parsed;
  }
  const int prg = header[4];
  const int chr = header[5];
  const int flags6 = header[6];
  const int flags7 = header[7];
  int mapper = (flags7 & 0xF0) | (flags6 >> 4);
  const bool nes2 = (flags7 & 0x0C) == 0x08;
  if (nes2) mapper |= (header[8] & 0x0F) << 8;
  const bool trainer = (flags6 & 0x04) != 0;
  const std::uint64_t expected = 16ULL + static_cast<std::uint64_t>(prg) * 16384ULL +
                                 static_cast<std::uint64_t>(chr) * 8192ULL + (trainer ? 512ULL : 0ULL);
  parsed.info.mapper = mapper;
  parsed.info.mapper_name = mapper_name(mapper);
  parsed.info.prg_kb = prg * 16;
  parsed.info.chr_kb = chr * 8;
  parsed.info.battery = (flags6 & 0x02) != 0;
  parsed.info.trainer = trainer;
  parsed.info.nes2 = nes2;
  if (parsed.file_size < expected) {
    parsed.info.error = "truncated ROM: expected at least " + std::to_string(expected) + " bytes";
    return parsed;
  }
  parsed.info.valid = true;
  return parsed;
}

}  // namespace

DownloadManager::DownloadManager(CatalogService& catalog, Config& config, LibraryService& library)
    : catalog_(catalog),
      config_(config),
      library_(library),
      http_(config.proxy, config.timeout_seconds) {
  const int count = std::clamp(config_.jobs, 1, 4);
  for (int i = 0; i < count; ++i) workers_.emplace_back([this] { worker(); });
}

DownloadManager::~DownloadManager() {
  {
    std::lock_guard lock(mutex_);
    stopping_ = true;
    for (auto& task : tasks_) {
      if (task->state == "queued" || task->state == "downloading") task->cancel = true;
    }
  }
  cv_.notify_all();
  for (auto& worker_thread : workers_) {
    if (worker_thread.joinable()) worker_thread.join();
  }
}

std::shared_ptr<DownloadManager::Task> DownloadManager::find_task(const std::string& id) const {
  for (const auto& task : tasks_) {
    if (task->game.id == id) return task;
  }
  return nullptr;
}

void DownloadManager::enqueue(const std::vector<std::string>& ids, const std::string& output_dir, bool force) {
  const fs::path output = output_dir.empty() ? expand_user(config_.download_dir) : expand_user(output_dir);
  std::string error;
  if (!ensure_dir(output, &error)) throw std::runtime_error(error);

  {
    std::lock_guard lock(mutex_);
    for (const auto& id : ids) {
      const auto game = catalog_.find(id);
      if (!game) {
        auto task = std::make_shared<Task>();
        task->game.id = id;
        task->game.title = id;
        task->state = "failed";
        task->error = "game not found";
        tasks_.push_back(std::move(task));
        continue;
      }

      auto existing = find_task(id);
      if (existing) {
        if (existing->state == "queued" || existing->state == "downloading" || existing->state == "verifying") {
          continue;
        }
        if (existing->state == "done" && !force) continue;
        existing->cancel = false;
        existing->state = "queued";
        existing->error.clear();
        existing->force = force;
        existing->output_dir = output;
        existing->total = 0;
        existing->received = 0;
        queue_.push_back(existing);
        continue;
      }

      auto task = std::make_shared<Task>();
      task->game = *game;
      task->output_dir = output;
      task->force = force;
      tasks_.push_back(task);
      queue_.push_back(task);
    }
  }
  cv_.notify_all();
}

void DownloadManager::worker() {
  while (true) {
    std::shared_ptr<Task> task;
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
      if (stopping_) return;
      task = queue_.front();
      queue_.pop_front();
      if (task->state != "queued") continue;
      task->state = "downloading";
    }

    try {
      process(task);
    } catch (const std::exception& exception) {
      std::lock_guard lock(mutex_);
      task->state = "failed";
      task->error = exception.what();
    }
  }
}

void DownloadManager::process(const std::shared_ptr<Task>& task) {
  const fs::path target = task->output_dir / (task->game.id + ".nes");
  const fs::path part = fs::path(target.string() + ".part");
  const bool archive = zip_archive(task->game);
  const fs::path download_path = archive ? fs::path(part.string() + ".zip") : part;
  std::error_code ec;

  if (fs::exists(target, ec) && !task->force) {
    const ParsedRom parsed = parse_ines(target);
    if (parsed.info.valid) {
      LibraryEntry entry;
      entry.id = task->game.id;
      entry.title = task->game.title;
      entry.path = target.string();
      entry.source = task->game.source;
      entry.size = parsed.file_size;
      entry.mapper = parsed.info.mapper;
      entry.mapper_name = parsed.info.mapper_name;
      entry.downloaded_at = now_iso8601();
      entry.sha256 = Sha256::hash_file(target, nullptr);
      library_.add(entry);
      std::lock_guard lock(mutex_);
      task->state = "done";
      task->path = target.string();
      task->received = parsed.file_size;
      task->total = parsed.file_size;
      task->speed = 0;
      return;
    }
    std::lock_guard lock(mutex_);
    task->state = "failed";
    task->error = "target exists and is not a valid ROM; use --force";
    return;
  }

  if (task->force) {
    // Keep the existing target until the replacement has been verified.
    fs::remove(part, ec);
    fs::remove(download_path, ec);
  }

  auto started = std::chrono::steady_clock::now();
  auto last = started;
  std::uint64_t last_bytes = 0;
  const auto progress = [&](std::uint64_t received, std::uint64_t total) {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - last).count();
    double speed = 0;
    if (elapsed > 0.25) {
      speed = static_cast<double>(received - last_bytes) / elapsed / (1024.0 * 1024.0);
      last = now;
      last_bytes = received;
    }
    std::lock_guard lock(mutex_);
    task->received = received;
    if (total > 0) task->total = total;
    if (speed > 0) task->speed = speed;
  };

  {
    std::lock_guard lock(mutex_);
    task->received = 0;
    task->speed = 0;
    task->total = 0;
    task->path.clear();
  }

  DownloadResult result;
  for (int attempt = 0; attempt < 3; ++attempt) {
    result = http_.download(task->game.url, download_path, !task->force || attempt > 0, progress, task->cancel);
    if (result.ok || task->cancel) break;
    if (result.status == 404 || result.status == 403 || result.status == 401) break;
    std::this_thread::sleep_for(std::chrono::seconds(1 + attempt));
  }
  if (task->cancel) {
    std::lock_guard lock(mutex_);
    task->state = "paused";
    task->error.clear();
    return;
  }
  if (!result.ok) {
    std::lock_guard lock(mutex_);
    task->state = "failed";
    task->error = result.error.empty() ? "download failed" : result.error;
    return;
  }

  if (archive) {
    std::string extract_error;
    if (!extract_zip_rom(download_path, part, &extract_error)) {
      std::lock_guard lock(mutex_);
      task->state = "failed";
      task->error = extract_error;
      return;
    }
    fs::remove(download_path, ec);
  }

  {
    std::lock_guard lock(mutex_);
    task->state = "verifying";
    task->received = result.bytes;
    if (task->total == 0) task->total = result.bytes;
  }

  const ParsedRom parsed = parse_ines(part);
  if (!parsed.info.valid) {
    std::lock_guard lock(mutex_);
    task->state = "failed";
    task->error = parsed.info.error;
    return;
  }

  const std::string sha = Sha256::hash_file(part, nullptr);
  if (!task->game.sha256.empty() && lower(task->game.sha256) != lower(sha)) {
    std::lock_guard lock(mutex_);
    task->state = "failed";
    task->error = "SHA-256 mismatch";
    return;
  }

  if (fs::exists(target, ec)) fs::remove(target, ec);
  fs::rename(part, target, ec);
  if (ec) {
    std::lock_guard lock(mutex_);
    task->state = "failed";
    task->error = "cannot move downloaded file: " + ec.message();
    return;
  }

  LibraryEntry entry;
  entry.id = task->game.id;
  entry.title = task->game.title;
  entry.path = target.string();
  entry.source = task->game.source;
  entry.sha256 = sha;
  entry.size = parsed.file_size;
  entry.mapper = parsed.info.mapper;
  entry.mapper_name = parsed.info.mapper_name;
  entry.downloaded_at = now_iso8601();
  library_.add(entry);

  {
    std::lock_guard lock(mutex_);
    task->state = "done";
    task->path = target.string();
    task->received = parsed.file_size;
    task->total = parsed.file_size;
    task->speed = 0;
    task->error.clear();
  }
}

std::vector<DownloadInfo> DownloadManager::snapshot() const {
  std::lock_guard lock(mutex_);
  std::vector<DownloadInfo> result;
  result.reserve(tasks_.size());
  for (const auto& task : tasks_) {
    if (task->state == "removed") continue;
    DownloadInfo info;
    info.id = task->game.id;
    info.title = task->game.title;
    info.state = task->state;
    info.error = task->error;
    info.path = task->path;
    info.received = task->received;
    info.total = task->total;
    info.speed = task->speed;
    if (task->state == "done") {
      info.progress = 100;
    } else if (task->total > 0) {
      info.progress = static_cast<int>(std::min<std::uint64_t>(100, task->received * 100 / task->total));
    }
    result.push_back(std::move(info));
  }
  return result;
}

void DownloadManager::pause(const std::string& id) {
  std::lock_guard lock(mutex_);
  auto task = find_task(id);
  if (!task) return;
  task->cancel = true;
  if (task->state == "queued") {
    task->state = "paused";
    task->error.clear();
  }
}

void DownloadManager::resume(const std::string& id) {
  std::lock_guard lock(mutex_);
  auto task = find_task(id);
  if (!task) return;
  if (task->state != "paused" && task->state != "failed") return;
  task->cancel = false;
  task->error.clear();
  task->state = "queued";
  queue_.push_back(task);
  cv_.notify_all();
}

void DownloadManager::remove(const std::string& id) {
  std::lock_guard lock(mutex_);
  auto task = find_task(id);
  if (!task) return;
  task->cancel = true;
  task->state = "removed";
  tasks_.erase(std::remove(tasks_.begin(), tasks_.end(), task), tasks_.end());
}

bool DownloadManager::has_active() const {
  std::lock_guard lock(mutex_);
  for (const auto& task : tasks_) {
    if (task->state == "queued" || task->state == "downloading" || task->state == "verifying") return true;
  }
  return false;
}

}  // namespace nes

#include "emulator.hpp"

#include <cstdlib>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace nes {

namespace {

bool executable(const fs::path& path) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) return false;
#ifndef _WIN32
  return ::access(path.string().c_str(), X_OK) == 0;
#else
  return true;
#endif
}

fs::path first_existing(const std::vector<fs::path>& candidates) {
  for (const auto& candidate : candidates) {
    if (executable(candidate)) return candidate;
  }
  return {};
}

}  // namespace

EmulatorService::EmulatorService(const Config& config) : config_(config) {}

fs::path EmulatorService::resolve(const std::string& id) const {
  const char* env_name = id == "gem6502" ? "NESEARCH_GEM6502" : "NESEARCH_NEMU";
  const std::string configured = id == "gem6502" ? config_.gem6502_path : config_.nemu_path;
  const char* env_value = std::getenv(env_name);

  std::vector<fs::path> candidates;
  if (env_value && *env_value) candidates.emplace_back(env_value);
  if (!configured.empty()) candidates.emplace_back(configured);

  const fs::path repo(NESEARCH_REPO_ROOT);
  if (id == "gem6502") {
    candidates.push_back(repo / "GEM6502" / "gem6502");
    candidates.push_back(fs::current_path() / "GEM6502" / "gem6502");
    candidates.push_back(fs::current_path().parent_path() / "GEM6502" / "gem6502");
  } else {
    candidates.push_back(repo / "NEMU" / "build" / "6502EMU");
    candidates.push_back(fs::current_path() / "NEMU" / "build" / "6502EMU");
    candidates.push_back(fs::current_path().parent_path() / "NEMU" / "build" / "6502EMU");
  }
  return first_existing(candidates);
}

std::vector<EmulatorInfo> EmulatorService::list() const {
  std::vector<EmulatorInfo> result;
  EmulatorInfo gem;
  gem.id = "gem6502";
  gem.name = "GEM6502";
  gem.path = resolve("gem6502").string();
  gem.available = !gem.path.empty();
  result.push_back(gem);

  EmulatorInfo nemu;
  nemu.id = "nemu";
  nemu.name = "NEMU / 6502EMU";
  nemu.path = resolve("nemu").string();
  nemu.available = !nemu.path.empty();
  result.push_back(nemu);
  return result;
}

bool EmulatorService::launch(const std::string& id, const fs::path& rom, std::string* error) const {
  const fs::path path = resolve(id);
  if (path.empty()) {
    if (error) *error = id == "gem6502" ? "GEM6502 executable not found" : "NEMU executable not found";
    return false;
  }
  std::error_code ec;
  if (!fs::is_regular_file(rom, ec)) {
    if (error) *error = "ROM not found: " + rom.string();
    return false;
  }

#ifdef _WIN32
  std::string command = "start \"\" \"" + path.string() + "\" ";
  if (id == "gem6502") command += "play ";
  command += "\"" + rom.string() + "\"";
  if (std::system(command.c_str()) != 0) {
    if (error) *error = "failed to start emulator";
    return false;
  }
  return true;
#else
  const pid_t pid = ::fork();
  if (pid < 0) {
    if (error) *error = "fork failed";
    return false;
  }
  if (pid == 0) {
    // Double-fork so the emulator survives the short-lived child and does not
    // become a zombie of the UI server.
    const pid_t grandchild = ::fork();
    if (grandchild < 0) ::_exit(127);
    if (grandchild > 0) ::_exit(0);

    ::setsid();
    const int devnull = ::open("/dev/null", O_RDWR);
    if (devnull >= 0) {
      ::dup2(devnull, STDOUT_FILENO);
      ::dup2(devnull, STDERR_FILENO);
      if (devnull > STDERR_FILENO) ::close(devnull);
    }
    if (id == "gem6502") {
      ::execl(path.string().c_str(), path.string().c_str(), "play", rom.string().c_str(),
              static_cast<char*>(nullptr));
    } else {
      ::execl(path.string().c_str(), path.string().c_str(), rom.string().c_str(),
              static_cast<char*>(nullptr));
    }
    ::_exit(127);
  }
  int status = 0;
  ::waitpid(pid, &status, 0);
  return true;
#endif
}

}  // namespace nes

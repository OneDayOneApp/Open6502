#pragma once

#include <atomic>
#include <string>

#include "catalog.hpp"
#include "config.hpp"
#include "download.hpp"
#include "emulator.hpp"
#include "library.hpp"
#include "http.hpp"

namespace nes {

class UiServer {
 public:
  UiServer(Config& config, CatalogService& catalog, DownloadManager& downloads,
           LibraryService& library, std::string token, std::string ui_dir);

  void run(int port, bool open_browser);

 private:
  Config& config_;
  CatalogService& catalog_;
  DownloadManager& downloads_;
  LibraryService& library_;
  std::string token_;
  std::string ui_dir_;
  EmulatorService emulators_;
};

}  // namespace nes

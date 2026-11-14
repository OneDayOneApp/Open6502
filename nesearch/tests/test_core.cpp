#include <cassert>
#include <filesystem>
#include <iostream>

#include "config.hpp"
#include "sha256.hpp"
#include "util.hpp"

int main() {
  assert(nes::Sha256::hash("abc") ==
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  assert(nes::Sha256::hash("") ==
         "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  assert(nes::slugify("Hello, World!") == "hello-world");
  assert(nes::title_from_slug("super-tilt-bro") == "Super Tilt Bro");
  assert(nes::human_size(40976) == "40 KiB");
  assert(nes::detect_license("Permission is hereby granted under the MIT License") == "MIT");

  // The default catalog includes the my6502 `tinybasic/games` GitHub tree as a
  // scoped sub-directory source next to the retrobrews catalog.
  nes::Config defaults;
  assert(defaults.github_sources.size() == 2);
  const nes::GithubSource& my6502 = defaults.github_sources.back();
  assert(my6502.repo == "redoop/my6502");
  assert(my6502.ref == "tinybasic");
  assert(my6502.path == "games");
  assert(nes::github_source_id(my6502) == "github:redoop/my6502/games");

  // Legacy single-repo configs (`github` object, no `github_sources`) still load.
  const auto legacy_path = std::filesystem::temp_directory_path() / "nesearch-config-legacy.json";
  std::string error;
  assert(nes::write_file_atomic(legacy_path, R"({"github":{"repo":"foo/bar","ref":"dev"}})", &error));
  const nes::Config legacy = nes::Config::load(legacy_path);
  assert(legacy.github_sources.size() == 1);
  assert(legacy.github_sources[0].repo == "foo/bar");
  assert(legacy.github_sources[0].ref == "dev");
  std::filesystem::remove(legacy_path);

  std::cout << "core tests passed\n";
  return 0;
}

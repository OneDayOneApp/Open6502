#include <cassert>
#include <iostream>

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
  std::cout << "core tests passed\n";
  return 0;
}

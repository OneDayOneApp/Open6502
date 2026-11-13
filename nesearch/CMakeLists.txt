cmake_minimum_required(VERSION 3.20)
project(nesearch VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
  execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE NESEARCH_SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NESEARCH_SDK_PATH AND EXISTS "${NESEARCH_SDK_PATH}/usr/include/c++/v1")
    include_directories(SYSTEM "${NESEARCH_SDK_PATH}/usr/include/c++/v1")
  endif()
endif()

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_library(nesearch_core
  src/util.cpp
  src/sha256.cpp
  src/http.cpp
  src/config.cpp
  src/catalog.cpp
  src/library.cpp
  src/download.cpp
  src/ui_server.cpp
  src/emulator.cpp
  src/export.cpp
)
target_include_directories(nesearch_core PUBLIC src third_party)
target_link_libraries(nesearch_core PUBLIC CURL::libcurl Threads::Threads)
if(WIN32)
  target_link_libraries(nesearch_core PUBLIC ws2_32)
endif()
target_compile_definitions(nesearch_core PUBLIC
  NESEARCH_VERSION="${PROJECT_VERSION}"
  NESEARCH_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/ui"
  NESEARCH_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
if (MSVC)
  target_compile_options(nesearch_core PRIVATE /W4)
else()
  target_compile_options(nesearch_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(nesearch src/main.cpp)
target_link_libraries(nesearch PRIVATE nesearch_core)
if (MSVC)
  target_compile_options(nesearch PRIVATE /W4)
else()
  target_compile_options(nesearch PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS nesearch RUNTIME DESTINATION bin)

enable_testing()
add_executable(nesearch_tests tests/test_core.cpp)
target_link_libraries(nesearch_tests PRIVATE nesearch_core)
add_test(NAME core COMMAND nesearch_tests)

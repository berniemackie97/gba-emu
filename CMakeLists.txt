cmake_minimum_required(VERSION 3.25)
project(gba_emu LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Core library
add_library(gba_core
    src/core/bus/bus.cpp
    src/core/cpu/arm7tdmi.cpp
    src/core/mmu/mmu.cpp
)
target_include_directories(gba_core PUBLIC src)
target_compile_definitions(gba_core PUBLIC PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# SDL2 frontend
find_package(SDL2 CONFIG REQUIRED)
add_executable(gba_sdl apps/sdl/main.cpp)
target_link_libraries(gba_sdl PRIVATE gba_core SDL2::SDL2 SDL2::SDL2main)

# Tests
enable_testing()
find_package(GTest CONFIG REQUIRED)
add_executable(core_tests tests/core_smoke.cpp tests/bios_load.cpp)
target_link_libraries(core_tests PRIVATE gba_core GTest::gtest GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(core_tests)


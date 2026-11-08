cmake_minimum_required(VERSION 3.25)
project(gba_emu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(gba_core
    src/core/content/file_image.cpp
    src/core/bus/bus.cpp
    src/core/cpu/arm7tdmi.cpp
    src/core/mmu/mmu.cpp
    src/core/io/io.cpp
)
target_include_directories(gba_core PUBLIC src)
target_compile_definitions(gba_core PUBLIC PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

find_package(SDL2 CONFIG REQUIRED)
add_executable(gba_sdl apps/sdl/main.cpp)
target_link_libraries(gba_sdl PRIVATE gba_core SDL2::SDL2 SDL2::SDL2main)

find_package(GTest CONFIG REQUIRED)
enable_testing()
include(GoogleTest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

foreach(test_src IN LISTS TEST_SOURCES)
    get_filename_component(test_name "${test_src}" NAME_WE)
    add_executable("${test_name}_test" "${test_src}")
    target_link_libraries("${test_name}_test" PRIVATE gba_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests("${test_name}_test")
endforeach()

foreach(scenario IN ITEMS startup invalid-video invalid-arguments)
    add_test(
        NAME "Frontend.${scenario}"
        COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:gba_sdl>"
            "-DSCENARIO=${scenario}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/frontend_smoke.cmake"
    )
    set_tests_properties("Frontend.${scenario}" PROPERTIES LABELS "frontend;smoke" TIMEOUT 30)
endforeach()

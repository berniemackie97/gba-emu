# Building

## Prerequisites (Windows)

- Visual Studio 2022 **C++** workload (Community or Build Tools).
- Windows 10/11 SDK (provides `rc.exe`/`mt.exe`).
- Ninja (via `winget install -e Ninja-build.Ninja`).
- CMake 3.25+.
- vcpkg cloned to `%USERPROFILE%\vcpkg` (bootstrap once).

> The repo ships a **user-local** `CMakeUserPresets.json` generated on first run; it is ignored by Git and pins your local toolchain and build directory.

## Configure, build, test

```powershell
cmake --preset default
cmake --build --preset default -j
ctest --preset default --output-on-failure
```

## Running the SDL app (stub)

When the front-end is wired, an `apps/sdl` target will appear. For now, tests validate the core.

## Notes

- **Do not commit** `CMakeUserPresets.json` or `build/` products.
- New test files under `tests/*.cpp` are auto-picked up by the CMake glob and compiled into per-file test executables. `gtest_discover_tests` handles registration.

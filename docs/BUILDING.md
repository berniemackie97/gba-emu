# Building

The build uses CMake 3.25+, C++20, vcpkg, SDL2 and GoogleTest.

Set `VCPKG_ROOT` to an installed vcpkg checkout. Both presets use the
`$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` toolchain and `vcpkg.json`.
Do not commit machine-specific compiler paths.

## Linux and macOS

```sh
cmake --preset default -DCMAKE_BUILD_TYPE=Debug
cmake --build --preset default --parallel
ctest --preset default --output-on-failure
```

Set `-DCMAKE_BUILD_TYPE=Release` for a Release build.
`./scripts/run_tests.sh --build-type Debug` uses the same preset.

## Windows

Install Visual Studio 2022 and its C++ desktop workload. The `windows`
preset selects the x64 Visual Studio generator without pinned paths.

```powershell
cmake --preset windows
cmake --build --preset windows --config Debug --parallel
ctest --preset windows --build-config Debug --output-on-failure
```

Use `--config Release` / `--build-config Release` to test release builds.
`.\scripts\run_tests.ps1 -BuildType Debug` uses the same preset.

Build output is in `build/default` or `build/windows`. Do not switch
generators in one configured build directory.

GitHub Actions tests Debug and Release on GCC, Clang, MSVC and Apple Clang.
Static analysis, coverage, sanitizers and the aggregate Quality Gate
must also pass.

## Frontend smoke verification

The SDL executable creates a 480 x 320 window and waits for a close event.
It does not yet execute a GBA game or present emulated frames.

```sh
ctest --preset default -L frontend --output-on-failure
```

On Windows:

```powershell
ctest --preset windows --build-config Debug -L frontend --output-on-failure
```

CTest runs the executable using SDL's dummy video driver and an explicit
`--smoke-test` shutdown event, then verifies initialization failure and
invalid-argument exit codes. These tests require no display or BIOS image.

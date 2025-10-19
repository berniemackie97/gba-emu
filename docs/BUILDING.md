# Building

We use CMake presets and vcpkg for dependencies (GoogleTest, SDL2).

## Configure & build

```bash
cmake -S . -B build --preset default
cmake --build --preset default -j
```

## Run tests

```bash
ctest --preset default --output-on-failure
```

## Notes

- Windows: Visual Studio 2022 Community is fine (MSVC toolset).
- macOS/Linux: any recent Clang or GCC works.
- vcpkg is invoked automatically during configure when needed.

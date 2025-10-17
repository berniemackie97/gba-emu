# GBA Emulator (Private)

**Stack:** C++20, CMake, Ninja, vcpkg, SDL2, GoogleTest.

## Quickstart

```powershell
# one-time deps: cmake, ninja, vcpkg; set VCPKG_ROOT
cmake --preset default
cmake --build --preset default -j
ctest --preset default
.\build\gba_sdl.exe

BIOS/ROMs

Place your BIOS at assets/gba_bios.bin. ROMs stay outside the repo or under assets/. These are .gitignored.
Layout

    src/ core emulator (cpu, mmu, bus)

    apps/sdl/ SDL2 frontend

    tests/ GTest unit tests
```

# Testing

Unit tests are organized by subsystem. Each file in `tests/*.cpp` builds into its own test binary
using `gtest_discover_tests`, so new test files are auto‑collected.

## Running

```powershell
cmake --build --preset default -j
ctest --preset default --output-on-failure
```

## Conventions

- No magic numbers: tests define symbols alongside expectations (e.g., `constexpr u16 kValueA1B2 = 0xA1B2;`).  
- Tests read like specifications: comments state the hardware rule being exercised.
- Prefer checking **both** the direct address and its mirror to prove aliasing behavior.

## Current suites

- `bios_load_test` — BIOS load and mapping.  
- `mmu_map_test` — Region boundaries and mirroring for PAL/VRAM/OAM.  
- `mmu_width_test` — 8/16/32‑bit access semantics, including unaligned behavior.  
- `cart_map_test` — Game Pak ROM mapping and write‑ignore behavior.

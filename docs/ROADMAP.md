# Roadmap

## Completed

- Project scaffolding (CMake/Ninja/vcpkg) and presets
- Bus façade delegating to MMU
- MMU: 8/16/32‑bit accesses, mirroring for PAL/VRAM/OAM, open‑bus policy
- BIOS loader (16 KiB), tests
- Game Pak ROM mapping (read‑only) with mirrors across wait‑states and ROM size
- Tests: mapping, width access, and cartridge mapping

## Next (short‑term)

- Typed I/O register block with reset values (DISPCNT, DISPSTAT/VCOUNT, BGxCNT, TMxCNT, KEYINPUT)
- CPU core skeleton (Thumb fetch/decode/execute loop) using Bus APIs
- Minimal PPU Mode 0 renderer (tile BGs) and software framebuffer for SDL
- Cartridge SRAM (8‑bit) save support

## Medium term

- DMA channels 0–3 (basic), timers, IRQ wiring
- CPU: full Thumb, then ARM instruction set and timing
- PPU: additional modes, affine, windowing, blending

## Quality & infrastructure

- CI (build + test), coverage reports
- Trace logging toggles for CPU/PPU/DMA

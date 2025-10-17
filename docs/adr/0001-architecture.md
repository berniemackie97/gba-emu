0001 — Architecture Skeleton

Status: accepted

Context: Private GBA emulator; use real BIOS provided locally (not committed).

Decision: Interpreter-first ARM7TDMI, SDL2 frontend, vcpkg for deps, CMake + Ninja builds.

Consequences: Fast iteration, portable; later can add Thumb, PPU, APU, DMA, IRQ, scheduler, debugger.

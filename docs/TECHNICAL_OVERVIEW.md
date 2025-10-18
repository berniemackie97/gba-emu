# GBA Emulator — Technical Overview

This document summarizes the high‑level design of the emulator as it exists now.
The project prioritizes **clarity and correctness** first; performance tuning
comes later. The hardware reference used throughout is **GBATEK**.

## Feature matrix (current)

- ✅ MMU regions with mirroring (BIOS, EWRAM, IWRAM, I/O, Palette, VRAM, OAM)
- ✅ 8/16/32‑bit accesses, little‑endian; unaligned CPU accesses are allowed
- ✅ Open‑bus default for unmapped reads (0xFF)
- ✅ Game Pak ROM mapping (read‑only), mirrors across wait‑state segments and actual ROM size
- 🧩 I/O register block: stubbed backing array (no side effects yet)
- 🚧 CPU core, PPU, DMA, APU, timers and IRQs: not implemented

## Memory map (CPU view)

| Region         | Base       | Size        | Access             | Notes                                                                                  |
|----------------|------------|-------------|--------------------|----------------------------------------------------------------------------------------|
| BIOS           | 0x00000000 | 0x00004000  | R (8/16/32)        | 16 KiB system ROM. Read‑only. If not loaded → open‑bus.                                |
| EWRAM          | 0x02000000 | 0x00040000  | R/W (8/16/32)      | 256 KiB external work RAM.                                                             |
| IWRAM          | 0x03000000 | 0x00008000  | R/W (8/16/32)      | 32 KiB internal work RAM.                                                              |
| I/O registers  | 0x04000000 | 0x00000400  | R/W (8/16/32)      | Backed by 1 KiB array for now; will be replaced with typed registers.                  |
| Palette (PAL)  | 0x05000000 | 0x00000400  | Prefers 16/32‑bit  | 1 KiB mirrored through the 16 MiB block (0x05000000–0x05FFFFFF).                      |
| VRAM           | 0x06000000 | 0x00018000  | Prefers 16/32‑bit  | 96 KiB; within the first 128 KiB window, the last 32 KiB alias back to the first 32.  |
| OAM            | 0x07000000 | 0x00000400  | Prefers 16/32‑bit  | 1 KiB mirrored through the 16 MiB block (0x07000000–0x07FFFFFF).                      |
| Game Pak ROM   | 0x08000000 | up to 32 MiB| R (8/16/32)        | Three 16 MiB wait‑state segments mirror the same ROM; also mirrors by actual ROM size.|
| Game Pak SRAM  | 0x0E000000 | 0x00010000  | R/W (8‑bit only)   | Not implemented yet.                                                                   |

**Mirroring rules implemented**

- **Palette / OAM:** `offset = (addr - BASE) & 0x3FF` (1 KiB window mirrored across 16 MiB).
- **VRAM (0x06000000–0x0601FFFF):** let `off = addr - BASE`. If `off < 0x18000`, then `offset = off` else
  `offset = off - 0x18000` (alias last 32 KiB back to first 32 KiB).
- **Game Pak ROM:** `offset = (addr - 0x08000000) % rom_size_bytes`. This covers both wait‑state segment mirrors and
  natural power‑of‑two mirroring of smaller ROMs.

**Alignment and width**

ARM7TDMI allows unaligned 16/32‑bit loads; the CPU rotates bytes when needed.
The MMU exposes `read8/16/32` and `write8/16/32`; no implicit alignment is enforced.

## Source layout

- `src/core/mmu/` — Address decoding, mirroring rules, BIOS loader, width helpers.
- `src/core/bus/` — CPU‑facing façade delegating to MMU (and later to PPU/APU/etc.).
- `tests/` — GoogleTest units; each `tests/*.cpp` is built to a dedicated test exe.
- `docs/` — This documentation set.

## Invariants

- No “magic numbers” in code or tests: every constant has a named symbol.
- Tests document behavior; production code mirrors the intent with comments and named helpers.

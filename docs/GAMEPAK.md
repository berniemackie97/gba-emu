# Game Pak (Cartridge) Mapping

This document explains how the emulator maps Game Pak ROM and (later) SRAM/Flash.

## Address ranges

- **ROM wait‑state segments (all mirror the same ROM):**
  - 0x08000000–0x09FFFFFF — Wait‑state 0
  - 0x0A000000–0x0BFFFFFF — Wait‑state 1
  - 0x0C000000–0x0DFFFFFF — Wait‑state 2
- **SRAM**: 0x0E000000–0x0E00FFFF (64 KiB max, 8‑bit bus). Not implemented yet.

## ROM mirroring rules

Let `rom_size_bytes` be the actual size of the loaded ROM. A CPU read at `addr` resolves to:

```
offset = (addr - 0x08000000) % rom_size_bytes
byte    = rom[offset]
```

This simultaneously handles:
- **Wait‑state mirrors** across the three 16 MiB segments.
- **Natural mirroring** of ROMs smaller than a segment (e.g., 4 MiB ROM repeats 4× in a 16 MiB window).

Writes to ROM are ignored.

## SRAM/Flash (future)

- 8‑bit wide; mapped at 0x0E000000–0x0E00FFFF.
- Different save types (SRAM, Flash, EEPROM) require distinct handling and may not share exactly the
  same address region. We will begin with plain SRAM and follow up with Flash/EEPROM when needed by games.

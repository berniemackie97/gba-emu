# MMU — Addressing and Access Semantics

This document describes the MMU implementation: how addresses map to backing storage,
what mirroring rules apply, and how 8/16/32‑bit accesses are performed.

## Constants (single source of truth)

```
BIOS_BASE = 0x00000000; BIOS_SIZE = 0x00004000;  // 16 KiB
EWRAM_BASE = 0x02000000; EWRAM_SIZE = 0x00040000; // 256 KiB
IWRAM_BASE = 0x03000000; IWRAM_SIZE = 0x00008000; // 32 KiB
IO_BASE   = 0x04000000; IO_SIZE   = 0x00000400;   // 1 KiB registers
PAL_BASE  = 0x05000000; PAL_SIZE  = 0x00000400;   // 1 KiB (mirrored)
VRAM_BASE = 0x06000000; VRAM_SIZE = 0x00018000;   // 96 KiB
OAM_BASE  = 0x07000000; OAM_SIZE  = 0x00000400;   // 1 KiB (mirrored)
kWindow16MiB = 0x01000000;     // 16 MiB region window
kVRAMWindow128KiB = 0x00020000;// 128 KiB window (0x06000000–0x0601FFFF)
kVRAMTailBytes = kVRAMWindow128KiB - VRAM_SIZE; // 32 KiB alias tail
kOpenBus = 0xFF;
```

## Address helpers

- `in(addr, base, size)` — true if `base <= addr < base + size`.
- `in_window(addr, base, window)` — true if `addr` lies inside `[base, base+window)`.
- `pal_offset(addr)` and `oam_offset(addr)` — `(addr - BASE) & (SIZE - 1)` (1 KiB wrap).
- `vram_offset(addr)` — within the first 128 KiB window, alias last 32 KiB back:
  ```c++
  const u32 off = addr - VRAM_BASE;
  return (off < VRAM_SIZE) ? off : (off - static_cast<u32>(VRAM_SIZE));
  ```

## Widths and endianness

- The CPU is little‑endian. `read16/read32` compose bytes as:
  ```c++
  u16 = low | (high << 8);
  u32 = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
  ```
- Unaligned accesses are allowed and implemented by composing adjacent bytes.
- Writes split the value into bytes and forward to `write8` at consecutive addresses.

## Open‑bus policy

Reads from unmapped regions return `0xFF` for now. This can be refined once we model bus state.

## I/O registers

For bring‑up, I/O is backed by a 1 KiB array. When we introduce typed registers, the MMU will
route reads/writes to those register implementations. Reset values and side effects will live
in that layer, not in the MMU.

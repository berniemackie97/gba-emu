# MMU

The Memory Management Unit implements the GBA’s flat 32‑bit address map and
mirroring rules for BIOS, IWRAM/EWRAM, VRAM, PAL, and OAM, plus IO and the
GamePak bus. It is intentionally simple and side‑effect free unless a mapped
device says otherwise.

---

## Address map (high level)

- `0x00000000` – BIOS (read‑only once mapped)
- `0x02000000` – EWRAM
- `0x03000000` – IWRAM
- `0x04000000` – IO registers
- `0x05000000` – PAL
- `0x06000000` – VRAM
- `0x07000000` – OAM
- `0x08000000` – GamePak (wait states, mirrors)

Each region applies the GBA’s documented mirroring rule. The test suite asserts
key mirroring/width properties.

---

## Width and alignment

The MMU composes values from bytes and supports byte/half/word widths.

> **CPU note:** For **unaligned word** LDR/STR, ARM7TDMI’s architected behavior
> is implemented at the CPU layer (read/write aligned word then rotate by
> `8 * (addr & 3)`). Direct `MMU::read32(addr)` callers will see a raw 32‑bit
> value composed from `addr..addr+3` without rotation.

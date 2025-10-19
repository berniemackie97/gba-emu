# Bus

The Bus is a thin, deterministic front door to the GBA memory map. It exposes
little‑endian byte/half/word access and delegates to the MMU and attached
devices. It does **not** implement CPU‑specific quirks such as the ARM7TDMI’s
unaligned word rotation — that is done in the CPU layer.

---

## Responsibilities

- Provide `read8/16/32` and `write8/16/32`
- Route requests to mapped regions (BIOS, IWRAM, VRAM, PAL, OAM, IO regs,
  cartridge bus/wait states)
- Remain boring: no implicit side effects besides what the region implements

## Semantics

- All accesses are little‑endian
- Word accesses are aligned by the MMU as required by the backing array/device
- **Unaligned access policy:** the Bus/MMU perform **no** architectural rotation.
  When the CPU executes `LDR/STR (word)` to an unaligned address, it reads/writes
  the aligned word and applies the rotate itself. This mirrors ARM7TDMI behavior
  and keeps the Bus simple and testable.

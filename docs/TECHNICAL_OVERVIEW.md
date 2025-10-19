# Technical Overview

This repository is a learning-focused, test-first Game Boy Advance emulator.
The design is intentionally modular and explicit about architectural boundaries:
Bus ↔ MMU ↔ Devices, with a small, well-tested CPU core driving everything.

---

## Feature matrix

- ✅ **CPU (Thumb, interpreter subset)**  
  Implemented instructions: `MOV (imm8)`, `ADD (imm8)`, `SUB (imm8)`, `B (imm11)`,
  `LDR (literal)`, `LDR/STR (word, imm5)`.
  The CPU maintains `N,Z,C,V` flags where defined and assumes `T` (Thumb) set.
  Unaligned **word** semantics for LDR/STR are handled at the CPU layer
  (rotate by `8 * (addr & 3)`).
  Covered by unit tests:  
  `CPUThumb.MovAddSubAndBranch`, `CPUThumbLoadStore.LdrLiteralThenStoreAndLoadWord`.

- ✅ **MMU / Bus**  
  Flat address space mapping with IWRAM, VRAM, PAL, OAM, IO regs, BIOS, and
  cartridge wait-state regions. Bus provides raw little‑endian byte/half/word
  access and does **not** perform CPU architectural rotations.

- 🚧 **PPU / APU / DMA / Timers / IRQ / Keypad / Serial**  
  Not implemented yet; IO register shells exist where needed for tests.

---

## Alignment and width

The CPU and MMU are little‑endian. The Bus reads/writes bytes (`8`), halfwords
(`16`), and words (`32`).

**Unaligned word accesses** are allowed by ARM7TDMI. In this project:

- The **CPU** performs the *architectural* behavior for unaligned `LDR/STR (word)`:
  read/write at the **aligned** address and then rotate the 32‑bit value
  by `8 * (addr & 3)` (right on load; the inverse on store).
- The **Bus/MMU** remain simple and only expose raw aligned reads/writes.

This division mirrors ARM7TDMI’s programmer’s model while keeping the Bus simple
and testable.

---

## Tests

All code paths are driven by small, focused GoogleTest suites. The current
suite covers CPU arithmetic, branching, literal pools, and MMU width/mirroring
behaviors. Run the suite with:

```bash
ctest --preset default --output-on-failure
```


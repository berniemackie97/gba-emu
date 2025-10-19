# CPU – ARM7TDMI (Thumb subset)

This document describes the currently implemented portion of the ARM7TDMI core
used by the GBA emulator. The focus is **functional correctness first**, with a
straightforward interpreter and tight unit tests.

---

## Registers & flags

- 16 general registers `r0..r15` with `r15==PC`
- `CPSR` flags used so far: `N` (31), `Z` (30), `C` (29), `V` (28); `T` (5) is
  assumed set (Thumb state)
- Program counter in Thumb is kept **halfword aligned** (bit0 cleared)

## Implemented instructions (Thumb‑16)

- Data‑processing (immediate): `MOV (imm8)`, `ADD (imm8)`, `SUB (imm8)`
- Branch: `B (imm11)` (PC‑relative)
- Loads/stores:
  - `LDR (literal)` — base = `(PC_old + 4)` aligned to 4, then `+ imm8*4`
  - `LDR (word, imm5)`, `STR (word, imm5)` — `[Rb + imm5*4]`

## Flags

- `set_nz(result)` clears/sets `N` and `Z`
- `set_add_cv(a,b,r)` sets `C` on unsigned overflow; `V` when signs of `a` and
  `b` are equal and sign of `r` differs
- `set_sub_cv(a,b,r)` sets `C` as **not‑borrow** (`a >= b`); `V` when signs of
  `a` and `b` differ and sign of `r` differs from `a`

## Unaligned word semantics (architectural)

ARM7TDMI allows unaligned word LDR/STR. The architecture specifies that loads
read the **aligned** word then rotate right by `8 * (addr & 3)`; stores perform
the inverse (rotate left by the same amount, then write aligned).

In this emulator:
- The **CPU** implements that rotate behavior:
  ```c++
  struct RotRight { explicit constexpr RotRight(std::uint8_t n) : n(n) {} std::uint8_t n; };
  static constexpr auto rotr(u32 value, RotRight amt) noexcept -> u32;
  // read_u32_unaligned(addr) = rotr(mmio.read32(addr & ~3), RotRight{8*(addr&3)})
  ```
- The **Bus/MMU** do not rotate; they provide raw little‑endian access.

## PC used by `LDR (literal)`

When executing `LDR Rd, [PC, #imm8*4]` in Thumb state, the base is the address
of the **current** instruction plus 4 **then** aligned to 4. The test
`CPUThumbLoadStore.LdrLiteralThenStoreAndLoadWord` encodes the literal pool
immediately after the four‑instruction sequence and verifies the behavior.

## Tests

- `CPUThumb.MovAddSubAndBranch` — arithmetic and control flow sanity checks
- `CPUThumbLoadStore.LdrLiteralThenStoreAndLoadWord` — literal pool + store/load
  and unaligned rotation path through the CPU helpers

## Future work

- Logical ops and shifts; comparisons; more load/store forms (byte/half, sign‑extend)
- High‑register ops, `BX`, and entry into ARM state
- Optional timing/pipeline model (separate from functional core)

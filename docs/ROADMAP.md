# Roadmap

We keep the roadmap incremental and test-driven. Ship small, ship often.

---

## Done / Milestones

- **CPU (Thumb) bring-up**
  - MOV/ADD/SUB (imm8), B (imm11)
  - LDR (literal), LDR/STR (word, imm5)
  - NZCV flags (where defined); Thumb state assumed
  - Unaligned word semantics implemented at the CPU layer
  - Unit tests: `CPUThumb.MovAddSubAndBranch` and
    `CPUThumbLoadStore.LdrLiteralThenStoreAndLoadWord`

---

## Next

- More Thumb ALU ops (logical and shifts), comparisons, condition flags polish
- LDR/STR variants: byte/halfword; sign-extending loads
- High-register ops and `BX` (path to ARM state switching)
- Broaden CPU test matrix; add negative/edge cases for flags
- Timing/pipeline model (separate from functional correctness)
- IO: timers, DMA scaffolding, and more IO-reg tests
- PPU slice-by-scanline simulation harness (start with mode 3 smoke tests)

---

## Later / Maybe

- ARM (32‑bit) execution core, switch-on-`BX`
- Save states; debugger; tracing hooks
- Audio processing and sync loop

# Coding Guidelines

These are short, practical rules that keep the codebase easy to read,
refactor, and test.

---

## General

- Prefer **small, testable units** over large objects
- Use `constexpr` and named constants instead of magic numbers
- Name things for what they **mean**, not how they’re used

## Bit‑twiddling & encodings

- Introduce named masks and shifts (e.g. `kTop5Shift`, `kLow3Mask`, `kImm5Mask`)
- In tests, write top‑5 opcode bits in binary for readability (e.g. `0b01001U`)
  but hide them behind helpers: `Thumb_LDR_literal(...)` etc.

## Flags and arithmetic

- Keep flag calculations (`N,Z,C,V`) in helpers (`set_nz`, `set_add_cv`, `set_sub_cv`)
- For addition/subtraction carry/overflow, prefer explicit 64‑bit widening or
  bit‑identities with small comments

## “Easily‑swappable parameters” lint

- Avoid pairs of same‑typed adjacent parameters when order matters. Wrap with a
  tiny strong type:
  ```c++
  struct RotRight { explicit constexpr RotRight(std::uint8_t n) : n(n) {} std::uint8_t n; };
  static constexpr auto rotr(u32 v, RotRight a) noexcept -> u32;
  ```
- If an external signature must remain `(address, value)` (e.g. Bus writes),
  add a local, one‑line suppression:
  ```c++
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — matches bus API
  ```

## Tests

- Keep tests **narrow** (one behavior per test) and **self‑documenting** with
  small encoder helpers and named constants.

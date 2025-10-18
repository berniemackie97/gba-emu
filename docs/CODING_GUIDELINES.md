# Coding Guidelines

These rules keep the codebase readable and warning‑free.

## General

- No magic numbers. Introduce named constants in the most local scope that makes sense.
- Self‑documenting names: `addr`, `base`, `offset`, `windowBytes` instead of single letters.
- Prefer `constexpr` functions and constants when possible.

## Safety

- Use `std::array::at()` during bring‑up for bounds checks.
- Prefer `<algorithm>`/`<ranges>` over manual loops when it improves clarity.

## Comments

- Every function has a short summary line.
- Non‑obvious hardware behavior (e.g., VRAM aliasing) gets a rule comment near the code and in docs.

## Warnings

- Keep the code free of compiler and static‑analysis warnings.
- When a tool flags an issue (e.g., “magic number”), introduce a named constant or helper.

# Contributing

## Engineering contract

Every change must have a clear observable outcome, an issue with bounded acceptance criteria, and tests derived independently from the implementation. Preserve correctness across CPU state, memory access widths, device events and timing boundaries.

Before coding, inspect the affected source and existing tests. Establish the invariant, identify the source of truth, and plan ownership and failure behavior. Prefer a focused implementation over parallel abstractions, duplicate decoders or speculative interfaces.

A review is not complete until the implementation is re-audited for undefined behavior, error paths, resource bounds, accidental duplication and documentation drift. Keep comments for hardware contracts or non-obvious decisions, not narration of ordinary code.

## Validation

Use the CMake presets and CTest commands documented in README after the build repair tracked in [#8](https://github.com/berniemackie97/gba-emu/issues/8). Until that issue is complete, current repository build commands and CI must not be assumed to work.

Tests must exercise success, failure, boundaries and regression scenarios. Unsupported instructions and asset formats must not silently succeed. Do not describe a subsystem as complete or cycle-accurate without independent evidence.

## Pull requests

Link the corresponding issue; explain the architectural effect, precise changes, verification results and residual limitations. Prefer cohesive slices that can be reviewed and proven together. Required CI must pass and actionable review threads must be resolved before merge.

Do not commit third-party BIOS images, game cartridges, proprietary assets or files with unclear redistribution rights.

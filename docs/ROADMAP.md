# Delivery roadmap

The [GitHub issues](https://github.com/berniemackie97/gba-emu/issues) are the authoritative planning and acceptance record. Release stages describe verified capability gates, not calendar promises. Stage completion requires all tracked children to meet their acceptance tests and required CI to pass.

| Stage | Gate | Epic |
| --- | --- | --- |
| R0 | Reproducible build, enforced CI, working startup and governance | [#1](https://github.com/berniemackie97/gba-emu/issues/1) |
| R1 | CPU instruction state and exception behavior independently verified | [#2](https://github.com/berniemackie97/gba-emu/issues/2) |
| R2 | Memory aliases, access-width effects, bus devices and cartridge safety verified | [#3](https://github.com/berniemackie97/gba-emu/issues/3) |
| R3 | Deterministic machine clock, DMA, IRQ, timers and keypad verified | [#4](https://github.com/berniemackie97/gba-emu/issues/4) |
| R4 | Image-tested rendering and persistent SDL presentation | [#5](https://github.com/berniemackie97/gba-emu/issues/5) |
| R5 | Tested audio output, backup storage and state continuity | [#6](https://github.com/berniemackie97/gba-emu/issues/6) |
| R6 | Legal ROM conformance, measurable performance and repeatable releases | [#7](https://github.com/berniemackie97/gba-emu/issues/7) |

## Current verified baseline

The implementation contains a Thumb CPU subset, basic memory mapping, three display I/O registers and GoogleTest suites. It does not implement a functional ARM execution path, scheduler, PPU, DMA, timers, IRQ controller, APU or save-memory hardware. The SDL entry point currently exits before window creation.

The latest recorded CI run failed. CMake preset and workflow inconsistencies, CPU branch semantics, PUSH/POP decoding, flag handling, VRAM/EWRAM/IWRAM aliases and input loading safety have dedicated blocking issues. See [#8](https://github.com/berniemackie97/gba-emu/issues/8), [#9](https://github.com/berniemackie97/gba-emu/issues/9), [#14](https://github.com/berniemackie97/gba-emu/issues/14), [#15](https://github.com/berniemackie97/gba-emu/issues/15) and [#22](https://github.com/berniemackie97/gba-emu/issues/22).

Work across stages can proceed in parallel only where explicit issue dependencies allow it. Do not count an implemented handler as a complete hardware feature without independent integration evidence.

# Security policy

The project is in early development and does not currently publish supported security-maintenance versions.

For potential vulnerabilities, avoid posting an exploit or sensitive reproduction publicly. Use GitHub private vulnerability reporting when it is enabled for this repository, or contact the maintainer privately through their GitHub profile.

For non-sensitive functional or correctness defects, open a normal GitHub issue with a minimal reproducible test.

Untrusted inputs include BIOS and cartridge images, save data, state files and user-supplied configuration. A parser failure must not cause unbounded resource consumption, memory corruption or silent partial replacement of valid data. Relevant hardening work is tracked in [#28](https://github.com/berniemackie97/gba-emu/issues/28) and [#52](https://github.com/berniemackie97/gba-emu/issues/52).

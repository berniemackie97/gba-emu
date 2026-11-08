# CPU-visible memory map

The MMU provides storage and address decoding for BIOS, internal RAM, the
display memory regions, I/O registers and cartridge ROM. The CPU's alignment
and unaligned word-load rotation are handled separately.

## Memory region aliases

| Address window | Physical storage | Mapping |
| --- | --- | --- |
| `0x02000000–0x02FFFFFF` | 256 KiB EWRAM | Every 256 KiB |
| `0x03000000–0x03FFFFFF` | 32 KiB IWRAM | Every 32 KiB |
| `0x05000000–0x05FFFFFF` | 1 KiB palette | Every 1 KiB |
| `0x06000000–0x06FFFFFF` | 96 KiB VRAM | 128 KiB repeating bank |
| `0x07000000–0x07FFFFFF` | 1 KiB OAM | Every 1 KiB |

Each mirror resolves to the same backing array, not a separate allocation.
VRAM offsets `0x00000–0x17FFF` map directly to physical storage. Offsets
`0x18000–0x1FFFF` alias `0x10000–0x17FFF`. The 128 KiB pattern repeats
through the entire 16 MiB VRAM window. BIOS and the standard I/O window
do not use the 16 MiB mirroring rule.

## Access width

The mapped 8-bit write behavior depends on the destination:

- EWRAM and IWRAM: update the addressed byte.
- Palette: duplicate the byte into both lanes of the addressed halfword.
- Background VRAM: duplicate the byte into both halfword lanes. In display
  modes 0–2 this covers physical offsets below `0x10000`; in modes 3–5
  it covers offsets below `0x14000`.
- Object VRAM and OAM: ignore byte writes.
- BIOS and cartridge ROM: ignore writes.

The rules use the *physical* VRAM offset after mirror decoding. A byte write
through the `0x18000` mirror therefore affects or ignores the same bytes
as its `0x10000` source according to the active display mode.

Direct 16-bit and 32-bit writes write their component bytes unchanged;
they do not apply the special byte-store replication rules. The MMU reads
and writes little-endian values by byte for width composition. The CPU
interprets unaligned word transfers; device-specific timing, IRQ behavior,
open-bus data and additional register ownership remain tracked separately.

## I/O reset

Reset clears the DISPCNT register, writable DISPSTAT enables and compare
value, VCOUNT and HBlank state. The live VCOUNT-equals-compare status bit
reads as set immediately after reset because both values are zero. Attempts
to write the read-only status flags do not change them.

## Verification and references

The `mmu_map`, `mmu_width` and `io_regs` test suites assert mirrored
reads/writes, 16 MiB region endpoints, 8/16/32-bit behavior, display-mode
boundaries and deterministic repeated resets.

Mapping and byte-write semantics:
[GBATEK, GBA Memory Map](https://mgba-emu.github.io/gbatek/) and
[gbadev documentation](https://github.com/gbadev-org/gbadoc/blob/main/content/overview.md).

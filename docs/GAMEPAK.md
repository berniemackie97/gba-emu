# Game Pak image loading and addressing

## File-backed asset policy

A BIOS file is accepted only when it is a readable regular file exactly
16 KiB long. A Game Pak file must be a readable regular file between
192 bytes and 32 MiB, inclusive.

A Game Pak file additionally requires the header fixed byte at offset
`0xB2` to equal `0x96`, and the header complement byte at `0xBD` to match
the sum specified in the cartridge header format: subtract bytes
`0xA0` through `0xBC` from zero, then subtract `0x19`, modulo 256.
The validator extracts the 12-byte title, 4-byte game code, 2-byte maker
code and one-byte software version without interpreting the text encoding.
It does **not** verify the Nintendo logo or implement BIOS boot
authentication. A passing header check is not proof that a cartridge
is authentic or its code will execute.

A file of any length within the documented bounds may be accepted,
including non-power-of-two lengths. There is no invented requirement to
pad the file to a power of two or discard trailing bytes. Every byte from
the source is preserved unchanged, including padding already present.

The in-memory `load_gamepak(span)` overload is a raw program-image
interface for generated instruction tests. It accepts a nonempty image
up to 32 MiB without checking a cartridge header. A successful raw load
does not expose file-cartridge metadata.

## Ownership and replacement

The reader checks file type and size before allocating, then reads into
a temporary bounded buffer. Metadata is checked before the buffer becomes
CPU-visible. A failure leaves the previous ROM bytes **and** metadata
intact. A successful file load replaces both. A successful raw load
replaces bytes and clears metadata. Reset unloads the bytes and clears
metadata. `MMU::cartridge_header()` reports the parsed metadata only
for a successfully loaded file image.

Use the optional `ImageLoadError` output of `load_gamepak` and
`image_load_error_message()` for actionable failure diagnostics,
including `InvalidHeader`. A failed load does not terminate the process
or corrupt the previous image.

## CPU-visible ROM windows

All three wait-state regions select the same cartridge image at the same
offset, but have separate timing policies to be implemented later:

| CPU address window | Image byte offset |
| --- | --- |
| `0x08000000–0x09FFFFFF` (WS0) | Address minus `0x08000000` |
| `0x0A000000–0x0BFFFFFF` (WS1) | Address minus `0x0A000000` |
| `0x0C000000–0x0DFFFFFF` (WS2) | Address minus `0x0C000000` |

Unlike work RAM, ordinary cartridge ROM is not generically mirrored by
the loaded file length. Reads at valid image offsets use the exact file
byte; writes are ignored. The MMU currently returns its provisional
`0xFF` unmapped value beyond the available image bytes. This is an
explicit incomplete bus policy, **not** a hardware-accurate open-bus
implementation. Handling absent-cartridge bus data, BIOS visibility,
cartridge-specific address lines, EEPROM ranges and access timing belongs
to issues #27 and the later device/scheduler work.

The SDL frontend currently opens a window but cannot launch a ROM.
Loading and address access are exercised through the core tests.

## Verification

`tests/cartridge_header.cpp` uses a fully generated 192-byte header
with independently calculated complement `0xBF`. It checks metadata,
invalid fixed/check bytes, preserved state and successful replacement.
`tests/cart_map.cpp` checks the three windows, exact last byte of a
32 MiB file, non-power-of-two files without invented modulo aliases,
and reset/raw-load lifecycle. No proprietary BIOS or ROM fixture is
required.

Reference: [GBATEK — GBA Cartridge Header and ROM](https://mgba-emu.github.io/gbatek/).

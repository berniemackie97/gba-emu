# Game Pak image loading and address mapping

## Asset input policy

- A BIOS file must be a regular, readable file of exactly 16 KiB.
- A Game Pak file must be a regular, readable file between 192 bytes (the
  header extent) and 32 MiB, inclusive. This is a structural size check,
  not a cartridge-header authenticity or compatibility validator.
- In-memory Game Pak images may be shorter than the file-header minimum,
  allowing generated instruction fixtures. They must be nonempty and no
  larger than 32 MiB.
- The file loader checks size before allocating, reads the entire expected
  image, and rejects changes or short reads detected during loading.
  Data is installed only after successful validation and read. A rejected
  replacement leaves any previously loaded BIOS or Game Pak unchanged.
- `Bus::load_bios` and both `Bus::load_gamepak` overloads return success
  or failure. Pass an `ImageLoadError` pointer for the reason and use
  `image_load_error_message` for a caller-visible diagnostic.

The SDL frontend does not currently expose asset-loading CLI options or
execute cartridge programs. CPU-visible access and file-validation behavior
are tested independently.

## ROM address windows

The current bus maps the same loaded Game Pak data through three 32 MiB
read-only windows:

| Address range | Wait-state region |
| --- | --- |
| `0x08000000`–`0x09FFFFFF` | WS0 |
| `0x0A000000`–`0x0BFFFFFF` | WS1 |
| `0x0C000000`–`0x0DFFFFFF` | WS2 |

Within a window, the current implementation takes the offset modulo the
loaded data length. Writes are ignored. Empty/unloaded windows return the
current unmapped byte value.

This describes the present implementation; hardware-correct ROM aliasing
and cartridge metadata are separate work tracked in issue #29. Backup
memory (SRAM, Flash, EEPROM) is not implemented.

# Device register ownership

CPU-visible I/O occupies 0x04000000–0x040003FF. Every access uses an
offset relative to that base. `IORegs::owner(offset)` identifies the device
that owns each documented range; unsupported ranges do not masquerade
as writable RAM.

| Offset range | Owner | Current behavior |
| --- | --- | --- |
| 0x000–0x055 | Display | DISPCNT, DISPSTAT and VCOUNT implemented |
| 0x060–0x0A7 | Audio | Not implemented |
| 0x0B0–0x0DF | DMA | Not implemented |
| 0x100–0x10F | Timers | Not implemented |
| 0x120–0x12B, 0x134–0x159 | Serial | Not implemented |
| 0x130–0x133 | Keypad | KEYINPUT and KEYCNT register behavior |
| 0x200–0x203, 0x208–0x209 | Interrupt | IE, IF and IME register behavior |
| 0x204–0x205 | Wait control | WAITCNT storage and writable mask |
| 0x300–0x301 | System | Not implemented |
| Other offsets | Unmapped | No register state |

Unimplemented and reserved offsets currently discard writes and return
the provisional `0xFF` value. This **is not** a claim that real hardware
returns 0xFF for every unmapped register; open-bus behavior is tracked by
issue #27. In particular, display registers other than DISPCNT, DISPSTAT
and VCOUNT are classified as display-owned but do not yet have storage
or side effects. DMA, timers and audio are explicit integration boundaries
for their existing R3/R5 issues.

## Implemented register semantics

- DISPCNT preserves the current 16-bit storage contract. DISPSTAT
  preserves writable IRQ enables and VCOUNT compare, while status bits
  0–2 reflect live VBlank/HBlank/VCOUNT state. VCOUNT cannot be written.
- KEYINPUT at 0x130 is active-low, with 10 available button bits;
  no buttons pressed reads `0x03FF`. KEYINPUT is read-only.
  KEYCNT at 0x132 stores selected buttons and bits 14–15; bits 10–13
  are discarded. The keypad IRQ trigger is not connected yet.
- IE at 0x200 accepts interrupt enable bits 0–13. IF at 0x202
  records device interrupt requests and clears each requested bit
  when software writes a 1. IME at 0x208 stores only bit 0.
  The pending condition is IME && (IE & IF). CPU IRQ entry and
  interrupt generation remain outstanding.
- WAITCNT at 0x204 preserves writable bits selected by mask
  `0x5FFF`; bit 13 is unused and bit 15 is read-only.
  This does not implement cartridge timing or prefetch.

Byte accesses update only their corresponding register lane. Halfword
operations transfer low then high bytes; word operations transfer the
lower halfword then the upper halfword. That order matters for IF's
write-one-to-clear behavior, including unaligned writes that cross
register boundaries. The MMU routes CPU-visible 16/32-bit I/O accesses
through the same ordered device operations.

Reset clears all writable register state, restores KEYINPUT to
`0x03FF` and resets the display counters/flags. The VCOUNT comparison
flag reads set immediately after reset because both VCOUNT and the
comparison value are zero.

## Verification

`tests/io_device_contract.cpp` covers ownership boundaries, reserved
register writes, keypad state and mask, IE/IF/IME ordering, WAITCNT
masking, unaligned writes and MMU-level access. Existing
`tests/io_regs.cpp` exercises DISPCNT, DISPSTAT and VCOUNT.

Source reference: [GBATEK I/O Map, Keypad, Interrupt and System
Control](https://mgba-emu.github.io/gbatek/). Issue #26 tracks the
remaining device implementations. Issue #27 owns accurate open-bus
values, BIOS visibility and wait-state timing.

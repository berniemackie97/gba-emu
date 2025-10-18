# Bus — CPU‑Facing Façade

The Bus provides the interface the CPU core will use to access memory‑mapped resources.
Currently it forwards to the MMU. Later it will mediate access to PPU/APU/DMA/Timers and
handle side‑effects of I/O registers.

## Responsibilities

- Expose 8/16/32‑bit read/write for the CPU.
- Provide BIOS loading/reset hooks for the front‑end.
- Keep MMU pure: address decoding and data storage only.

## API (current)

```c++
class Bus {
public:
  void reset() noexcept;
  bool load_bios(const std::filesystem::path& file) noexcept;

  u8  read8(u32 addr)  const noexcept;
  u16 read16(u32 addr) const noexcept;
  u32 read32(u32 addr) const noexcept;

  void write8(u32 addr, u8 value) noexcept;
  void write16(u32 addr, u16 value) noexcept;
  void write32(u32 addr, u32 value) noexcept;
};
```

As PPU and I/O mature, Bus will dispatch to those subsystems explicitly (e.g. `ppu.read_vcount()`
for `0x04000006`). The MMU will no longer back I/O with a raw array.

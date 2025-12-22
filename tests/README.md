# Test Suite Documentation

This directory contains comprehensive unit and integration tests for the GBA emulator, organized by subsystem and hardware component.

## Test Organization

### CPU Tests

#### `cpu_thumb_smoke.cpp`
Basic smoke tests for Thumb-mode CPU functionality:
- MOV/ADD/SUB immediate instructions
- Branch (B) instruction
- Basic program flow

#### `cpu_thumb_loadstore.cpp`
Load/store operations in Thumb mode:
- LDR literal (PC-relative loads)
- LDR/STR word with immediate offset
- Unaligned word access semantics

#### `cpu_thumb_loadstore_byte.cpp`
Byte-width load/store operations:
- LDRB/STRB with immediate offset
- Byte access to various memory regions

#### `cpu_thumb_alu.cpp` **[NEW]**
Comprehensive ALU operations testing:
- **Shift Operations (Format 1)**:
  - LSL (Logical Shift Left) with immediate
  - LSR (Logical Shift Right) with immediate
  - ASR (Arithmetic Shift Right) with immediate
- **Logical Operations (Format 4)**:
  - AND (bitwise AND)
  - EOR (bitwise XOR)
  - ORR (bitwise OR)
  - BIC (bit clear)
  - MVN (bitwise NOT)
- **Comparison Operations**:
  - CMP with immediate (equality, less than, greater than)

#### `cpu_flags.cpp` **[NEW]**
CPSR flag behavior validation:
- **Zero Flag (Z)**: Set/clear on zero/non-zero results
- **Negative Flag (N)**: Set/clear based on result sign bit
- **Carry Flag (C)**: Unsigned overflow in addition, borrow in subtraction
- **Overflow Flag (V)**: Signed overflow detection
- **Combined Flag Tests**: Multiple operations and flag preservation

### Memory Tests

#### `mmu_map.cpp`
Memory mapping and region boundaries:
- EWRAM read/write and boundaries
- VRAM mirroring (last 32 KiB mirrors first 32 KiB)
- Palette RAM mirroring (1 KiB across 16 MiB)
- OAM mirroring (1 KiB across 16 MiB)
- BIOS read-only behavior and open bus

#### `mmu_width.cpp`
Access width semantics (8/16/32-bit):
- Byte, halfword, and word accesses
- Unaligned access behavior
- Little-endian byte ordering

#### `bios_load.cpp`
BIOS loading functionality:
- BIOS file loading from filesystem
- BIOS region mapping
- Read-only enforcement

#### `cart_map.cpp`
Game Pak cartridge mapping:
- ROM mapping across wait state regions (WS0/WS1/WS2)
- Write-ignore behavior (ROM is read-only)
- Mirroring within 32 MiB windows

### I/O Tests

#### `io_regs.cpp`
I/O register functionality:
- DISPCNT read/write (8-bit and 16-bit access)
- VCOUNT read-only system-driven behavior
- DISPSTAT flags and LYC compare logic
- VBlank/HBlank flag behavior

#### `core_smoke.cpp`
Basic integration smoke test:
- CPU + Bus + MMU interaction
- End-to-end functionality validation

## Test Conventions

### No Magic Numbers
All tests define named constants for values and expectations:
```cpp
constexpr u8 kTestValue = 0x42U;
constexpr u32 kExpectedResult = 0x1234U;
```

### Hardware Spec Comments
Tests include comments stating the hardware rule being exercised:
```cpp
// GBA hardware: VRAM last 32 KiB mirrors first 32 KiB
EXPECT_EQ(mmu.read8(vram_base), mmu.read8(vram_alias));
```

### Mirror Testing
Tests verify both direct addresses and their mirrors:
```cpp
mmu.write8(pal_base, kValue);
EXPECT_EQ(mmu.read8(pal_base), kValue);
EXPECT_EQ(mmu.read8(pal_alias), kValue);  // Verify mirror
```

### Instruction Encoding Helpers
Each test file defines encoding helper functions that self-document the instruction format:
```cpp
constexpr auto Thumb_MOV_imm(u8 destReg, u8 imm8) -> u16 {
    return (kTop5_MOV << kThumbTop5Shift) |
           ((destReg & kLow3Mask) << kRegFieldShift) | imm8;
}
```

## Running Tests

### Build and Run All Tests
```powershell
cmake --build --preset default -j
ctest --preset default --output-on-failure
```

### Run Specific Test Suite
```powershell
.\build\Debug\cpu_thumb_alu_test.exe
.\build\Debug\cpu_flags_test.exe
```

### Run with Verbose Output
```powershell
ctest --preset default --output-on-failure --verbose
```

### Run Specific Test Cases
```powershell
.\build\Debug\cpu_flags_test.exe --gtest_filter=CPUFlags.ZeroFlagSetOnZeroResult
```

## Coverage Goals

### Current Coverage (Implemented)
- ✅ Thumb MOV/ADD/SUB immediate (Format 3)
- ✅ Thumb branch (Format 18)
- ✅ Thumb LDR/STR word and byte (Format 7, 9)
- ✅ Thumb ALU operations (Format 1, 4, 5) **[NEW]**
- ✅ CPSR flags (N, Z, C, V) comprehensive testing **[NEW]**
- ✅ MMU region mapping and mirroring
- ✅ Basic I/O register functionality

### Planned Coverage (Next Phase)
- ⏳ Thumb high-register operations (Format 5: ADD/CMP/MOV high regs)
- ⏳ Thumb BX instruction (mode switching)
- ⏳ Thumb push/pop (Format 14)
- ⏳ Thumb multiple load/store (Format 15)
- ⏳ ARM mode (32-bit) instruction execution
- ⏳ Conditional execution based on flags
- ⏳ DMA controller functionality
- ⏳ Timer operations
- ⏳ IRQ/interrupt controller
- ⏳ PPU rendering (Mode 3, 4, 5, sprites)
- ⏳ APU audio generation

### Future Coverage (Integration & Validation)
- 🔜 Integration tests with real GBA test ROMs
- 🔜 Known-good ROM validation (homebrew demos)
- 🔜 Timing accuracy tests
- 🔜 Cycle-accurate behavior validation
- 🔜 Performance benchmarks

## Test ROM Integration

The emulator will eventually run standardized GBA test ROMs:
- **AGS Aging Cartridge**: Official Nintendo hardware test
- **mGBA test suite**: Comprehensive community test ROM
- **gba-tests by jsmolka**: ARM/Thumb instruction tests
- **tonc-demos**: Well-documented homebrew examples

These will be added to a separate `tests/roms/` directory with automated validation.

## Continuous Integration

GitHub Actions CI runs the full test suite on:
- Windows (MSVC, x64)
- Ubuntu (GCC, x64)
- macOS (Clang, x64)

See `.github/workflows/ci.yml` for configuration.

## References

Test implementations follow these specifications:
- [GBATEK](https://problemkaputt.de/gbatek.htm) - Martin Korth's GBA technical reference
- [ARM7TDMI Data Sheet](http://datasheets.chipdb.org/ARM/arm7tdmi.pdf) - Official ARM documentation
- [no$gba](https://problemkaputt.de/gba.htm) - Debugging emulator with excellent documentation
- [mGBA](https://mgba.io/) - Modern, accurate GBA emulator (reference implementation)

## Contributing Test Cases

When adding new tests:
1. Follow the naming convention: `component_feature.cpp`
2. Use named constants instead of magic numbers
3. Add comments referencing the hardware specification
4. Test both success and edge cases
5. Verify mirroring behavior where applicable
6. Update this README with new test coverage

Example test structure:
```cpp
TEST(ComponentName, FeatureDescription) {
    // Setup
    Bus bus;
    bus.reset();

    // Define named constants
    constexpr u32 kTestAddress = MMU::IWRAM_BASE;
    constexpr u8 kTestValue = 0x42U;

    // Execute operation
    bus.write8(kTestAddress, kTestValue);

    // Verify behavior matches hardware spec
    // Comment: GBA spec section X.Y states...
    EXPECT_EQ(bus.read8(kTestAddress), kTestValue);
}
```

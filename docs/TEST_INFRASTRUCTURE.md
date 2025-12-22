# Test Infrastructure

This document describes the testing infrastructure, methodology, and best practices for the GBA emulator project.

## Overview

The project follows a **test-driven development** (TDD) approach with comprehensive unit tests covering all hardware subsystems. The test suite validates behavior against official GBA hardware specifications.

## Test Framework

**GoogleTest (GTest)**: Industry-standard C++ testing framework
- Version: Latest from vcpkg
- Documentation: https://google.github.io/googletest/
- Features: Fixtures, parameterized tests, death tests, mocking

## Test Organization

### Directory Structure
```
tests/
├── README.md                      # Test suite documentation
├── cpu_thumb_smoke.cpp            # Basic CPU smoke tests
├── cpu_thumb_alu.cpp              # ALU operations (NEW)
├── cpu_thumb_loadstore.cpp        # Load/store word operations
├── cpu_thumb_loadstore_byte.cpp   # Load/store byte operations
├── cpu_flags.cpp                  # CPSR flag behavior (NEW)
├── mmu_map.cpp                    # Memory mapping tests
├── mmu_width.cpp                  # Access width tests
├── bios_load.cpp                  # BIOS loading tests
├── cart_map.cpp                   # Cartridge mapping tests
├── io_regs.cpp                    # I/O register tests
├── core_smoke.cpp                 # Integration smoke test
└── roms/                          # Test ROM directory (future)
    ├── README.md
    └── [test ROMs here]
```

### Naming Conventions

**Test Files**: `<component>_<feature>.cpp`
- Examples: `cpu_thumb_alu.cpp`, `mmu_map.cpp`, `io_regs.cpp`

**Test Suites**: `<ComponentName>`
- Examples: `CPUThumbALU`, `MMUMap`, `IORegs`

**Test Cases**: `<FeatureDescription>`
- Use descriptive names: `LogicalShiftLeft`, `BitwiseAND`, `CompareImmediateEqual`

**Example**:
```cpp
TEST(CPUThumbALU, LogicalShiftLeft) {
    // Test implementation
}
```

## Test Development Standards

### 1. No Magic Numbers

Always define named constants:
```cpp
// ❌ BAD
EXPECT_EQ(cpu.debug_reg(0), 0x42);

// ✅ GOOD
constexpr std::uint8_t kExpectedValue = 0x42U;
EXPECT_EQ(cpu.debug_reg(0), kExpectedValue);
```

### 2. Hardware Specification Comments

Include comments referencing the hardware spec:
```cpp
// ARM7TDMI spec: LDR (literal) base = (PC + 4) aligned to 4
const auto base = (cpu.debug_pc() + 4U) & ~0x3U;
```

### 3. Test Both Success and Edge Cases

```cpp
TEST(MMUMap, VRAMMirroring) {
    // Test normal VRAM access
    mmu.write8(VRAM_BASE, kValue);
    EXPECT_EQ(mmu.read8(VRAM_BASE), kValue);

    // Test mirrored access (edge case)
    EXPECT_EQ(mmu.read8(VRAM_BASE + VRAM_SIZE), kValue);
}
```

### 4. Instruction Encoding Helpers

Define encoding functions for readability:
```cpp
namespace {
    constexpr auto Thumb_MOV_imm(u8 destReg, u8 imm8) -> u16 {
        return static_cast<u16>((kTop5_MOV << kThumbTop5Shift) |
                                ((destReg & kLow3Mask) << kRegFieldShift) |
                                imm8);
    }
}

TEST(CPUThumb, MovInstruction) {
    const auto insn = Thumb_MOV_imm(0U, 0x42U);  // Self-documenting
    bus.write16(addr, insn);
}
```

### 5. Arrange-Act-Assert Pattern

```cpp
TEST(Example, Feature) {
    // Arrange: Set up test environment
    Bus bus;
    bus.reset();
    constexpr u32 kTestAddr = MMU::IWRAM_BASE;

    // Act: Execute the operation
    bus.write8(kTestAddr, kTestValue);

    // Assert: Verify expected behavior
    EXPECT_EQ(bus.read8(kTestAddr), kTestValue);
}
```

## Running Tests

### Local Development

```powershell
# Configure
cmake --preset default

# Build
cmake --build --preset default -j

# Run all tests
ctest --preset default --output-on-failure

# Run specific suite
.\build\Debug\cpu_thumb_alu_test.exe

# Run with filter
.\build\Debug\cpu_flags_test.exe --gtest_filter=CPUFlags.ZeroFlagSetOnZeroResult

# Run with verbose output
ctest --preset default --output-on-failure --verbose
```

### Continuous Integration

GitHub Actions automatically runs tests on:
- **Windows**: MSVC (Debug + Release)
- **Linux**: GCC-12 and Clang-15 (Debug + Release)
- **macOS**: Apple Clang (Debug + Release)

Additional CI jobs:
- **Static Analysis**: clang-tidy on all source files
- **Code Coverage**: lcov with Codecov upload
- **Sanitizers**: AddressSanitizer, UndefinedBehaviorSanitizer

## Test Coverage Tracking

### Current Coverage

#### CPU (Thumb Mode)
- ✅ MOV/ADD/SUB immediate (Format 3)
- ✅ Branch (B) with immediate offset (Format 18)
- ✅ LDR literal, LDR/STR word/byte (Format 7, 9)
- ✅ Shift operations: LSL, LSR, ASR (Format 1)
- ✅ ALU operations: AND, EOR, ORR, BIC, MVN (Format 4)
- ✅ Comparison: CMP immediate (Format 5)
- ✅ CPSR flags: N, Z, C, V behavior

#### Memory Management
- ✅ Region mapping: BIOS, EWRAM, IWRAM, IO, PAL, VRAM, OAM
- ✅ Mirroring: VRAM (32 KiB), PAL (1 KiB), OAM (1 KiB)
- ✅ Access widths: 8-bit, 16-bit, 32-bit
- ✅ Cartridge mapping across wait states
- ✅ BIOS read-only enforcement

#### I/O Registers
- ✅ DISPCNT read/write
- ✅ VCOUNT read-only behavior
- ✅ DISPSTAT flags and LYC compare

### Missing Coverage (To Be Implemented)

#### CPU (Thumb Mode)
- ⏳ High-register operations: ADD/CMP/MOV (Format 5)
- ⏳ BX instruction (mode switching)
- ⏳ Push/Pop (Format 14)
- ⏳ Multiple load/store: LDMIA, STMIA (Format 15)
- ⏳ Conditional branches: BEQ, BNE, BCS, etc. (Format 16)
- ⏳ Software interrupt: SWI (Format 17)
- ⏳ ADD/SUB register (Format 2)

#### CPU (ARM Mode)
- ⏳ Data processing instructions
- ⏳ Load/store word and byte
- ⏳ Load/store multiple
- ⏳ Branch and exchange (BX)
- ⏳ Multiply and multiply-accumulate
- ⏳ Software interrupt (SWI)

#### Hardware Subsystems
- ⏳ **DMA** (4 channels)
  - Memory-to-memory transfer
  - Immediate, VBlank, HBlank, FIFO triggers
  - Word/halfword transfer modes
  - Source/dest address control

- ⏳ **Timers** (4 timers)
  - Prescaler settings
  - Count-up mode
  - IRQ generation on overflow
  - Cascading timers

- ⏳ **Interrupts**
  - IRQ controller (IME, IE, IF registers)
  - Interrupt priorities
  - VBlank, HBlank, VCount interrupts
  - Timer, DMA, Serial, Keypad interrupts

- ⏳ **PPU (Picture Processing Unit)**
  - Mode 0: Tiled backgrounds
  - Mode 3: 240x160 bitmap
  - Mode 4: 240x160 8-bit palette
  - Mode 5: 160x128 bitmap
  - Sprite rendering (OAM)
  - Scrolling and rotation/scaling
  - Window effects
  - Blending and special effects

- ⏳ **APU (Audio Processing Unit)**
  - Channel 1: Square wave with sweep
  - Channel 2: Square wave
  - Channel 3: Programmable wave
  - Channel 4: Noise
  - DMA sound (Direct Sound A/B)

- ⏳ **Input**
  - Button state reading (KEYINPUT)
  - Key interrupts (KEYCNT)

## Integration Testing with Test ROMs

### Planned Test ROM Integration

The following test ROMs will be integrated for validation:

#### 1. **mGBA Test Suite**
- Source: https://github.com/mgba-emu/suite
- Coverage: Comprehensive CPU, memory, I/O validation
- Format: Automated pass/fail via serial output

#### 2. **gba-tests by jsmolka**
- Source: https://github.com/jsmolka/gba-tests
- Coverage: ARM/Thumb instruction accuracy
- Format: Visual + automated verification

#### 3. **AGS Aging Cartridge**
- Official Nintendo hardware diagnostic
- Coverage: Full hardware validation
- Format: Interactive test suite

#### 4. **TONC Demos**
- Source: https://www.coranac.com/tonc/text/toc.htm
- Coverage: Well-documented homebrew examples
- Format: Visual verification

### Test ROM Directory Structure

```
tests/roms/
├── README.md                  # Documentation and sources
├── mgba-suite/
│   ├── suite.gba
│   └── expected_output.txt
├── jsmolka/
│   ├── arm.gba
│   ├── thumb.gba
│   └── memory.gba
├── tonc/
│   ├── first.gba
│   ├── brin_demo.gba
│   └── ...
└── validation/
    └── test_rom_runner.cpp    # Automated ROM test runner
```

### Automated ROM Testing

```cpp
// Example: tests/roms/validation/test_rom_runner.cpp
TEST(IntegrationROM, MGBASuite) {
    Emulator emu;
    emu.load_rom("tests/roms/mgba-suite/suite.gba");

    // Run until test completion signal
    while (!emu.test_complete()) {
        emu.step_frame();
    }

    // Verify output matches expected results
    const auto output = emu.get_serial_output();
    const auto expected = load_file("tests/roms/mgba-suite/expected_output.txt");
    EXPECT_EQ(output, expected);
}
```

## Performance and Benchmarking

### Benchmark Tests (Future)

```cpp
TEST(Performance, FrameRenderingSpeed) {
    Emulator emu;
    emu.load_rom("tests/roms/tonc/first.gba");

    const auto start = std::chrono::high_resolution_clock::now();

    // Render 60 frames (1 second at 60 FPS)
    for (int i = 0; i < 60; ++i) {
        emu.step_frame();
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration<double>(end - start).count();

    // Should complete faster than real-time
    EXPECT_LT(duration, 1.0);
}
```

## Debugging Failed Tests

### Visual Studio Code Integration

Add to `.vscode/settings.json`:
```json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureOnOpen": true,
    "testMate.cpp.test.executables": "build/**/*_test.exe"
}
```

### GDB/LLDB Debugging

```bash
# Run test under debugger
gdb --args ./build/cpu_thumb_alu_test --gtest_filter=CPUThumbALU.LogicalShiftLeft
```

### Common Issues

**Issue**: Test fails with wrong register value
```
Expected: cpu.debug_reg(0) == 0x42
  Actual: 0x00
```
**Solution**: Check instruction encoding, verify CPU step count

**Issue**: CPSR flags incorrect
```
Expected: Z flag set
  Actual: Z flag clear
```
**Solution**: Verify flag update logic in CPU, check reference spec

**Issue**: Memory access returns wrong value
```
Expected: mmu.read8(addr) == 0xAA
  Actual: 0xFF (open bus)
```
**Solution**: Check address mapping, verify region bounds

## Best Practices Summary

1. **Write tests first**: Define expected behavior before implementation
2. **Name clearly**: Test names should describe what's being validated
3. **Test edge cases**: Boundaries, overflows, invalid inputs
4. **Verify against spec**: Reference GBATEK, ARM7TDMI docs
5. **Keep tests focused**: One concept per test case
6. **Use fixtures for setup**: Reduce duplication
7. **Document assumptions**: Comment why certain values are expected
8. **Run tests frequently**: Catch regressions early
9. **Maintain CI green**: Fix failing tests immediately
10. **Update docs**: Keep test documentation in sync with code

## Resources

- [GBATEK](https://problemkaputt.de/gbatek.htm) - Comprehensive GBA technical reference
- [ARM7TDMI Technical Manual](http://datasheets.chipdb.org/ARM/arm7tdmi.pdf) - Official CPU documentation
- [GoogleTest Primer](https://google.github.io/googletest/primer.html) - Testing framework guide
- [no$gba Debugging](https://problemkaputt.de/gba.htm) - Reference debugger
- [mGBA](https://mgba.io/) - Accurate reference emulator
- [TONC](https://www.coranac.com/tonc/text/toc.htm) - GBA programming tutorials

## Contributing

When adding new tests:
1. Follow the naming conventions
2. Use the AAA pattern (Arrange-Act-Assert)
3. Add hardware spec comments
4. Test success and edge cases
5. Update coverage documentation
6. Ensure CI passes
7. Update this document if adding new test categories

For questions or issues with the test infrastructure, open a GitHub issue with the `testing` label.

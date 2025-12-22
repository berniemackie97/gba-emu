# GBA Emulator

[![CI](https://github.com/YOUR_USERNAME/gba-emu/workflows/CI/badge.svg)](https://github.com/YOUR_USERNAME/gba-emu/actions)
[![codecov](https://codecov.io/gh/YOUR_USERNAME/gba-emu/branch/main/graph/badge.svg)](https://codecov.io/gh/YOUR_USERNAME/gba-emu)

A test-driven, cycle-accurate Game Boy Advance emulator written in modern C++20.

**Stack:** C++20, CMake, Ninja, vcpkg, SDL2, GoogleTest

## Status

**Early Development** - Currently implements:
- ✅ Thumb CPU subset (MOV/ADD/SUB, ALU ops, shifts, branches, load/store)
- ✅ Complete memory management (all regions with proper mirroring)
- ✅ Basic I/O registers (DISPCNT, VCOUNT, DISPSTAT)
- ✅ Comprehensive test suite (11 test files, 50+ test cases)
- ⏳ ARM mode (not yet implemented)
- ⏳ PPU, DMA, Timers, IRQ, APU (not yet implemented)

See [docs/PROJECT_ASSESSMENT.md](docs/PROJECT_ASSESSMENT.md) for detailed status.

## Quickstart

### Prerequisites
- **CMake** 3.25+
- **Ninja** build system
- **vcpkg** (set `VCPKG_ROOT` environment variable)
- **C++20 compiler** (MSVC 2022, GCC 12+, Clang 15+)

### Build and Test

```powershell
# Windows (PowerShell)
cmake --preset default
cmake --build --preset default -j
ctest --preset default --output-on-failure

# Or use the test runner script
.\scripts\run_tests.ps1 -BuildType Debug -Verbose
```

```bash
# Linux/macOS
cmake -B build -G Ninja
cmake --build build -j
ctest --test-dir build --output-on-failure

# Or use the test runner script
./scripts/run_tests.sh --build-type Debug --verbose
```

### Run the Emulator

```powershell
# Windows
.\build\Debug\gba_sdl.exe

# Linux/macOS
./build/gba_sdl
```

**Note:** Place your GBA BIOS at `assets/gba_bios.bin`. ROMs go in `assets/` or outside the repo (both are `.gitignored`).

## Project Structure

```
gba-emu/
├── src/
│   └── core/              # Core emulator components
│       ├── cpu/           # ARM7TDMI CPU (Thumb subset)
│       ├── mmu/           # Memory Management Unit
│       ├── bus/           # System bus
│       └── io/            # I/O registers
├── apps/
│   └── sdl/               # SDL2 frontend
├── tests/                 # GoogleTest unit tests (11 test files)
├── docs/                  # Documentation
│   ├── BUILDING.md        # Build instructions
│   ├── TESTING.md         # Testing conventions
│   ├── TEST_INFRASTRUCTURE.md  # Testing guide
│   ├── PROJECT_ASSESSMENT.md   # Project status and assessment
│   ├── TECHNICAL_OVERVIEW.md   # Architecture details
│   ├── CPU.md             # CPU implementation
│   ├── MMU.md             # Memory management
│   └── ROADMAP.md         # Development roadmap
├── scripts/               # Build and test scripts
│   ├── run_tests.ps1      # Windows test runner
│   └── run_tests.sh       # Linux/macOS test runner
└── .github/
    └── workflows/
        └── ci.yml         # GitHub Actions CI/CD
```

## Testing

This project follows a **test-driven development** approach with comprehensive unit tests.

### Test Suite

- **11 test files**, 50+ test cases
- Full coverage of implemented features
- Hardware spec-driven validation
- No magic numbers (all constants named)

**Test Files:**
- `cpu_thumb_smoke.cpp` - Basic CPU functionality
- `cpu_thumb_alu.cpp` - ALU operations (shifts, logical ops)
- `cpu_thumb_loadstore.cpp` - Word load/store
- `cpu_thumb_loadstore_byte.cpp` - Byte load/store
- `cpu_flags.cpp` - CPSR flag behavior (N, Z, C, V)
- `mmu_map.cpp` - Memory mapping and mirroring
- `mmu_width.cpp` - Access width semantics
- `bios_load.cpp` - BIOS loading
- `cart_map.cpp` - Cartridge mapping
- `io_regs.cpp` - I/O register functionality
- `core_smoke.cpp` - Integration tests

### Run Tests

```powershell
# Run all tests
ctest --preset default --output-on-failure

# Run specific test suite
.\build\Debug\cpu_thumb_alu_test.exe

# Run with filter
.\build\Debug\cpu_flags_test.exe --gtest_filter=CPUFlags.ZeroFlagSetOnZeroResult
```

See [docs/TEST_INFRASTRUCTURE.md](docs/TEST_INFRASTRUCTURE.md) for comprehensive testing guide.

## Documentation

| Document | Description |
|----------|-------------|
| [BUILDING.md](docs/BUILDING.md) | Build instructions and dependencies |
| [TESTING.md](docs/TESTING.md) | Testing conventions and practices |
| [TEST_INFRASTRUCTURE.md](docs/TEST_INFRASTRUCTURE.md) | Comprehensive testing guide |
| [PROJECT_ASSESSMENT.md](docs/PROJECT_ASSESSMENT.md) | Project status and assessment |
| [TECHNICAL_OVERVIEW.md](docs/TECHNICAL_OVERVIEW.md) | Architecture and design |
| [CPU.md](docs/CPU.md) | CPU implementation details |
| [MMU.md](docs/MMU.md) | Memory management |
| [ROADMAP.md](docs/ROADMAP.md) | Development roadmap |
| [CODING_GUIDELINES.md](docs/CODING_GUIDELINES.md) | Code standards |

## CI/CD

GitHub Actions automatically runs:
- **Multi-platform builds**: Windows (MSVC), Linux (GCC + Clang), macOS (Clang)
- **Build configurations**: Debug and Release
- **Static analysis**: clang-tidy
- **Code coverage**: lcov with Codecov upload
- **Sanitizers**: AddressSanitizer, UndefinedBehaviorSanitizer

See [.github/workflows/ci.yml](.github/workflows/ci.yml) for details.

## References

- [GBATEK](https://problemkaputt.de/gbatek.htm) - Comprehensive GBA technical reference
- [ARM7TDMI Data Sheet](http://datasheets.chipdb.org/ARM/arm7tdmi.pdf) - Official CPU documentation
- [no$gba](https://problemkaputt.de/gba.htm) - Reference debugger
- [mGBA](https://mgba.io/) - Modern, accurate reference emulator
- [TONC](https://www.coranac.com/tonc/text/toc.htm) - GBA programming tutorials

## License

Private project - All rights reserved

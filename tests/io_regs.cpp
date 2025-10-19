// tests/io_regs.cpp
#include <gtest/gtest.h>
#include <limits>
#include "core/mmu/mmu.h"
#include "core/io/io.h"

using gba::IORegs;
using gba::MMU;

namespace {
    // canonical values used by tests (no magic)
    constexpr std::uint16_t kValue16 = 0x1234U;
    constexpr std::uint8_t kLowByte = static_cast<std::uint8_t>(kValue16 & 0xFFU);
    constexpr std::uint8_t kHighByte = static_cast<std::uint8_t>((kValue16 >> 8U) & 0xFFU);
    constexpr std::uint16_t kScanline = 100U; // arbitrary, within 0..227 typical range
    constexpr std::uint16_t kAllOnes16 = std::numeric_limits<std::uint16_t>::max();
    // DISPSTAT flags
    constexpr std::uint16_t kFlagVBlank = static_cast<std::uint16_t>(1U << 0);
    constexpr std::uint16_t kFlagHBlank = static_cast<std::uint16_t>(1U << 1);
    constexpr std::uint16_t kFlagVCount = static_cast<std::uint16_t>(1U << 2);
} // namespace

TEST(IORegs, DispcntReadWrite16AndBytes) {
    MMU mmu;
    mmu.reset();

    const std::uint32_t dispcntAddr = MMU::IO_BASE + IORegs::kOffDISPCNT;
    mmu.write16(dispcntAddr, kValue16);

    EXPECT_EQ(mmu.read16(dispcntAddr), kValue16);
    EXPECT_EQ(mmu.read8(dispcntAddr + 0U), kLowByte);
    EXPECT_EQ(mmu.read8(dispcntAddr + 1U), kHighByte);
}

TEST(IORegs, VcountIsReadOnlyAndSystemDriven) {
    MMU mmu;
    mmu.reset();

    const std::uint32_t vcountAddr = MMU::IO_BASE + IORegs::kOffVCOUNT;

    // System/PPU sets VCOUNT; user writes are ignored.
    mmu.debug_set_vcount_for_tests(kScanline);
    EXPECT_EQ(mmu.read16(vcountAddr), kScanline);

    mmu.write16(vcountAddr, kAllOnes16); // ignored
    EXPECT_EQ(mmu.read16(vcountAddr), kScanline);
}

TEST(IORegs, DispstatFlagsAndLycCompare) {
    MMU mmu;
    mmu.reset();

    const std::uint32_t dispstatAddr = MMU::IO_BASE + IORegs::kOffDISPSTAT;

    // Set LYC to kScanline (bits 8..15). IRQ enable bits don’t affect flags showing up.
    const auto dispstatWithLyc = static_cast<std::uint16_t>(kScanline << IORegs::kBitsPerByte);
    mmu.write16(dispstatAddr, dispstatWithLyc);

    // VCOUNT compare flag toggles when VCOUNT == LYC
    mmu.debug_set_vcount_for_tests(static_cast<std::uint16_t>(kScanline - 1U));
    EXPECT_EQ(mmu.read16(dispstatAddr) & kFlagVCount, 0U);

    mmu.debug_set_vcount_for_tests(kScanline);
    EXPECT_NE(mmu.read16(dispstatAddr) & kFlagVCount, 0U);

    // VBlank flag: set when VCOUNT >= 160
    mmu.debug_set_vcount_for_tests(IORegs::kVisibleLines);
    EXPECT_NE(mmu.read16(dispstatAddr) & kFlagVBlank, 0U);

    // HBlank flag: controlled by system; we flip it via the debug hook
    mmu.debug_set_hblank_for_tests(true);
    EXPECT_NE(mmu.read16(dispstatAddr) & kFlagHBlank, 0U);

    mmu.debug_set_hblank_for_tests(false);
    EXPECT_EQ(mmu.read16(dispstatAddr) & kFlagHBlank, 0U);
}

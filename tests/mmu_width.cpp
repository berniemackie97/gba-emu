// tests/mmu_width.cpp
#include <gtest/gtest.h>
#include <cstdint>
#include "core/mmu/mmu.h"

using gba::MMU;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

namespace {
    // Byte/bit helpers (so we never sprinkle 8/16/24 around)
    constexpr u32 kByteBits = 8U;
    constexpr u32 kHalfBits = 16U;
    constexpr u32 kThreeBytes = 24U;
    constexpr u32 kByteMask = 0xFFU;

    // Named data patterns for clarity
    constexpr u16 kWord1234 = 0x1234U;
    constexpr u32 kDword01203040 = 0x0120'3040U;
    constexpr u16 kWordA1B2 = 0xA1B2U;
    constexpr u16 kWord4555 = 0x4555U;
    constexpr u16 kWord6677 = 0x6677U;

    // Derived expectations (avoid raw hex when asserting)
    constexpr u8 kDword01203040_b0 = static_cast<u8>((kDword01203040 >> 0) & kByteMask);         // 0x40
    constexpr u8 kDword01203040_b1 = static_cast<u8>((kDword01203040 >> kByteBits) & kByteMask); // 0x30
    constexpr u16 kDword01203040_lo16_from_plus1 = static_cast<u16>(
        static_cast<u16>(kDword01203040_b0) | (static_cast<u16>(kDword01203040_b1) << kByteBits)); // 0x3040

    // Region-relative offsets expressed in terms of sizes
    constexpr u32 kPalLastHalfwordOffset = static_cast<u32>(MMU::PAL_SIZE - 2U); // 0x3FE
    constexpr u32 kOamSmallHalfwordOffset = 0x20U; // arbitrary, inside 1 KiB; give it a name

    // 16-bit open-bus value derived from the 8-bit open-bus constant
    constexpr u16 kOpenBus16 =
        static_cast<u16>(static_cast<u16>(MMU::kOpenBus) | (static_cast<u16>(MMU::kOpenBus) << kByteBits));
} // namespace

TEST(MMUWidth, Ewram16And32ReadsWrites) {
    MMU mmu;
    mmu.reset();

    const u32 a16 = MMU::EWRAM_BASE + 2U;
    const u32 a32 = MMU::EWRAM_BASE + 4U;

    mmu.write16(a16, kWord1234);
    EXPECT_EQ(mmu.read16(a16), kWord1234);

    mmu.write32(a32, kDword01203040);
    EXPECT_EQ(mmu.read32(a32), kDword01203040);

    // Unaligned write; CPU would handle rotation semantics—MMU just composes bytes LE.
    mmu.write32(a32 + 1U, kDword01203040);
    EXPECT_EQ(mmu.read8(a32 + 1U), kDword01203040_b0);
    EXPECT_EQ(mmu.read16(a32 + 1U), kDword01203040_lo16_from_plus1);
    EXPECT_EQ(mmu.read32(a32 + 1U), kDword01203040);
}

TEST(MMUWidth, Vram16WriteMirrors) {
    MMU mmu;
    mmu.reset();

    const u32 base = MMU::VRAM_BASE;                                     // inside first 96 KiB
    const u32 alias = MMU::VRAM_BASE + static_cast<u32>(MMU::VRAM_SIZE); // start of 32 KiB tail

    mmu.write16(base, kWordA1B2);
    EXPECT_EQ(mmu.read16(base), kWordA1B2);
    EXPECT_EQ(mmu.read16(alias), kWordA1B2); // aliased region
}

TEST(MMUWidth, PalAndOam16Mirror) {
    MMU mmu;
    mmu.reset();

    // Palette mirrors every 1 KiB across 16 MiB
    const u32 pal0 = MMU::PAL_BASE + kPalLastHalfwordOffset;
    const u32 palA = pal0 + static_cast<u32>(MMU::PAL_SIZE);
    mmu.write16(pal0, kWord4555);
    EXPECT_EQ(mmu.read16(pal0), kWord4555);
    EXPECT_EQ(mmu.read16(palA), kWord4555);

    // OAM mirrors every 1 KiB across 16 MiB
    const u32 oam0 = MMU::OAM_BASE + kOamSmallHalfwordOffset;
    const u32 oamA = oam0 + static_cast<u32>(MMU::OAM_SIZE);
    mmu.write16(oam0, kWord6677);
    EXPECT_EQ(mmu.read16(oam0), kWord6677);
    EXPECT_EQ(mmu.read16(oamA), kWord6677);
}

TEST(MMUWidth, BiosReadOnlyEvenFor16And32) {
    MMU mmu;
    mmu.reset();

    // Open bus when BIOS not loaded
    EXPECT_EQ(mmu.read16(MMU::BIOS_BASE + 2U), kOpenBus16);

    // Writes are ignored
    mmu.write16(MMU::BIOS_BASE + 2U, kWord1234);
    EXPECT_EQ(mmu.read8(MMU::BIOS_BASE + 2U), MMU::kOpenBus);
    EXPECT_EQ(mmu.read16(MMU::BIOS_BASE + 2U), kOpenBus16);
}

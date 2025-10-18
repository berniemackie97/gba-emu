// tests/mmu_map.cpp
#include <gtest/gtest.h>
#include "core/mmu/mmu.h"

using gba::MMU;
using u8 = std::uint8_t;
using u32 = std::uint32_t;

namespace {
    // Test data bytes (named so no “magic numbers”)
    constexpr u8 kV1 = 0x12U;
    constexpr u8 kV2 = 0x34U;
    constexpr u8 kA5 = 0xA5U;
    constexpr u8 k3F = 0x3FU;
    constexpr u8 k77 = 0x77U;
    constexpr u8 kCC = 0xCCU;
} // namespace

TEST(MMUMap, EwramReadWriteAndBounds) {
    MMU mmu;
    mmu.reset();

    const u32 base = MMU::EWRAM_BASE;
    const u32 last = base + static_cast<u32>(MMU::EWRAM_SIZE - 1);

    mmu.write8(base, kV1);
    mmu.write8(last, kV2);
    EXPECT_EQ(mmu.read8(base), kV1);
    EXPECT_EQ(mmu.read8(last), kV2);
}

TEST(MMUMap, MirrorsForVramPalOam) {
    MMU mmu;
    mmu.reset();

    // VRAM: last 32 KiB in first 128 KiB window mirrors first 32 KiB
    const u32 vram_base = MMU::VRAM_BASE;
    const u32 vram_alias = MMU::VRAM_BASE + static_cast<u32>(MMU::VRAM_SIZE); // start of 32 KiB tail
    mmu.write8(vram_base, kA5);
    EXPECT_EQ(mmu.read8(vram_base), kA5);
    EXPECT_EQ(mmu.read8(vram_alias), kA5);

    // Palette: 1 KiB mirror (PAL_SIZE) across 16 MiB window
    const u32 pal_off = 0x3FU; // any offset < PAL_SIZE; deliberately named
    const u32 pal_base = MMU::PAL_BASE + pal_off;
    const u32 pal_alias = pal_base + static_cast<u32>(MMU::PAL_SIZE);
    mmu.write8(pal_base, k3F);
    EXPECT_EQ(mmu.read8(pal_base), k3F);
    EXPECT_EQ(mmu.read8(pal_alias), k3F);

    // OAM: 1 KiB mirror (OAM_SIZE) across 16 MiB window
    const u32 oam_off = 0x10U; // any offset < OAM_SIZE
    const u32 oam_base = MMU::OAM_BASE + oam_off;
    const u32 oam_alias = oam_base + static_cast<u32>(MMU::OAM_SIZE);
    mmu.write8(oam_base, k77);
    EXPECT_EQ(mmu.read8(oam_base), k77);
    EXPECT_EQ(mmu.read8(oam_alias), k77);
}

TEST(MMUMap, BiosIsReadOnlyAndOpenBusWhenNotLoaded) {
    MMU mmu;
    mmu.reset();

    EXPECT_EQ(mmu.read8(MMU::BIOS_BASE + 0U), MMU::kOpenBus); // before BIOS load

    mmu.write8(MMU::BIOS_BASE + 1U, kCC); // ignored
    EXPECT_EQ(mmu.read8(MMU::BIOS_BASE + 1U), MMU::kOpenBus);
}

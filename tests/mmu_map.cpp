#include <cstdint>
#include <gtest/gtest.h>

#include "core/mmu/mmu.h"

using gba::MMU;
using u32 = std::uint32_t;

TEST(MMUMap, EwramRepeatsAcrossEntire16MiBWindow) {
    MMU mmu;
    mmu.reset();

    constexpr u32 base = MMU::EWRAM_BASE;
    constexpr u32 size = MMU::EWRAM_SIZE;
    mmu.write8(base, 0x12U);
    mmu.write8(base + size - 1U, 0x34U);

    EXPECT_EQ(mmu.read8(base + size), 0x12U);
    EXPECT_EQ(mmu.read8(base + size * 17U), 0x12U);
    EXPECT_EQ(mmu.read8(0x02FFFFFFU), 0x34U);

    mmu.write8(0x02FC0000U, 0x56U);
    EXPECT_EQ(mmu.read8(base), 0x56U);
    mmu.write8(0x02FFFFFFU, 0x78U);
    EXPECT_EQ(mmu.read8(base + size - 1U), 0x78U);

    mmu.write32(base + size - 2U, 0xA1B2C3D4U);
    EXPECT_EQ(mmu.read16(base + size - 2U), 0xC3D4U);
    EXPECT_EQ(mmu.read16(base), 0xA1B2U);
    EXPECT_EQ(mmu.read32(base + size - 2U), 0xA1B2C3D4U);
    EXPECT_EQ(mmu.read8(0x04000010U), 0U);
}

TEST(MMUMap, IwramRepeatsAcrossEntire16MiBWindow) {
    MMU mmu;
    mmu.reset();

    constexpr u32 base = MMU::IWRAM_BASE;
    constexpr u32 size = MMU::IWRAM_SIZE;
    mmu.write16(base, 0x3210U);
    mmu.write16(base + size - 2U, 0x7654U);

    EXPECT_EQ(mmu.read16(base + size), 0x3210U);
    EXPECT_EQ(mmu.read16(0x03FF8000U), 0x3210U);
    EXPECT_EQ(mmu.read16(0x03FFFFFEU), 0x7654U);

    mmu.write32(0x03FF8004U, 0x89ABCDEFU);
    EXPECT_EQ(mmu.read32(base + 4U), 0x89ABCDEFU);
    mmu.write8(0x03FFFFFFU, 0x66U);
    EXPECT_EQ(mmu.read8(base + size - 1U), 0x66U);

    EXPECT_EQ(mmu.read8(0x01FFFFFFU), MMU::kOpenBus);
}

TEST(MMUMap, VramTailAliasesFinalBankAcross16MiBWindow) {
    MMU mmu;
    mmu.reset();

    constexpr u32 base = MMU::VRAM_BASE;
    mmu.write16(base, 0x1122U);
    mmu.write16(base + 0x10000U, 0x3344U);
    mmu.write16(base + 0x17FFEU, 0x5566U);

    EXPECT_EQ(mmu.read16(base + 0x18000U), 0x3344U);
    EXPECT_EQ(mmu.read16(base + 0x1FFFEU), 0x5566U);
    EXPECT_EQ(mmu.read16(base + 0x20000U), 0x1122U);
    EXPECT_EQ(mmu.read16(base + 0x38000U), 0x3344U);
    EXPECT_EQ(mmu.read16(0x06FF8000U), 0x3344U);
    EXPECT_EQ(mmu.read16(0x06FFFFFEU), 0x5566U);

    mmu.write16(base + 0x18000U, 0x7788U);
    EXPECT_EQ(mmu.read16(base + 0x10000U), 0x7788U);
    EXPECT_EQ(mmu.read16(base), 0x1122U);
}

TEST(MMUMap, PaletteAndOamMirrorWithinTheir16MiBWindows) {
    MMU mmu;
    mmu.reset();

    mmu.write16(MMU::PAL_BASE + 0x3FEU, 0xA1B2U);
    EXPECT_EQ(mmu.read16(MMU::PAL_BASE + 0x7FEU), 0xA1B2U);
    EXPECT_EQ(mmu.read16(0x05FFFFFEU), 0xA1B2U);

    mmu.write32(MMU::OAM_BASE + 0x20U, 0x12345678U);
    EXPECT_EQ(mmu.read32(MMU::OAM_BASE + 0x420U), 0x12345678U);
    EXPECT_EQ(mmu.read32(0x07FFF020U), 0x12345678U);
}

TEST(MMUMap, BiosIsReadOnlyAndOpenBusWhenNotLoaded) {
    MMU mmu;
    mmu.reset();
    EXPECT_EQ(mmu.read8(MMU::BIOS_BASE), MMU::kOpenBus);
    mmu.write8(MMU::BIOS_BASE + 1U, 0xCCU);
    EXPECT_EQ(mmu.read8(MMU::BIOS_BASE + 1U), MMU::kOpenBus);
}

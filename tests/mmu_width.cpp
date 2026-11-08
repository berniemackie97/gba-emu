#include <cstdint>
#include <gtest/gtest.h>

#include "core/io/io.h"
#include "core/mmu/mmu.h"

using gba::IORegs;
using gba::MMU;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

TEST(MMUWidth, WorkRamSupportsByteHalfwordAndWordTransfers) {
    MMU mmu;
    mmu.reset();

    constexpr u32 base = MMU::EWRAM_BASE + 4U;
    mmu.write16(base, 0x1234U);
    EXPECT_EQ(mmu.read16(base), 0x1234U);

    mmu.write32(base + 4U, 0x01203040U);
    EXPECT_EQ(mmu.read32(base + 4U), 0x01203040U);

    mmu.write32(base + 5U, 0x01203040U);
    EXPECT_EQ(mmu.read8(base + 5U), 0x40U);
    EXPECT_EQ(mmu.read16(base + 5U), 0x3040U);
    EXPECT_EQ(mmu.read32(base + 5U), 0x01203040U);
}

TEST(MMUWidth, PaletteByteWritesDuplicateToBothHalfwordLanes) {
    MMU mmu;
    mmu.reset();

    mmu.write16(MMU::PAL_BASE + 0x2U, 0x1234U);
    mmu.write8(MMU::PAL_BASE + 0x2U, 0xA5U);
    EXPECT_EQ(mmu.read16(MMU::PAL_BASE + 0x2U), 0xA5A5U);

    mmu.write8(MMU::PAL_BASE + 0x403U, 0xB6U);
    EXPECT_EQ(mmu.read16(MMU::PAL_BASE + 0x2U), 0xB6B6U);

    mmu.write8(0x05FFFFFFU, 0x7CU);
    EXPECT_EQ(mmu.read16(MMU::PAL_BASE + 0x3FEU), 0x7C7CU);

    mmu.write32(MMU::PAL_BASE + 0x10U, 0x12345678U);
    EXPECT_EQ(mmu.read32(MMU::PAL_BASE + 0x10U), 0x12345678U);
    EXPECT_EQ(mmu.read16(MMU::PAL_BASE + 0x10U), 0x5678U);
    EXPECT_EQ(mmu.read16(MMU::PAL_BASE + 0x12U), 0x1234U);
}

TEST(MMUWidth, BackgroundVramByteWritesDuplicateAcrossMirroredHalfwords) {
    MMU mmu;
    mmu.reset();

    const u32 base = MMU::VRAM_BASE;
    mmu.write16(base + 0x2U, 0x1234U);
    mmu.write8(base + 0x3U, 0xA7U);
    EXPECT_EQ(mmu.read16(base + 0x2U), 0xA7A7U);

    mmu.write8(base + 0x20002U, 0x53U);
    EXPECT_EQ(mmu.read16(base + 0x2U), 0x5353U);

    mmu.write16(base + 0xFFFEU, 0x1010U);
    mmu.write8(base + 0xFFFFU, 0x6DU);
    EXPECT_EQ(mmu.read16(base + 0xFFFEU), 0x6D6DU);
}

TEST(MMUWidth, ObjectVramByteWritesDependOnDisplayMode) {
    constexpr u32 dispcnt = MMU::IO_BASE + IORegs::kOffDISPCNT;
    constexpr u32 base = MMU::VRAM_BASE;

    for (u16 mode = 0U; mode <= 5U; ++mode) {
        MMU mmu;
        mmu.reset();
        mmu.write16(dispcnt, mode);
        mmu.write16(base + 0x10000U, 0x1234U);
        mmu.write16(base + 0x14000U, 0x5678U);

        mmu.write8(base + 0x10000U, 0xA5U);
        EXPECT_EQ(mmu.read16(base + 0x10000U), mode < 3U ? 0x1234U : 0xA5A5U) << "Mode " << mode;

        mmu.write8(base + 0x18001U, 0xBCU);
        EXPECT_EQ(mmu.read16(base + 0x10000U), mode < 3U ? 0x1234U : 0xBCBCU) << "Mode " << mode;

        mmu.write8(base + 0x14001U, 0xDEU);
        mmu.write8(base + 0x1C000U, 0xEFU);
        EXPECT_EQ(mmu.read16(base + 0x14000U), 0x5678U) << "Mode " << mode;

        mmu.write32(base + 0x14000U, 0x89ABCDEFU);
        EXPECT_EQ(mmu.read32(base + 0x1C000U), 0x89ABCDEFU) << "Mode " << mode;
    }
}

TEST(MMUWidth, OamIgnoresByteWritesButAcceptsHalfwordAndWordWrites) {
    MMU mmu;
    mmu.reset();

    const u32 base = MMU::OAM_BASE;
    mmu.write16(base + 0x20U, 0x1234U);
    mmu.write8(base + 0x20U, 0xAAU);
    mmu.write8(base + 0x421U, 0xBBU);
    EXPECT_EQ(mmu.read16(base + 0x20U), 0x1234U);

    mmu.write32(base + 0x20U, 0x44332211U);
    EXPECT_EQ(mmu.read32(base + 0x20U), 0x44332211U);
    mmu.write8(0x07FFFFFFU, 0xCCU);
    EXPECT_EQ(mmu.read32(base + 0x20U), 0x44332211U);
}

TEST(MMUWidth, VramTailMirrorsLast32KiBForHalfwordsAndWords) {
    MMU mmu;
    mmu.reset();

    const u32 base = MMU::VRAM_BASE;
    mmu.write32(base + 0x10000U, 0xA1B2C3D4U);
    EXPECT_EQ(mmu.read32(base + 0x18000U), 0xA1B2C3D4U);

    mmu.write16(base + 0x1FFFEU, 0x6677U);
    EXPECT_EQ(mmu.read16(base + 0x17FFEU), 0x6677U);
    EXPECT_EQ(mmu.read16(0x06FFFFFEU), 0x6677U);
    EXPECT_EQ(mmu.read16(base), 0U);
}

TEST(MMUWidth, BiosRemainsReadOnlyAtAllWidths) {
    MMU mmu;
    mmu.reset();

    EXPECT_EQ(mmu.read16(MMU::BIOS_BASE + 2U), 0xFFFFU);
    mmu.write16(MMU::BIOS_BASE + 2U, 0x1234U);
    mmu.write32(MMU::BIOS_BASE + 4U, 0xAABBCCDDU);
    EXPECT_EQ(mmu.read16(MMU::BIOS_BASE + 2U), 0xFFFFU);
    EXPECT_EQ(mmu.read32(MMU::BIOS_BASE + 4U), 0xFFFFFFFFU);
}


#include <gtest/gtest.h>
#include <vector>
#include "core/mmu/mmu.h"

using gba::MMU;
using u8 = std::uint8_t;
using u32 = std::uint32_t;

namespace {
    // A tiny “ROM” pattern that makes wrapping obvious.
    constexpr u8 kD0 = 0xDEU;
    constexpr u8 kA0 = 0xADU;
    constexpr u8 kB0 = 0xBEU;
    constexpr u8 kE0 = 0xEFU;

    const std::vector<u8> kTinyRom = {kD0, kA0, kB0, kE0};

    // Helpful base addresses
    constexpr u32 kWs0 = MMU::WS0_BASE;
    constexpr u32 kWs1 = MMU::WS1_BASE;
    constexpr u32 kWs2 = MMU::WS2_BASE;
} // namespace

TEST(GamePak, MirrorsAcrossWaitStatesAndBySize) {
    MMU mmu;
    mmu.reset();
    mmu.load_gamepak(kTinyRom); // 4 bytes long, will mirror by size

    // Same offset in all three regions should read the same byte.
    EXPECT_EQ(mmu.read8(kWs0 + 0U), kD0);
    EXPECT_EQ(mmu.read8(kWs1 + 0U), kD0);
    EXPECT_EQ(mmu.read8(kWs2 + 0U), kD0);

    EXPECT_EQ(mmu.read8(kWs0 + 1U), kA0);
    EXPECT_EQ(mmu.read8(kWs1 + 2U), kB0);
    EXPECT_EQ(mmu.read8(kWs2 + 3U), kE0);

    // Wrap by ROM size inside the 32 MiB window.
    constexpr u32 kOffsetPastSize = 4U; // == rom size
    EXPECT_EQ(mmu.read8(kWs0 + kOffsetPastSize + 0U), kD0);
    EXPECT_EQ(mmu.read8(kWs0 + kOffsetPastSize + 1U), kA0);
}

TEST(GamePak, WritesAreIgnored) {
    MMU mmu;
    mmu.reset();
    mmu.load_gamepak(kTinyRom);

    const u32 addr = MMU::WS0_BASE + 0U;
    EXPECT_EQ(mmu.read8(addr), kD0);

    mmu.write8(addr, 0x00U); // should be ignored
    EXPECT_EQ(mmu.read8(addr), kD0);
}

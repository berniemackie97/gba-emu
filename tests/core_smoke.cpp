#include <array>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

TEST(CoreSmoke, BusDefaults) {
    gba::Bus bus;
    bus.reset();
    EXPECT_EQ(bus.read8(gba::MMU::BIOS_BASE), gba::MMU::kOpenBus);
    EXPECT_EQ(bus.read8(gba::MMU::WS0_BASE), gba::MMU::kOpenBus);
}

TEST(CoreSmoke, ResetLoadExecuteAndShutdown) {
    // Thumb MOVS r0, #5; ADDS r0, #3; SUBS r0, #2.
    constexpr std::array<std::uint8_t, 6> program{
        0x05U, 0x20U, 0x03U, 0x30U, 0x02U, 0x38U
    };

    gba::Bus bus;
    bus.reset();
    ASSERT_TRUE(bus.load_gamepak(program));

    gba::ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(gba::MMU::WS0_BASE);

    cpu.step();
    cpu.step();
    cpu.step();

    EXPECT_EQ(cpu.debug_reg(0), 6U);
    EXPECT_EQ(cpu.debug_pc(), gba::MMU::WS0_BASE + static_cast<std::uint32_t>(program.size()));

    cpu.reset();
    bus.reset();
    EXPECT_EQ(cpu.debug_reg(0), 0U);
    EXPECT_EQ(cpu.debug_pc(), 0U);
    EXPECT_EQ(bus.read8(gba::MMU::WS0_BASE), gba::MMU::kOpenBus);
}

TEST(CoreSmoke, MissingStartupAssetsAreRejected) {
    gba::Bus bus;
    bus.reset();

    const auto missing = std::filesystem::current_path() / "nonexistent-gba-smoke-fixtures" / "missing.bin";
    ASSERT_FALSE(std::filesystem::exists(missing));
    EXPECT_FALSE(bus.load_bios(missing));
    EXPECT_FALSE(bus.load_gamepak(missing));
    EXPECT_EQ(bus.read8(gba::MMU::BIOS_BASE), gba::MMU::kOpenBus);
    EXPECT_EQ(bus.read8(gba::MMU::WS0_BASE), gba::MMU::kOpenBus);
}

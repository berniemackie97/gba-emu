#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"
#include "support/temp_asset.h"
#include "support/thumb_program.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

TEST(ThumbBranch, UnconditionalOffsetsUseInstructionAddressPlusFour) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE + 0x1000U;
    struct Vector {
        std::uint16_t opcode;
        std::uint32_t target;
    };
    constexpr std::array<Vector, 6> cases{{
        {0xE000U, base + 4U},
        {0xE001U, base + 6U},
        {0xE7FFU, base + 2U},
        {0xE7FEU, base},
        {0xE3FFU, base + 2050U},
        {0xE400U, base - 2044U}
    }};

    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);
    for (const auto &vector : cases) {
        cpu.reset();
        bus.write16(base, vector.opcode);
        cpu.debug_set_program_counter(base);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), vector.target) << "Opcode: " << vector.opcode;
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }
}

TEST(ThumbBranch, TakenAndNotTakenConditionalBranch) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE;
    constexpr std::array<std::uint16_t, 5> program{
        0x2001U,  // MOVS r0, #1
        0x2801U,  // CMP r0, #1: Z=1
        0xD000U,  // BEQ: branch to instruction address +4
        0x2163U,  // MOVS r1, #99 (skipped)
        0x2107U   // MOVS r1, #7
    };

    Bus bus;
    bus.reset();
    gba::test::write_thumb_program(bus, base, program);
    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.step();
    cpu.step();
    cpu.step();
    EXPECT_EQ(cpu.debug_pc(), base + 8U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(1), 7U);
    EXPECT_EQ(cpu.debug_pc(), base + 10U);

    bus.write16(base + 4U, 0xD100U); // BNE with Z set: not taken
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.step();
    cpu.step();
    cpu.step();
    EXPECT_EQ(cpu.debug_pc(), base + 6U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(1), 99U);
}

TEST(ThumbBranch, NegativeConditionalOffsetsRemainDefined) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE + 0x400U;

    Bus bus;
    bus.reset();
    bus.write16(base, 0xD180U); // BNE -256, with Z clear at reset
    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.step();
    EXPECT_EQ(cpu.debug_pc(), base - 252U); // PC +4 -256
    EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);

    cpu.reset();
    bus.write16(base, 0xD1FFU); // BNE -2
    cpu.debug_set_program_counter(base);
    cpu.step();
    EXPECT_EQ(cpu.debug_pc(), base + 2U);

    cpu.reset();
    bus.write16(base, 0xD100U); // BNE +0
    cpu.debug_set_program_counter(base);
    cpu.step();
    EXPECT_EQ(cpu.debug_pc(), base + 4U);
}

TEST(ThumbBranch, GeneratedCartridgeExecutesBranchToTarget) {
    constexpr std::array<std::uint8_t, 6> cartridge{
        0x00U, 0xE0U, // B +0: instruction address +4
        0x7FU, 0x20U, // MOVS r0, #127 (skipped)
        0x2AU, 0x20U  // MOVS r0, #42
    };
    Bus bus;
    bus.reset();
    ASSERT_TRUE(bus.load_gamepak(cartridge));

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(MMU::WS0_BASE);
    cpu.step();
    EXPECT_EQ(cpu.debug_pc(), MMU::WS0_BASE + 4U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(0), 42U);
    EXPECT_EQ(cpu.debug_pc(), MMU::WS0_BASE + 6U);
}

TEST(ThumbBranch, NegativeOffsetWrapsAddressSpaceAtZero) {
    std::array<std::uint8_t, MMU::BIOS_SIZE> bios{};
    bios[0] = 0xFCU;
    bios[1] = 0xE7U; // B -8 from address 0, PC+4-8 wraps to 0xFFFFFFFC

    const gba::test::TempAsset image("cpu-thumb-branch-wrap-bios.bin");
    ASSERT_TRUE(image.write(bios));

    Bus bus;
    bus.reset();
    ASSERT_TRUE(bus.load_bios(image.path()));

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(MMU::BIOS_BASE);
    cpu.step();

    EXPECT_EQ(cpu.debug_pc(), 0xFFFFFFFCU);
    EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
}

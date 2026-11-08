#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"
#include "support/thumb_program.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

TEST(ThumbLoadFlags, LiteralLoadPreservesCompleteCpsr) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE + 0x100U;
    Bus bus;
    bus.reset();
    bus.write16(base, 0x2000U);      // MOVS r0, #0
    bus.write16(base + 2U, 0x4900U); // LDR r1, [PC, #0]
    bus.write32(base + 4U, 0x80000000U);

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.step();
    const auto before = cpu.debug_cpsr();
    ASSERT_NE(before & ARM7TDMI::kFlagZ, 0U);

    cpu.step();
    EXPECT_EQ(cpu.debug_reg(1), 0x80000000U);
    EXPECT_EQ(cpu.debug_cpsr(), before);
    EXPECT_EQ(cpu.debug_pc(), base + 4U);
}

TEST(ThumbLoadFlags, WordByteLoadsAndStoresPreserveCompleteCpsr) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE + 0x200U;
    constexpr std::uint32_t data = MMU::IWRAM_BASE + 0x500U;
    constexpr std::array<std::uint16_t, 6> instructions{
        0x2000U, // MOVS r0, #0
        0x2800U, // CMP r0, #0
        0x6811U, // LDR r1, [r2, #0]
        0x7813U, // LDRB r3, [r2, #0]
        0x6051U, // STR r1, [r2, #4]
        0x7211U  // STRB r1, [r2, #8]
    };
    Bus bus;
    bus.reset();
    gba::test::write_thumb_program(bus, base, instructions);
    bus.write32(data, 0xAABBCCDDU);

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.debug_set_reg(2, data);
    cpu.step();
    cpu.step();
    const auto before = cpu.debug_cpsr();
    ASSERT_NE(before & ARM7TDMI::kFlagZ, 0U);
    ASSERT_NE(before & ARM7TDMI::kFlagC, 0U);

    for (int instruction = 0; instruction < 4; ++instruction) {
        cpu.step();
        EXPECT_EQ(cpu.debug_cpsr(), before);
    }
    EXPECT_EQ(cpu.debug_reg(1), 0xAABBCCDDU);
    EXPECT_EQ(cpu.debug_reg(3), 0xDDU);
    EXPECT_EQ(bus.read32(data + 4U), 0xAABBCCDDU);
    EXPECT_EQ(bus.read8(data + 8U), 0xDDU);
}

TEST(ThumbLoadFlags, LoadDoesNotChangeSubsequentBranchDecision) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE + 0x300U;
    constexpr std::uint32_t data = MMU::IWRAM_BASE + 0x600U;
    constexpr std::array<std::uint16_t, 5> instructions{
        0x2000U, // MOVS r0, #0: Z set
        0x681AU, // LDR r2, [r3, #0]: Z preserved
        0xD000U, // BEQ +0, skip next instruction
        0x2163U, // MOVS r1, #99 (skipped)
        0x2107U  // MOVS r1, #7
    };
    Bus bus;
    bus.reset();
    gba::test::write_thumb_program(bus, base, instructions);
    bus.write32(data, 0x12345678U);

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.debug_set_reg(3, data);

    cpu.step();
    const auto flags = cpu.debug_cpsr();
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(2), 0x12345678U);
    EXPECT_EQ(cpu.debug_cpsr(), flags);
    cpu.step();
    EXPECT_EQ(cpu.debug_pc(), base + 8U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(1), 7U);
    EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
}

TEST(ThumbLoadFlags, CompareAndArithmeticStillUpdateFlags) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE + 0x400U;
    constexpr std::array<std::uint16_t, 3> instructions{
        0x2001U, // MOVS r0, #1
        0x2801U, // CMP r0, #1 (Z and C set)
        0x3802U  // SUBS r0, #2 (N set, C clear)
    };
    Bus bus;
    bus.reset();
    gba::test::write_thumb_program(bus, base, instructions);
    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.step();
    cpu.step();
    EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagZ, 0U);
    EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagC, 0U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(0), 0xFFFFFFFFU);
    EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagN, 0U);
    EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagZ, 0U);
    EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagC, 0U);
}

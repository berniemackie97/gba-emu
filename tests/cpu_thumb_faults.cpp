#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

TEST(CpuFault, MissingBusIsReportedBeforeFetch) {
    ARM7TDMI cpu;
    cpu.reset();
    cpu.debug_set_program_counter(MMU::IWRAM_BASE);
    cpu.step();

    const auto &fault = cpu.execution_fault();
    EXPECT_EQ(fault.kind, ARM7TDMI::FaultKind::MissingBus);
    EXPECT_EQ(fault.pc, MMU::IWRAM_BASE);
    EXPECT_EQ(fault.opcode, 0U);
    EXPECT_NE(fault.cpsr & ARM7TDMI::kFlagT, 0U);
    EXPECT_EQ(cpu.debug_pc(), MMU::IWRAM_BASE);

    cpu.reset();
    EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
}

TEST(CpuFault, UnsupportedAndUndefinedInstructionsHavePreciseContext) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE;
    struct Vector {
        std::uint16_t opcode;
        ARM7TDMI::FaultKind kind;
    };
    constexpr std::array<Vector, 4> vectors{{
        {0xBE00U, ARM7TDMI::FaultKind::UnsupportedThumb},
        {0xDE00U, ARM7TDMI::FaultKind::UndefinedThumb},
        {0xDF00U, ARM7TDMI::FaultKind::UnsupportedThumb},
        {0xB600U, ARM7TDMI::FaultKind::UnsupportedThumb}
    }};

    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    for (const auto &vector : vectors) {
        cpu.reset();
        cpu.debug_set_program_counter(base);
        bus.write16(base, vector.opcode);
        cpu.step();
        const auto &fault = cpu.execution_fault();
        EXPECT_EQ(fault.kind, vector.kind);
        EXPECT_EQ(fault.pc, base);
        EXPECT_EQ(fault.opcode, vector.opcode);
        EXPECT_NE(fault.cpsr & ARM7TDMI::kFlagT, 0U);
        EXPECT_EQ(cpu.debug_pc(), base);

        bus.write16(base, 0x2107U);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), base);
        EXPECT_EQ(cpu.debug_reg(1), 0U);
    }
}

TEST(CpuFault, ValidHighRegisterMoveIsNotAnUnsupportedOpcode) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE;
    Bus bus;
    bus.reset();
    bus.write16(base, 0x46C0U); // MOV r8, r8

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.debug_set_reg(8, 0x12345678U);
    const auto flags = cpu.debug_cpsr();
    cpu.step();

    EXPECT_EQ(cpu.debug_reg(8), 0x12345678U);
    EXPECT_EQ(cpu.debug_cpsr(), flags);
    EXPECT_EQ(cpu.debug_pc(), base + 2U);
    EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
}

TEST(CpuFault, ArmStateTransitionFetchesAndReportsUnsupportedArmOpcode) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE;
    constexpr std::uint32_t target = MMU::IWRAM_BASE + 0x100U;
    Bus bus;
    bus.reset();
    bus.write16(base, 0x4700U); // BX r0
    bus.write32(target, 0xEF000000U); // SWI remains unsupported until exception modes

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.debug_set_reg(0, target);
    cpu.step();
    ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    ASSERT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U);
    ASSERT_EQ(cpu.debug_pc(), target);

    cpu.step();
    const auto &fault = cpu.execution_fault();
    EXPECT_EQ(fault.kind, ARM7TDMI::FaultKind::UnsupportedArmOpcode);
    EXPECT_EQ(fault.pc, target);
    EXPECT_EQ(fault.opcode, 0xEF000000U);
    EXPECT_EQ(fault.cpsr & ARM7TDMI::kFlagT, 0U);
    EXPECT_EQ(cpu.debug_pc(), target);
    EXPECT_EQ(cpu.debug_reg(1), 0U);
}

TEST(CpuFault, ThumbBranchExchangeContinuesExecutingThumbCode) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE;
    constexpr std::uint32_t target = base + 0x100U;
    Bus bus;
    bus.reset();
    bus.write16(base, 0x4700U); // BX r0
    bus.write16(target, 0x2109U); // MOVS r1, #9

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);
    cpu.debug_set_reg(0, target | 1U);

    cpu.step();
    EXPECT_EQ(cpu.debug_pc(), target);
    EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(1), 9U);
    EXPECT_EQ(cpu.debug_pc(), target + 2U);
    EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
}

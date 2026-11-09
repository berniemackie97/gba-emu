#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "support/arm_program.h"

namespace {

    using gba::ARM7TDMI;
    using gba::Bus;
    using namespace gba::test;

    TEST(ArmControl, ArchitecturalPcReadsAndStoreUseDistinctPipelineOffsets) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        cpu.debug_set_reg(5, kArmData);
        bus.write32(kArmEntry, 0xE1A0100FU);      // MOV r1,pc
        bus.write32(kArmEntry + 4U, 0xE585F000U); // STR pc,[r5]
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(1), kArmEntry + 8U);
        cpu.step();
        EXPECT_EQ(bus.read32(kArmData), kArmEntry + 16U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ArmControl, BranchAndLinkUseInstructionAddressPlusEightForTarget) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        bus.write32(kArmEntry, 0xEA000000U); // B +0
        bus.write32(kArmEntry + 8U, 0xEBFFFFFEU); // BL to itself
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), kArmEntry + 8U);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), kArmEntry + 8U);
        EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegLR), kArmEntry + 12U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ArmControl, MovToPcAlignsButDoesNotExchangeState) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        constexpr std::uint32_t target = kArmEntry + 0x200U;
        cpu.debug_set_reg(1, target | 3U);
        bus.write32(kArmEntry, 0xE1A0F001U); // MOV pc,r1
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), target);
        EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ArmControl, ConditionFailureSkipsUnsupportedInstructionWithoutSideEffects) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu); // CMP r7,#1 leaves Z clear
        bus.write32(kArmEntry, 0x0F000000U); // SWIEQ (not taken)
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), kArmEntry + 4U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ArmControl, UnsupportedArmInstructionReportsFullOpcodeAndStickyPc) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        bus.write32(kArmEntry, 0xEF00000FU); // SWI (pending exception implementation)
        bus.write32(kArmEntry + 4U, 0xE3A01007U);
        cpu.step();
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::UnsupportedArmOpcode);
        EXPECT_EQ(cpu.execution_fault().pc, kArmEntry);
        EXPECT_EQ(cpu.execution_fault().opcode, 0xEF00000FU);
        EXPECT_EQ(cpu.debug_pc(), kArmEntry);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), kArmEntry);
        EXPECT_EQ(cpu.debug_reg(1), 0U);
    }

    TEST(ArmControl, StatusRegisterReadAndExceptionReturnAreNotFakedAsDataOperations) {
        constexpr std::array<std::uint32_t, 2> instructions{
            0xE10F1000U, // MRS r1,CPSR
            0xE1B0F00EU  // MOVS pc,lr: requires SPSR
        };
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        for (const auto insn : instructions) {
            enter_arm(bus, cpu);
            bus.write32(kArmEntry, insn);
            cpu.step();
            EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::UnsupportedArmOpcode);
            EXPECT_EQ(cpu.execution_fault().opcode, insn);
            EXPECT_EQ(cpu.debug_pc(), kArmEntry);
        }
    }

} // namespace

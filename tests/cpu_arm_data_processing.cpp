#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "support/arm_program.h"

namespace {

    using gba::ARM7TDMI;
    using gba::Bus;
    using namespace gba::test;

    TEST(ArmExecution, ConditionalProgramCrossesBothInstructionSets) {
        Bus bus;
        bus.reset();
        constexpr std::array<std::uint32_t, 6> program{
            0xE3A0107FU, // MOV r1,#127
            0xE2812001U, // ADD r2,r1,#1
            0xE3520080U, // CMP r2,#128
            0x03A03007U, // MOVEQ r3,#7
            0x13A03063U, // MOVNE r3,#99 (skipped)
            0xE12FFF14U  // BX r4 (return to Thumb)
        };
        for (std::size_t i = 0; i < program.size(); ++i) {
            bus.write32(kArmEntry + static_cast<std::uint32_t>(i * 4U), program[i]);
        }
        constexpr std::uint32_t returnAddress = kThumbEntry + 0x200U;
        bus.write16(returnAddress, 0x2509U); // MOVS r5,#9
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
        EXPECT_EQ(cpu.debug_pc(), kArmEntry);
        EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U);
        cpu.debug_set_reg(4, returnAddress | 1U);
        for (unsigned i = 0; i < program.size(); ++i) {
            cpu.step();
            ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None) << i;
        }
        EXPECT_EQ(cpu.debug_reg(1), 127U);
        EXPECT_EQ(cpu.debug_reg(2), 128U);
        EXPECT_EQ(cpu.debug_reg(3), 7U);
        EXPECT_EQ(cpu.debug_pc(), returnAddress);
        EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(5), 9U);
        EXPECT_EQ(cpu.debug_pc(), returnAddress + 2U);
    }

    TEST(ArmExecution, AddAndSubtractSetCarryBorrowAndOverflow) {
        struct Vector {
            std::uint32_t instruction;
            std::uint32_t left;
            std::uint32_t right;
            bool carry;
            std::uint32_t result;
            std::uint32_t flags;
        };
        constexpr std::array<Vector, 4> vectors{{
            {0xE0912004U, 0x7FFFFFFFU, 1U, false, 0x80000000U, ARM7TDMI::kFlagN | ARM7TDMI::kFlagV},
            {0xE0912004U, 0xFFFFFFFFU, 1U, false, 0U, ARM7TDMI::kFlagZ | ARM7TDMI::kFlagC},
            {0xE0D12004U, 0U, 0U, false, 0xFFFFFFFFU, ARM7TDMI::kFlagN},
            {0xE0D12004U, 0U, 0U, true, 0U, ARM7TDMI::kFlagZ | ARM7TDMI::kFlagC}
        }};
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        for (const auto &v : vectors) {
            SCOPED_TRACE(v.instruction);
            enter_arm(bus, cpu, v.carry);
            cpu.debug_set_reg(1, v.left);
            cpu.debug_set_reg(4, v.right);
            bus.write32(kArmEntry, v.instruction);
            cpu.step();
            ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
            EXPECT_EQ(cpu.debug_reg(2), v.result);
            constexpr std::uint32_t nzcv = ARM7TDMI::kFlagN | ARM7TDMI::kFlagZ |
                ARM7TDMI::kFlagC | ARM7TDMI::kFlagV;
            EXPECT_EQ(cpu.debug_cpsr() & nzcv, v.flags);
        }
    }

    TEST(ArmExecution, ShifterBoundaryVectorsFollowArmv4TCarries) {
        struct Vector {
            std::uint32_t opcode;
            std::uint32_t value;
            std::uint32_t shift;
            bool carry;
            std::uint32_t result;
            bool carryOut;
        };
        constexpr std::array<Vector, 8> vectors{{
            {0xE1B02081U, 0x80000001U, 0U, false, 2U, true},           // LSL #1
            {0xE1B02021U, 0x80000001U, 0U, false, 0U, true},           // LSR #32
            {0xE1B02041U, 0x80000001U, 0U, false, 0xFFFFFFFFU, true}, // ASR #32
            {0xE1B02061U, 1U, 0U, true, 0x80000000U, true},           // RRX
            {0xE1B02311U, 0x80000001U, 32U, false, 0U, true},         // LSL r3
            {0xE1B02311U, 0x80000001U, 33U, true, 0U, false},
            {0xE1B02371U, 0x80000001U, 32U, false, 0x80000001U, true},// ROR r3
            {0xE1B02371U, 0x80000001U, 0U, true, 0x80000001U, true}
        }};
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        for (const auto &v : vectors) {
            SCOPED_TRACE(v.opcode);
            SCOPED_TRACE(v.shift);
            enter_arm(bus, cpu, v.carry);
            cpu.debug_set_reg(1, v.value);
            cpu.debug_set_reg(3, v.shift);
            bus.write32(kArmEntry, v.opcode);
            cpu.step();
            ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
            EXPECT_EQ(cpu.debug_reg(2), v.result);
            EXPECT_EQ((cpu.debug_cpsr() & ARM7TDMI::kFlagC) != 0U, v.carryOut);
            EXPECT_EQ((cpu.debug_cpsr() & ARM7TDMI::kFlagZ) != 0U, v.result == 0U);
            EXPECT_EQ((cpu.debug_cpsr() & ARM7TDMI::kFlagN) != 0U,
                      (v.result & ARM7TDMI::kSignBit) != 0U);
        }
    }

    TEST(ArmExecution, MultiplyAndLongMultiplyUseFullWidthProducts) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        cpu.debug_set_reg(1, 7U);
        cpu.debug_set_reg(2, 6U);
        cpu.debug_set_reg(4, 5U);
        bus.write32(kArmEntry, 0xE0030291U); // MUL r3,r1,r2
        bus.write32(kArmEntry + 4U, 0xE0234291U); // MLA r3,r1,r2,r4
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(3), 42U);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(3), 47U);

        enter_arm(bus, cpu);
        cpu.debug_set_reg(1, 0xFFFFFFFFU);
        cpu.debug_set_reg(2, 2U);
        bus.write32(kArmEntry, 0xE0843291U); // UMULL r3,r4,r1,r2
        bus.write32(kArmEntry + 4U, 0xE0C43291U); // SMULL r3,r4,r1,r2
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(3), 0xFFFFFFFEU);
        EXPECT_EQ(cpu.debug_reg(4), 1U);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(3), 0xFFFFFFFEU);
        EXPECT_EQ(cpu.debug_reg(4), 0xFFFFFFFFU);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

} // namespace

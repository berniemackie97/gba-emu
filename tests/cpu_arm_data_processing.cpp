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

    TEST(ArmExecution, AllSixteenDataProcessingOpcodesHonorFlagsAndDestinationRules) {
        struct Vector {
            std::uint32_t instruction;
            std::uint32_t result;
            std::uint32_t flags;
            bool writes;
        };
        constexpr std::array<Vector, 16> vectors{{
            {0xE0112004U, 0U, 0x60000000U, true},          // ANDS
            {0xE0312004U, 0x80000001U, 0xA0000000U, true}, // EORS
            {0xE0512004U, 0x7FFFFFFFU, 0x30000000U, true}, // SUBS
            {0xE0712004U, 0x80000001U, 0x90000000U, true}, // RSBS
            {0xE0912004U, 0x80000001U, 0x80000000U, true}, // ADDS
            {0xE0B12004U, 0x80000002U, 0x80000000U, true}, // ADCS
            {0xE0D12004U, 0x7FFFFFFFU, 0x30000000U, true}, // SBCS
            {0xE0F12004U, 0x80000001U, 0x90000000U, true}, // RSCS
            {0xE1112004U, 0U, 0x60000000U, false},         // TST
            {0xE1312004U, 0x80000001U, 0xA0000000U, false}, // TEQ
            {0xE1512004U, 0x7FFFFFFFU, 0x30000000U, false}, // CMP
            {0xE1712004U, 0x80000001U, 0x80000000U, false}, // CMN
            {0xE1912004U, 0x80000001U, 0xA0000000U, true}, // ORRS
            {0xE1B12004U, 1U, 0x20000000U, true},          // MOVS
            {0xE1D12004U, 0x80000000U, 0xA0000000U, true}, // BICS
            {0xE1F12004U, 0xFFFFFFFEU, 0xA0000000U, true} // MVNS
        }};

        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        for (const auto &v : vectors) {
            SCOPED_TRACE(v.instruction);
            enter_arm(bus, cpu, true);
            cpu.debug_set_reg(1, 0x80000000U);
            cpu.debug_set_reg(2, 0x12345678U);
            cpu.debug_set_reg(4, 1U);
            bus.write32(kArmEntry, v.instruction);
            cpu.step();
            ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
            EXPECT_EQ(cpu.debug_reg(2), v.writes ? v.result : 0x12345678U);
            constexpr std::uint32_t nzcv = ARM7TDMI::kFlagN | ARM7TDMI::kFlagZ |
                ARM7TDMI::kFlagC | ARM7TDMI::kFlagV;
            EXPECT_EQ(cpu.debug_cpsr() & nzcv, v.flags);
        }
    }

    TEST(ArmExecution, RotatedImmediateUpdatesCarryWhileUnrotatedPreservesIt) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu, true);
        bus.write32(kArmEntry, 0xE3B02102U); // MOVS r2,#0x80000000
        bus.write32(kArmEntry + 4U, 0xE3B03001U); // MOVS r3,#1
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(2), 0x80000000U);
        EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagC, 0U);
        EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagN, 0U);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(3), 1U);
        EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagC, 0U);
        EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagN, 0U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
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
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);

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

        enter_arm(bus, cpu);
        cpu.debug_set_reg(1, 0xFFFFFFFFU);
        cpu.debug_set_reg(2, 2U);
        cpu.debug_set_reg(3, 10U);
        cpu.debug_set_reg(4, 1U);
        bus.write32(kArmEntry, 0xE0A43291U); // UMLAL r3,r4,r1,r2
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(3), 8U);
        EXPECT_EQ(cpu.debug_reg(4), 3U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

} // namespace

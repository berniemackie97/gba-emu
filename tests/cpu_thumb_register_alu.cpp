#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

namespace {

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

constexpr std::uint32_t kCode = MMU::IWRAM_BASE;
constexpr std::uint32_t kNzcv = ARM7TDMI::kFlagN | ARM7TDMI::kFlagZ |
    ARM7TDMI::kFlagC | ARM7TDMI::kFlagV;

constexpr auto alu(std::uint16_t op, std::uint16_t destination = 0U,
                   std::uint16_t source = 1U) -> std::uint16_t {
    return static_cast<std::uint16_t>(0x4000U | (op << 6U) | (source << 3U) | destination);
}

void execute(Bus &bus, ARM7TDMI &cpu, std::uint16_t opcode,
             std::uint32_t left, std::uint32_t right, bool carry) {
    cpu.reset();
    cpu.debug_set_program_counter(kCode);
    cpu.debug_set_reg(0, left);
    cpu.debug_set_reg(1, right);
    cpu.debug_set_reg(7, 0U);
    bus.write16(kCode, carry ? 0x2F00U : 0x2F01U); // CMP r7,#0 or CMP r7,#1
    bus.write16(kCode + 2U, opcode);
    cpu.step();
    cpu.step();
}

TEST(ThumbRegisterAlu, DataProcessingVectorsCoverEveryOperation) {
    struct Vector {
        std::uint8_t operation;
        std::uint32_t left;
        std::uint32_t right;
        bool carry;
        std::uint32_t expectedResult;
        std::uint32_t expectedFlags;
        bool writes;
        std::uint32_t flagMask;
    };
    constexpr std::array<Vector, 17> vectors{{
        {0, 0xF0U, 0x0FU, true, 0U, 0x60000000U, true, kNzcv},
        {1, 0xF0U, 0x0FU, true, 0xFFU, 0x20000000U, true, kNzcv},
        {2, 0x80000001U, 1U, true, 2U, 0x20000000U, true, kNzcv},
        {3, 0x80000001U, 1U, true, 0x40000000U, 0x20000000U, true, kNzcv},
        {4, 0x80000001U, 1U, true, 0xC0000000U, 0xA0000000U, true, kNzcv},
        {5, 0xFFFFFFFFU, 0U, true, 0U, 0x60000000U, true, kNzcv},
        {6, 0U, 0U, false, 0xFFFFFFFFU, 0x80000000U, true, kNzcv},
        {7, 0x80000001U, 1U, true, 0xC0000000U, 0xA0000000U, true, kNzcv},
        {8, 0x10U, 0x01U, true, 0x10U, 0x60000000U, false, kNzcv},
        {9, 0x1234U, 0x80000000U, true, 0x80000000U, 0x90000000U, true, kNzcv},
        {10, 5U, 5U, false, 5U, 0x60000000U, false, kNzcv},
        {11, 0x7FFFFFFFU, 1U, false, 0x7FFFFFFFU, 0x90000000U, false, kNzcv},
        {12, 0x80000000U, 0x7FFFFFFFU, true, 0xFFFFFFFFU, 0xA0000000U, true, kNzcv},
        {13, 0xFFFFFFFFU, 2U, true, 0xFFFFFFFEU, 0x80000000U, true, kNzcv & ~ARM7TDMI::kFlagC},
        {14, 0xFFFFFFFFU, 0x0FU, true, 0xFFFFFFF0U, 0xA0000000U, true, kNzcv},
        {15, 0U, 0U, true, 0xFFFFFFFFU, 0xA0000000U, true, kNzcv},
        {5, 0x7FFFFFFFU, 0U, true, 0x80000000U, 0x90000000U, true, kNzcv}
    }};

    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    for (const auto &v : vectors) {
        SCOPED_TRACE(static_cast<int>(v.operation));
        execute(bus, cpu, alu(v.operation), v.left, v.right, v.carry);
        ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
        EXPECT_EQ(cpu.debug_reg(0), v.expectedResult);
        EXPECT_EQ(cpu.debug_reg(1), v.right);
        EXPECT_EQ(cpu.debug_cpsr() & v.flagMask, v.expectedFlags & v.flagMask);
    }
}

TEST(ThumbRegisterAlu, RegisterShiftBoundariesUseLowByteAndExactCarryRules) {
    struct Vector {
        std::uint8_t operation;
        std::uint32_t value;
        std::uint32_t shift;
        bool carryIn;
        std::uint32_t result;
        bool carryOut;
    };
    constexpr std::array<Vector, 19> vectors{{
        {2, 0x80000001U, 0U, true, 0x80000001U, true},
        {2, 0x80000001U, 1U, false, 2U, true},
        {2, 0x80000001U, 32U, false, 0U, true},
        {2, 0x80000001U, 33U, true, 0U, false},
        {2, 0x80000001U, 257U, false, 2U, true},
        {3, 0x80000001U, 0U, true, 0x80000001U, true},
        {3, 0x80000001U, 32U, false, 0U, true},
        {3, 0x80000001U, 33U, true, 0U, false},
        {4, 0x80000001U, 1U, false, 0xC0000000U, true},
        {4, 0x80000001U, 32U, false, 0xFFFFFFFFU, true},
        {4, 0x80000001U, 255U, false, 0xFFFFFFFFU, true},
        {4, 0x7FFFFFFFU, 32U, true, 0U, false},
        {7, 0x80000001U, 0U, true, 0x80000001U, true},
        {7, 0x80000001U, 1U, false, 0xC0000000U, true},
        {7, 0x80000001U, 32U, false, 0x80000001U, true},
        {7, 0x80000001U, 33U, false, 0xC0000000U, true},
        {7, 0x80000001U, 64U, false, 0x80000001U, true},
        {7, 0x80000000U, 1U, true, 0x40000000U, false},
        {7, 1U, 1U, false, 0x80000000U, true}
    }};

    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);
    for (const auto &v : vectors) {
        SCOPED_TRACE(static_cast<int>(v.operation));
        SCOPED_TRACE(v.shift);
        execute(bus, cpu, alu(v.operation), v.value, v.shift, v.carryIn);
        ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
        EXPECT_EQ(cpu.debug_reg(0), v.result);
        EXPECT_EQ((cpu.debug_cpsr() & ARM7TDMI::kFlagC) != 0U, v.carryOut);
        EXPECT_EQ((cpu.debug_cpsr() & ARM7TDMI::kFlagZ) != 0U, v.result == 0U);
        EXPECT_EQ((cpu.debug_cpsr() & ARM7TDMI::kFlagN) != 0U, (v.result & 0x80000000U) != 0U);
        EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagV, 0U);
    }
}

TEST(ThumbRegisterAlu, CarryBorrowOverflowAndLogicalFlagPreservation) {
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    execute(bus, cpu, alu(6), 0x80000000U, 0x7FFFFFFFU, false);
    EXPECT_EQ(cpu.debug_reg(0), 0U);
    EXPECT_EQ(cpu.debug_cpsr() & kNzcv, 0x70000000U);

    execute(bus, cpu, alu(6), 0U, 0U, true);
    EXPECT_EQ(cpu.debug_reg(0), 0U);
    EXPECT_EQ(cpu.debug_cpsr() & kNzcv, 0x60000000U);

    cpu.reset();
    cpu.debug_set_program_counter(kCode);
    cpu.debug_set_reg(7, 0x80000000U);
    cpu.debug_set_reg(0, 0xF0U);
    cpu.debug_set_reg(1, 0x0FU);
    bus.write16(kCode, 0x2F01U); // CMP r7,#1: C=1,V=1
    bus.write16(kCode + 2U, alu(0));
    cpu.step();
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(0), 0U);
    EXPECT_EQ(cpu.debug_cpsr() & kNzcv, 0x70000000U);

    execute(bus, cpu, alu(2, 0U, 0U), 0x80000001U, 0U, true);
    EXPECT_EQ(cpu.debug_reg(0), 1U);
    EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagC, 0U);
}

} // namespace

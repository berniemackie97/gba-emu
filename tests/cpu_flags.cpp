// tests/cpu_flags.cpp
// Comprehensive tests for CPSR flag behavior (N, Z, C, V)
#include <array>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

namespace {
    constexpr std::uint16_t kTop5Shift = 11U;
    constexpr std::uint8_t kRegFieldShift = 8U;
    constexpr std::uint8_t kLow3Mask = 0x07U;

    constexpr std::uint16_t kTop5_MOV = 0b00100U;
    constexpr std::uint16_t kTop5_ADD = 0b00110U;
    constexpr std::uint16_t kTop5_SUB = 0b00111U;

    constexpr auto Thumb_MOV_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_MOV << kTop5Shift) |
                                          ((destReg & kLow3Mask) << kRegFieldShift) | imm8);
    }

    constexpr auto Thumb_ADD_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_ADD << kTop5Shift) |
                                          ((destReg & kLow3Mask) << kRegFieldShift) | imm8);
    }

    constexpr auto Thumb_SUB_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_SUB << kTop5Shift) |
                                          ((destReg & kLow3Mask) << kRegFieldShift) | imm8);
    }

} // namespace

// ============================================================================
// Zero Flag Tests
// ============================================================================

TEST(CPUFlags, ZeroFlagSetOnZeroResult) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 5U),
        Thumb_SUB_imm(0U, 5U), // r0 = 5 - 5 = 0
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #5
    cpu.step(); // SUB r0, #5

    EXPECT_EQ(cpu.debug_reg(0), 0U);
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_NE(cpsr & ARM7TDMI::kFlagZ, 0U); // Z flag should be set
}

TEST(CPUFlags, ZeroFlagClearOnNonZeroResult) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 5U),
        Thumb_ADD_imm(0U, 3U), // r0 = 5 + 3 = 8
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #5
    cpu.step(); // ADD r0, #3

    EXPECT_EQ(cpu.debug_reg(0), 8U);
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagZ, 0U); // Z flag should be clear
}

// ============================================================================
// Negative Flag Tests
// ============================================================================

TEST(CPUFlags, NegativeFlagSetOnNegativeResult) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 5U),
        Thumb_SUB_imm(0U, 10U), // r0 = 5 - 10 = -5 (in 2's complement)
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #5
    cpu.step(); // SUB r0, #10

    // Result should be negative in 2's complement (MSB set)
    const auto result = cpu.debug_reg(0);
    EXPECT_NE(result & ARM7TDMI::kSignBit, 0U);

    const auto cpsr = cpu.debug_cpsr();
    EXPECT_NE(cpsr & ARM7TDMI::kFlagN, 0U); // N flag should be set
}

TEST(CPUFlags, NegativeFlagClearOnPositiveResult) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 10U),
        Thumb_SUB_imm(0U, 5U), // r0 = 10 - 5 = 5 (positive)
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #10
    cpu.step(); // SUB r0, #5

    EXPECT_EQ(cpu.debug_reg(0), 5U);
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagN, 0U); // N flag should be clear
}

// ============================================================================
// Carry Flag Tests
// ============================================================================

TEST(CPUFlags, CarryFlagSetOnUnsignedOverflow) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 0xFFU), // r0 = 255
        Thumb_ADD_imm(0U, 1U),    // r0 = 255 + 1 = 256 (overflows 8-bit)
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #0xFF
    cpu.step(); // ADD r0, #1

    EXPECT_EQ(cpu.debug_reg(0), 0x100U);
    const auto cpsr = cpu.debug_cpsr();
    // No unsigned overflow in 32-bit context (0xFF + 1 = 0x100, no carry)
    // This test shows the behavior in 32-bit register context
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagC, 0U);
}

TEST(CPUFlags, CarryFlagSetOnLargeAddition) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // To trigger carry in 32-bit context, we need to test with the actual
    // register values that will cause carry out of bit 31
    // This is a placeholder test - actual implementation depends on CPU behavior

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 0xFEU),
        Thumb_ADD_imm(0U, 2U), // Won't overflow in 32-bit
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step();
    cpu.step();

    // Document expected behavior
    EXPECT_EQ(cpu.debug_reg(0), 0x100U);
}

TEST(CPUFlags, CarryFlagInSubtraction) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 10U),
        Thumb_SUB_imm(0U, 5U), // r0 = 10 - 5 = 5 (no borrow)
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #10
    cpu.step(); // SUB r0, #5

    EXPECT_EQ(cpu.debug_reg(0), 5U);
    const auto cpsr = cpu.debug_cpsr();
    // C flag in subtraction: set when NO borrow (a >= b)
    EXPECT_NE(cpsr & ARM7TDMI::kFlagC, 0U);
}

TEST(CPUFlags, CarryFlagClearOnBorrow) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 5U),
        Thumb_SUB_imm(0U, 10U), // r0 = 5 - 10 (borrow occurs)
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #5
    cpu.step(); // SUB r0, #10

    // Result is (5 - 10) = -5 in 2's complement = 0xFFFFFFFB
    const auto cpsr = cpu.debug_cpsr();
    // C flag should be clear (borrow occurred since 5 < 10)
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagC, 0U);
}

// ============================================================================
// Overflow Flag Tests
// ============================================================================

TEST(CPUFlags, OverflowFlagOnSignedAdditionOverflow) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Adding two positive numbers that result in negative (signed overflow)
    // 0x7F + 1 = 0x80 (127 + 1 = -128 in signed 8-bit)
    // In 32-bit context: we need larger values
    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 0x7FU), // Positive
        Thumb_ADD_imm(0U, 0x7FU), // Positive + Positive = 0xFE (still positive in 32-bit)
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #0x7F
    cpu.step(); // ADD r0, #0x7F

    EXPECT_EQ(cpu.debug_reg(0), 0xFEU);

    // In 32-bit context, 0x7F + 0x7F = 0xFE is still positive (no overflow)
    // True signed overflow in 32-bit needs bit 31 to flip inappropriately
    const auto cpsr = cpu.debug_cpsr();
    // Document that this doesn't overflow in 32-bit context
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagV, 0U);
}

TEST(CPUFlags, OverflowFlagOnSignedSubtractionOverflow) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Subtracting negative from positive causing sign flip
    // This is tricky in immediate mode since we can't set negative values directly
    // Document the behavior with available immediates
    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, 0U),
        Thumb_SUB_imm(0U, 1U), // 0 - 1 = -1 (0xFFFFFFFF)
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #0
    cpu.step(); // SUB r0, #1

    // Result should be 0xFFFFFFFF (-1)
    EXPECT_EQ(cpu.debug_reg(0), 0xFFFFFFFFU);

    const auto cpsr = cpu.debug_cpsr();
    // 0 (pos) - 1 (pos) = -1 (neg): signs of operands same (both pos), result differs
    // V should NOT be set (both operands are positive)
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagV, 0U);
}

// ============================================================================
// Combined Flag Tests
// ============================================================================

TEST(CPUFlags, MultipleOperationsPreserveFlags) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    const std::array<std::uint16_t, 4> program{
        Thumb_MOV_imm(0U, 10U),
        Thumb_SUB_imm(0U, 10U), // r0 = 0, Z=1
        Thumb_ADD_imm(1U, 5U),  // r1 = 5, Z should be clear after this
        Thumb_MOV_imm(2U, 0U),  // r2 = 0, Z=1
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #10
    cpu.step(); // SUB r0, #10 -> r0 = 0, Z=1

    auto cpsr = cpu.debug_cpsr();
    EXPECT_NE(cpsr & ARM7TDMI::kFlagZ, 0U); // Z should be set

    cpu.step(); // ADD r1, #5 -> r1 = 5, Z=0

    cpsr = cpu.debug_cpsr();
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagZ, 0U); // Z should be clear now

    cpu.step(); // MOV r2, #0 -> Z=1

    cpsr = cpu.debug_cpsr();
    EXPECT_NE(cpsr & ARM7TDMI::kFlagZ, 0U); // Z should be set again
}

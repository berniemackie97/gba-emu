// tests/cpu_thumb_alu.cpp
// Comprehensive tests for Thumb ALU operations (logical, shifts, comparisons)
#include <array>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

namespace {
    // Encoding constants
    constexpr std::uint16_t kTop5Shift = 11U;
    constexpr std::uint8_t kRegFieldShift = 8U;
    constexpr std::uint8_t kLow3Mask = 0x07U;

    // Thumb opcodes for ALU operations (format 1: shift by immediate)
    constexpr std::uint16_t kTop5_LSL = 0b00000U;  // Logical shift left
    constexpr std::uint16_t kTop5_LSR = 0b00001U;  // Logical shift right
    constexpr std::uint16_t kTop5_ASR = 0b00010U;  // Arithmetic shift right

    // Thumb opcodes for format 2: add/subtract register
    constexpr std::uint16_t kTop4_ADD_reg = 0b0001100U;  // ADD Rd, Rs, Rn
    constexpr std::uint16_t kTop4_SUB_reg = 0b0001101U;  // SUB Rd, Rs, Rn
    constexpr std::uint16_t kTop4_ADD_imm3 = 0b0001110U; // ADD Rd, Rs, #imm3
    constexpr std::uint16_t kTop4_SUB_imm3 = 0b0001111U; // SUB Rd, Rs, #imm3

    // Thumb opcodes for format 4: ALU operations (register-based)
    constexpr std::uint8_t kFormat4_AND = 0b0000U;
    constexpr std::uint8_t kFormat4_EOR = 0b0001U;
    constexpr std::uint8_t kFormat4_LSL = 0b0010U;
    constexpr std::uint8_t kFormat4_LSR = 0b0011U;
    constexpr std::uint8_t kFormat4_ASR = 0b0100U;
    constexpr std::uint8_t kFormat4_ADC = 0b0101U;
    constexpr std::uint8_t kFormat4_SBC = 0b0110U;
    constexpr std::uint8_t kFormat4_ROR = 0b0111U;
    constexpr std::uint8_t kFormat4_TST = 0b1000U;
    constexpr std::uint8_t kFormat4_NEG = 0b1001U;
    constexpr std::uint8_t kFormat4_CMP = 0b1010U;
    constexpr std::uint8_t kFormat4_CMN = 0b1011U;
    constexpr std::uint8_t kFormat4_ORR = 0b1100U;
    constexpr std::uint8_t kFormat4_MUL = 0b1101U;
    constexpr std::uint8_t kFormat4_BIC = 0b1110U;
    constexpr std::uint8_t kFormat4_MVN = 0b1111U;

    // Thumb opcodes for format 5: compare operations
    constexpr std::uint16_t kTop5_CMP_imm = 0b00101U; // CMP Rd, #imm8

    // Helper for MOV immediate (for test setup)
    constexpr std::uint16_t kTop5_MOV = 0b00100U;

    constexpr auto Thumb_MOV_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_MOV << kTop5Shift) |
                                          ((destReg & kLow3Mask) << kRegFieldShift) | imm8);
    }

    // Format 1: Shift by immediate
    constexpr auto Thumb_LSL_imm(std::uint8_t destReg, std::uint8_t srcReg, std::uint8_t imm5) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_LSL << kTop5Shift) | (imm5 << 6U) |
                                          ((srcReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    constexpr auto Thumb_LSR_imm(std::uint8_t destReg, std::uint8_t srcReg, std::uint8_t imm5) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_LSR << kTop5Shift) | (imm5 << 6U) |
                                          ((srcReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    constexpr auto Thumb_ASR_imm(std::uint8_t destReg, std::uint8_t srcReg, std::uint8_t imm5) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_ASR << kTop5Shift) | (imm5 << 6U) |
                                          ((srcReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    // Format 4: ALU register operations
    constexpr auto Thumb_ALU_reg(std::uint8_t op, std::uint8_t destReg, std::uint8_t srcReg) -> std::uint16_t {
        return static_cast<std::uint16_t>(0b010000U << 10U | (op << 6U) |
                                          ((srcReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    // Format 5: Compare immediate
    constexpr auto Thumb_CMP_imm(std::uint8_t reg, std::uint8_t imm8) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_CMP_imm << kTop5Shift) |
                                          ((reg & kLow3Mask) << kRegFieldShift) | imm8);
    }

} // namespace

// ============================================================================
// Shift Operations Tests (Format 1)
// ============================================================================

TEST(CPUThumbALU, LogicalShiftLeft) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 0x55U;  // 0b01010101
    constexpr std::uint8_t kShift = 2U;
    constexpr std::uint8_t kExpected = 0x54U; // 0b01010100 << 2 = 0x150 (truncated to 8-bit context)

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),
        Thumb_LSL_imm(1U, 0U, kShift), // r1 = r0 << 2
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

    cpu.step(); // MOV r0, #0x55
    cpu.step(); // LSL r1, r0, #2

    // Result should be (0x55 << 2) = 0x154
    EXPECT_EQ(cpu.debug_reg(1), 0x154U);

    // Check Z flag is not set (result != 0)
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagZ, 0U);
}

TEST(CPUThumbALU, LogicalShiftLeftToZero) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 0x80U;  // 0b10000000
    constexpr std::uint8_t kShift = 8U;       // Shift all bits out

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),
        Thumb_LSL_imm(1U, 0U, kShift), // r1 = r0 << 8
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

    cpu.step(); // MOV
    cpu.step(); // LSL

    // Result should be (0x80 << 8) = 0x8000 in a 32-bit context
    EXPECT_EQ(cpu.debug_reg(1), 0x8000U);
}

TEST(CPUThumbALU, LogicalShiftRight) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 0xAAU; // 0b10101010
    constexpr std::uint8_t kShift = 2U;
    constexpr std::uint8_t kExpected = 0x2AU; // 0b00101010

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),
        Thumb_LSR_imm(1U, 0U, kShift), // r1 = r0 >> 2
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

    cpu.step(); // MOV r0, #0xAA
    cpu.step(); // LSR r1, r0, #2

    // Result should be (0xAA >> 2) = 0x2A
    EXPECT_EQ(cpu.debug_reg(1), 0x2AU);
}

TEST(CPUThumbALU, ArithmeticShiftRight) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Test with positive value first (MSB = 0)
    const std::array<std::uint16_t, 2> program1{
        Thumb_MOV_imm(0U, 0x7FU), // Positive: 0b01111111
        Thumb_ASR_imm(1U, 0U, 2U), // r1 = r0 ASR 2
    };

    std::uint32_t addr = kBase;
    for (const auto insn : program1) {
        bus.write16(addr, insn);
        addr += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(kBase);

    cpu.step(); // MOV r0, #0x7F
    cpu.step(); // ASR r1, r0, #2

    // Result should be (0x7F >> 2) = 0x1F (sign bit doesn't extend because MSB was 0)
    EXPECT_EQ(cpu.debug_reg(1), 0x1FU);
}

// ============================================================================
// Logical Operations Tests (Format 4)
// ============================================================================

TEST(CPUThumbALU, BitwiseAND) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal1 = 0xF0U; // 0b11110000
    constexpr std::uint8_t kVal2 = 0xAAU; // 0b10101010
    constexpr std::uint8_t kExpected = 0xA0U; // 0b10100000

    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, kVal1),
        Thumb_MOV_imm(1U, kVal2),
        Thumb_ALU_reg(kFormat4_AND, 0U, 1U), // r0 = r0 AND r1
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

    cpu.step(); // MOV r0, #0xF0
    cpu.step(); // MOV r1, #0xAA
    cpu.step(); // AND r0, r1

    EXPECT_EQ(cpu.debug_reg(0), kExpected);
}

TEST(CPUThumbALU, BitwiseEOR) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal1 = 0xF0U; // 0b11110000
    constexpr std::uint8_t kVal2 = 0xAAU; // 0b10101010
    constexpr std::uint8_t kExpected = 0x5AU; // 0b01011010

    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, kVal1),
        Thumb_MOV_imm(1U, kVal2),
        Thumb_ALU_reg(kFormat4_EOR, 0U, 1U), // r0 = r0 XOR r1
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

    cpu.step(); // MOV r0, #0xF0
    cpu.step(); // MOV r1, #0xAA
    cpu.step(); // EOR r0, r1

    EXPECT_EQ(cpu.debug_reg(0), kExpected);
}

TEST(CPUThumbALU, BitwiseORR) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal1 = 0xF0U; // 0b11110000
    constexpr std::uint8_t kVal2 = 0x0FU; // 0b00001111
    constexpr std::uint8_t kExpected = 0xFFU; // 0b11111111

    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, kVal1),
        Thumb_MOV_imm(1U, kVal2),
        Thumb_ALU_reg(kFormat4_ORR, 0U, 1U), // r0 = r0 OR r1
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

    cpu.step(); // MOV r0, #0xF0
    cpu.step(); // MOV r1, #0x0F
    cpu.step(); // ORR r0, r1

    EXPECT_EQ(cpu.debug_reg(0), kExpected);
}

TEST(CPUThumbALU, BitwiseBIC) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal1 = 0xFFU; // 0b11111111
    constexpr std::uint8_t kVal2 = 0xF0U; // 0b11110000
    constexpr std::uint8_t kExpected = 0x0FU; // 0b00001111

    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, kVal1),
        Thumb_MOV_imm(1U, kVal2),
        Thumb_ALU_reg(kFormat4_BIC, 0U, 1U), // r0 = r0 AND NOT r1
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
    cpu.step(); // MOV r1, #0xF0
    cpu.step(); // BIC r0, r1

    EXPECT_EQ(cpu.debug_reg(0), kExpected);
}

TEST(CPUThumbALU, BitwiseMVN) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal = 0xAAU; // 0b10101010
    constexpr std::uint32_t kExpected = 0xFFFFFF55U; // ~0xAA in 32-bit

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kVal),
        Thumb_ALU_reg(kFormat4_MVN, 0U, 0U), // r0 = NOT r0
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

    cpu.step(); // MOV r0, #0xAA
    cpu.step(); // MVN r0, r0

    EXPECT_EQ(cpu.debug_reg(0), kExpected);
}

// ============================================================================
// Comparison Operations Tests
// ============================================================================

TEST(CPUThumbALU, CompareImmediateEqual) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal = 0x42U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kVal),
        Thumb_CMP_imm(0U, kVal), // CMP r0, #0x42
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

    cpu.step(); // MOV r0, #0x42
    cpu.step(); // CMP r0, #0x42

    const auto cpsr = cpu.debug_cpsr();
    // When equal, Z flag should be set (result of subtraction is zero)
    EXPECT_NE(cpsr & ARM7TDMI::kFlagZ, 0U);
    // N flag should be clear (result is not negative)
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagN, 0U);
}

TEST(CPUThumbALU, CompareImmediateLess) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal = 0x10U;
    constexpr std::uint8_t kCmpVal = 0x20U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kVal),
        Thumb_CMP_imm(0U, kCmpVal), // CMP r0, #0x20 (0x10 - 0x20 = negative)
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

    cpu.step(); // MOV r0, #0x10
    cpu.step(); // CMP r0, #0x20

    const auto cpsr = cpu.debug_cpsr();
    // When r0 < immediate, result is negative so N flag should be set
    EXPECT_NE(cpsr & ARM7TDMI::kFlagN, 0U);
    // Z flag should be clear (result is not zero)
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagZ, 0U);
}

TEST(CPUThumbALU, CompareImmediateGreater) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal = 0x50U;
    constexpr std::uint8_t kCmpVal = 0x20U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kVal),
        Thumb_CMP_imm(0U, kCmpVal), // CMP r0, #0x20 (0x50 - 0x20 = positive)
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

    cpu.step(); // MOV r0, #0x50
    cpu.step(); // CMP r0, #0x20

    const auto cpsr = cpu.debug_cpsr();
    // When r0 > immediate, result is positive so N flag should be clear
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagN, 0U);
    // Z flag should be clear (result is not zero)
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagZ, 0U);
}

// tests/cpu_thumb_format2.cpp
// Tests for Thumb Format 2: Add/subtract register/immediate
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
    constexpr std::uint16_t kTop7Shift = 9U;
    constexpr std::uint8_t kLow3Mask = 0x07U;

    // Format 2 opcodes (top 7 bits)
    constexpr std::uint16_t kTop7_ADD_reg = 0b0001100U;  // ADD Rd, Rs, Rn
    constexpr std::uint16_t kTop7_SUB_reg = 0b0001101U;  // SUB Rd, Rs, Rn
    constexpr std::uint16_t kTop7_ADD_imm3 = 0b0001110U; // ADD Rd, Rs, #imm3
    constexpr std::uint16_t kTop7_SUB_imm3 = 0b0001111U; // SUB Rd, Rs, #imm3

    // Helper for MOV immediate (for test setup)
    constexpr std::uint16_t kTop5_MOV = 0b00100U;

    constexpr auto Thumb_MOV_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_MOV << 11U) |
                                          ((destReg & kLow3Mask) << 8U) | imm8);
    }

    // Format 2 encoders
    constexpr auto Thumb_ADD_reg(std::uint8_t destReg, std::uint8_t srcReg, std::uint8_t offsetReg)
        -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop7_ADD_reg << kTop7Shift) |
                                          ((offsetReg & kLow3Mask) << 6U) |
                                          ((srcReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    constexpr auto Thumb_SUB_reg(std::uint8_t destReg, std::uint8_t srcReg, std::uint8_t offsetReg)
        -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop7_SUB_reg << kTop7Shift) |
                                          ((offsetReg & kLow3Mask) << 6U) |
                                          ((srcReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    constexpr auto Thumb_ADD_imm3(std::uint8_t destReg, std::uint8_t srcReg, std::uint8_t imm3) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop7_ADD_imm3 << kTop7Shift) | ((imm3 & kLow3Mask) << 6U) |
                                          ((srcReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    constexpr auto Thumb_SUB_imm3(std::uint8_t destReg, std::uint8_t srcReg, std::uint8_t imm3) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop7_SUB_imm3 << kTop7Shift) | ((imm3 & kLow3Mask) << 6U) |
                                          ((srcReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

} // namespace

// ============================================================================
// ADD Register Tests
// ============================================================================

TEST(CPUThumbFormat2, AddRegisterBasic) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal1 = 10U;
    constexpr std::uint8_t kVal2 = 15U;
    constexpr std::uint8_t kExpected = 25U;

    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, kVal1),          // r0 = 10
        Thumb_MOV_imm(1U, kVal2),          // r1 = 15
        Thumb_ADD_reg(2U, 0U, 1U), // r2 = r0 + r1
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
    cpu.step(); // MOV r1, #15
    cpu.step(); // ADD r2, r0, r1

    EXPECT_EQ(cpu.debug_reg(2), kExpected);

    // Verify flags: result is positive, non-zero
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagZ, 0U); // Not zero
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagN, 0U); // Not negative
}

TEST(CPUThumbFormat2, AddRegisterToSameRegister) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 7U;
    constexpr std::uint8_t kExpected = 14U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),    // r0 = 7
        Thumb_ADD_reg(0U, 0U, 0U), // r0 = r0 + r0 (double)
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

    cpu.step(); // MOV r0, #7
    cpu.step(); // ADD r0, r0, r0

    EXPECT_EQ(cpu.debug_reg(0), kExpected);
}

// ============================================================================
// SUB Register Tests
// ============================================================================

TEST(CPUThumbFormat2, SubRegisterBasic) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal1 = 50U;
    constexpr std::uint8_t kVal2 = 20U;
    constexpr std::uint8_t kExpected = 30U;

    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, kVal1),          // r0 = 50
        Thumb_MOV_imm(1U, kVal2),          // r1 = 20
        Thumb_SUB_reg(2U, 0U, 1U), // r2 = r0 - r1
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

    cpu.step(); // MOV r0, #50
    cpu.step(); // MOV r1, #20
    cpu.step(); // SUB r2, r0, r1

    EXPECT_EQ(cpu.debug_reg(2), kExpected);

    // Verify flags: result is positive, no borrow
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagZ, 0U); // Not zero
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagN, 0U); // Not negative
    EXPECT_NE(cpsr & ARM7TDMI::kFlagC, 0U); // Carry set (no borrow: 50 >= 20)
}

TEST(CPUThumbFormat2, SubRegisterWithBorrow) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal1 = 10U;
    constexpr std::uint8_t kVal2 = 20U;

    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, kVal1),          // r0 = 10
        Thumb_MOV_imm(1U, kVal2),          // r1 = 20
        Thumb_SUB_reg(2U, 0U, 1U), // r2 = r0 - r1 (10 - 20 = -10)
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
    cpu.step(); // MOV r1, #20
    cpu.step(); // SUB r2, r0, r1

    // Result should be negative (2's complement)
    const auto result = cpu.debug_reg(2);
    EXPECT_NE(result & ARM7TDMI::kSignBit, 0U); // MSB set (negative)

    // Verify flags
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_NE(cpsr & ARM7TDMI::kFlagN, 0U); // Negative flag set
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagC, 0U); // Carry clear (borrow: 10 < 20)
}

TEST(CPUThumbFormat2, SubRegisterResultZero) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kVal = 42U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kVal),           // r0 = 42
        Thumb_SUB_reg(1U, 0U, 0U), // r1 = r0 - r0 = 0
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

    cpu.step(); // MOV r0, #42
    cpu.step(); // SUB r1, r0, r0

    EXPECT_EQ(cpu.debug_reg(1), 0U);

    // Verify Zero flag is set
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_NE(cpsr & ARM7TDMI::kFlagZ, 0U); // Zero flag set
}

// ============================================================================
// ADD Immediate (3-bit) Tests
// ============================================================================

TEST(CPUThumbFormat2, AddImm3Basic) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 10U;
    constexpr std::uint8_t kImm = 5U;
    constexpr std::uint8_t kExpected = 15U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),    // r0 = 10
        Thumb_ADD_imm3(1U, 0U, kImm), // r1 = r0 + 5
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
    cpu.step(); // ADD r1, r0, #5

    EXPECT_EQ(cpu.debug_reg(1), kExpected);
}

TEST(CPUThumbFormat2, AddImm3MaxValue) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 10U;
    constexpr std::uint8_t kMaxImm3 = 7U; // 3-bit max
    constexpr std::uint8_t kExpected = 17U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),       // r0 = 10
        Thumb_ADD_imm3(1U, 0U, kMaxImm3), // r1 = r0 + 7
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
    cpu.step(); // ADD r1, r0, #7

    EXPECT_EQ(cpu.debug_reg(1), kExpected);
}

TEST(CPUThumbFormat2, AddImm3Zero) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 42U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),   // r0 = 42
        Thumb_ADD_imm3(1U, 0U, 0U), // r1 = r0 + 0
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

    cpu.step(); // MOV r0, #42
    cpu.step(); // ADD r1, r0, #0

    // Should just copy r0 to r1
    EXPECT_EQ(cpu.debug_reg(1), kInitVal);
}

// ============================================================================
// SUB Immediate (3-bit) Tests
// ============================================================================

TEST(CPUThumbFormat2, SubImm3Basic) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 20U;
    constexpr std::uint8_t kImm = 3U;
    constexpr std::uint8_t kExpected = 17U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),    // r0 = 20
        Thumb_SUB_imm3(1U, 0U, kImm), // r1 = r0 - 3
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

    cpu.step(); // MOV r0, #20
    cpu.step(); // SUB r1, r0, #3

    EXPECT_EQ(cpu.debug_reg(1), kExpected);
}

TEST(CPUThumbFormat2, SubImm3ResultZero) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 5U;

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),       // r0 = 5
        Thumb_SUB_imm3(1U, 0U, kInitVal), // r1 = r0 - 5 = 0
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
    cpu.step(); // SUB r1, r0, #5

    EXPECT_EQ(cpu.debug_reg(1), 0U);

    // Verify Zero flag is set
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_NE(cpsr & ARM7TDMI::kFlagZ, 0U);
}

TEST(CPUThumbFormat2, SubImm3WithBorrow) {
    Bus bus;
    bus.reset();

    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint8_t kInitVal = 2U;
    constexpr std::uint8_t kImm = 7U; // Larger than init value

    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(0U, kInitVal),    // r0 = 2
        Thumb_SUB_imm3(1U, 0U, kImm), // r1 = r0 - 7 (2 - 7 = -5)
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

    cpu.step(); // MOV r0, #2
    cpu.step(); // SUB r1, r0, #7

    // Result should be negative
    const auto result = cpu.debug_reg(1);
    EXPECT_NE(result & ARM7TDMI::kSignBit, 0U);

    // Verify flags
    const auto cpsr = cpu.debug_cpsr();
    EXPECT_NE(cpsr & ARM7TDMI::kFlagN, 0U); // Negative flag set
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagC, 0U); // Carry clear (borrow occurred)
}

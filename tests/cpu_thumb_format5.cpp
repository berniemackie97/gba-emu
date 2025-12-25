// tests/cpu_thumb_format5.cpp
// Comprehensive tests for Thumb Format 5: High register operations and BX
#include <array>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

namespace {
    // Encoding helpers for Format 5 instructions
    // Format: 01000_1xx_h1_h2_Rs_Rd
    // Bits 8-9: opcode (00=ADD, 01=CMP, 10=MOV, 11=BX)
    // Bit 7 (h1): destination is high register (r8-r15)
    // Bit 6 (h2): source is high register (r8-r15)
    // Bits 3-5: source register (low 3 bits)
    // Bits 0-2: destination register (low 3 bits)

    constexpr std::uint16_t Thumb_ADD_high(std::uint8_t destReg, std::uint8_t srcReg) {
        const std::uint16_t h1 = (destReg >> 3U) & 0x1U; // bit 3 of destReg
        const std::uint16_t h2 = (srcReg >> 3U) & 0x1U;  // bit 3 of srcReg
        const std::uint16_t destLow = destReg & 0x7U;
        const std::uint16_t srcLow = srcReg & 0x7U;
        return 0x4400U | (h1 << 7U) | (h2 << 6U) | (srcLow << 3U) | destLow;
    }

    constexpr std::uint16_t Thumb_CMP_high(std::uint8_t destReg, std::uint8_t srcReg) {
        const std::uint16_t h1 = (destReg >> 3U) & 0x1U;
        const std::uint16_t h2 = (srcReg >> 3U) & 0x1U;
        const std::uint16_t destLow = destReg & 0x7U;
        const std::uint16_t srcLow = srcReg & 0x7U;
        return 0x4500U | (h1 << 7U) | (h2 << 6U) | (srcLow << 3U) | destLow;
    }

    constexpr std::uint16_t Thumb_MOV_high(std::uint8_t destReg, std::uint8_t srcReg) {
        const std::uint16_t h1 = (destReg >> 3U) & 0x1U;
        const std::uint16_t h2 = (srcReg >> 3U) & 0x1U;
        const std::uint16_t destLow = destReg & 0x7U;
        const std::uint16_t srcLow = srcReg & 0x7U;
        return 0x4600U | (h1 << 7U) | (h2 << 6U) | (srcLow << 3U) | destLow;
    }

    constexpr std::uint16_t Thumb_BX(std::uint8_t srcReg) {
        const std::uint16_t h2 = (srcReg >> 3U) & 0x1U;
        const std::uint16_t srcLow = srcReg & 0x7U;
        return 0x4700U | (h2 << 6U) | (srcLow << 3U);
    }

    constexpr std::uint16_t Thumb_MOV_imm(std::uint8_t rd, std::uint8_t imm8) {
        return static_cast<std::uint16_t>(0x2000U | (static_cast<std::uint16_t>(rd & 0x7U) << 8U) | imm8);
    }

} // namespace

// ==================== ADD (High Register) Tests ====================

TEST(CPUThumbFormat5, AddHighRegisterToLowRegister) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r0 = 10, r8 = 20, then ADD r0, r8
    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, 10U),              // r0 = 10
        Thumb_MOV_high(8U, 0U),              // r8 = r0 (= 10)
        Thumb_ADD_high(0U, 8U),              // r0 = r0 + r8 (= 20)
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

    // Execute: MOV r0, #10
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(0), 10U);

    // Execute: MOV r8, r0
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(8), 10U);

    // Execute: ADD r0, r8
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(0), 20U);
    EXPECT_EQ(cpu.debug_reg(8), 10U);
}

TEST(CPUThumbFormat5, AddLowRegisterToHighRegister) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r1 = 15, r9 = 25, then ADD r9, r1
    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(1U, 15U),              // r1 = 15
        Thumb_MOV_high(9U, 1U),              // r9 = r1 (= 15)
        Thumb_ADD_high(9U, 1U),              // r9 = r9 + r1 (= 30)
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

    cpu.step(); // MOV r1, #15
    cpu.step(); // MOV r9, r1
    EXPECT_EQ(cpu.debug_reg(9), 15U);

    cpu.step(); // ADD r9, r1
    EXPECT_EQ(cpu.debug_reg(9), 30U);
    EXPECT_EQ(cpu.debug_reg(1), 15U);
}

TEST(CPUThumbFormat5, AddHighToHighRegister) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r10 = 100, r11 = 50, then ADD r10, r11
    const std::array<std::uint16_t, 4> program{
        Thumb_MOV_imm(0U, 100U),             // r0 = 100
        Thumb_MOV_high(10U, 0U),             // r10 = r0
        Thumb_MOV_imm(1U, 50U),              // r1 = 50
        Thumb_ADD_high(10U, 11U),            // r10 = r10 + r11 (but r11 is 0, so stays 100)
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

    cpu.step(); // r0 = 100
    cpu.step(); // r10 = 100
    cpu.step(); // r1 = 50

    // Note: r11 is uninitialized (0), so result is 100 + 0 = 100
    cpu.step(); // ADD r10, r11
    EXPECT_EQ(cpu.debug_reg(10), 100U);
}

TEST(CPUThumbFormat5, AddDoesNotAffectFlags) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Hardware spec: ADD (high register form) does NOT affect flags
    const std::array<std::uint16_t, 4> program{
        Thumb_MOV_imm(0U, 0U),               // r0 = 0, sets Z flag
        Thumb_MOV_imm(1U, 100U),             // r1 = 100, clears Z flag, clears N flag
        Thumb_MOV_high(8U, 1U),              // r8 = 100
        Thumb_ADD_high(8U, 1U),              // r8 = 200, should NOT affect flags
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

    cpu.step(); // r0 = 0, Z flag set
    cpu.step(); // r1 = 100, Z flag clear, N flag clear
    const std::uint32_t cpsrBeforeAdd = cpu.debug_cpsr();
    EXPECT_EQ(cpsrBeforeAdd & ARM7TDMI::kFlagZ, 0U) << "Z should be clear after MOV #100";
    EXPECT_EQ(cpsrBeforeAdd & ARM7TDMI::kFlagN, 0U) << "N should be clear after MOV #100";

    cpu.step(); // r8 = 100 (MOV high does not affect flags)
    cpu.step(); // ADD r8, r1 (ADD high does not affect flags)

    // Flags should be unchanged
    EXPECT_EQ(cpu.debug_cpsr(), cpsrBeforeAdd) << "ADD high should not affect flags";
    EXPECT_EQ(cpu.debug_reg(8), 200U);
}

// ==================== CMP (High Register) Tests ====================

TEST(CPUThumbFormat5, CmpHighRegisterEqual) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r8 = 42, r0 = 42, then CMP r8, r0 (should set Z flag)
    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, 42U),              // r0 = 42
        Thumb_MOV_high(8U, 0U),              // r8 = 42
        Thumb_CMP_high(8U, 0U),              // CMP r8, r0 (42 - 42 = 0)
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

    cpu.step(); // r0 = 42
    cpu.step(); // r8 = 42
    cpu.step(); // CMP r8, r0

    const std::uint32_t cpsr = cpu.debug_cpsr();
    // Result is zero, so Z should be set, N clear
    EXPECT_NE(cpsr & ARM7TDMI::kFlagZ, 0U) << "Z flag should be set (equal)";
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagN, 0U) << "N flag should be clear (positive)";
}

TEST(CPUThumbFormat5, CmpHighRegisterLess) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r9 = 10, r1 = 20, then CMP r9, r1 (10 - 20 = negative)
    const std::array<std::uint16_t, 4> program{
        Thumb_MOV_imm(0U, 10U),              // r0 = 10
        Thumb_MOV_high(9U, 0U),              // r9 = 10
        Thumb_MOV_imm(1U, 20U),              // r1 = 20
        Thumb_CMP_high(9U, 1U),              // CMP r9, r1 (10 - 20)
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

    cpu.step(); // r0 = 10
    cpu.step(); // r9 = 10
    cpu.step(); // r1 = 20
    cpu.step(); // CMP r9, r1

    const std::uint32_t cpsr = cpu.debug_cpsr();
    // 10 - 20 = negative (borrow occurs), so N set, C clear
    EXPECT_NE(cpsr & ARM7TDMI::kFlagN, 0U) << "N flag should be set (negative result)";
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagC, 0U) << "C flag should be clear (borrow)";
}

TEST(CPUThumbFormat5, CmpHighRegisterGreater) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r10 = 50, r2 = 30, then CMP r10, r2 (50 - 30 = positive)
    const std::array<std::uint16_t, 4> program{
        Thumb_MOV_imm(0U, 50U),              // r0 = 50
        Thumb_MOV_high(10U, 0U),             // r10 = 50
        Thumb_MOV_imm(2U, 30U),              // r2 = 30
        Thumb_CMP_high(10U, 2U),             // CMP r10, r2 (50 - 30 = 20)
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

    cpu.step(); // r0 = 50
    cpu.step(); // r10 = 50
    cpu.step(); // r2 = 30
    cpu.step(); // CMP r10, r2

    const std::uint32_t cpsr = cpu.debug_cpsr();
    // 50 - 30 = 20 (positive, no borrow), so N clear, C set, Z clear
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagN, 0U) << "N flag should be clear (positive result)";
    EXPECT_NE(cpsr & ARM7TDMI::kFlagC, 0U) << "C flag should be set (no borrow)";
    EXPECT_EQ(cpsr & ARM7TDMI::kFlagZ, 0U) << "Z flag should be clear (non-zero)";
}

// ==================== MOV (High Register) Tests ====================

TEST(CPUThumbFormat5, MovLowToHighRegister) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r3 = 123, then MOV r11, r3
    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(3U, 123U),             // r3 = 123
        Thumb_MOV_high(11U, 3U),             // r11 = r3
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

    cpu.step(); // r3 = 123
    cpu.step(); // MOV r11, r3

    EXPECT_EQ(cpu.debug_reg(11), 123U);
    EXPECT_EQ(cpu.debug_reg(3), 123U);
}

TEST(CPUThumbFormat5, MovHighToLowRegister) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r12 = 200, then MOV r4, r12
    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, 200U),             // r0 = 200
        Thumb_MOV_high(12U, 0U),             // r12 = 200
        Thumb_MOV_high(4U, 12U),             // r4 = r12
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

    cpu.step(); // r0 = 200
    cpu.step(); // r12 = 200
    cpu.step(); // MOV r4, r12

    EXPECT_EQ(cpu.debug_reg(4), 200U);
    EXPECT_EQ(cpu.debug_reg(12), 200U);
}

TEST(CPUThumbFormat5, MovHighToHighRegister) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Setup: r13 (SP) = 150, then MOV r14 (LR), r13
    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, 150U),             // r0 = 150
        Thumb_MOV_high(13U, 0U),             // r13 (SP) = 150
        Thumb_MOV_high(14U, 13U),            // r14 (LR) = r13
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

    cpu.step(); // r0 = 150
    cpu.step(); // r13 = 150
    cpu.step(); // MOV r14, r13

    EXPECT_EQ(cpu.debug_reg(14), 150U);
    EXPECT_EQ(cpu.debug_reg(13), 150U);
}

TEST(CPUThumbFormat5, MovDoesNotAffectFlags) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Hardware spec: MOV (high register form) does NOT affect flags
    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, 0U),               // r0 = 0, sets Z flag
        Thumb_MOV_imm(1U, 255U),             // r1 = 255, clears Z, sets N (for signed interpretation)
        Thumb_MOV_high(8U, 1U),              // r8 = 255, should NOT affect flags
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

    cpu.step(); // r0 = 0, Z flag set
    cpu.step(); // r1 = 255, Z clear, N clear (255 is positive in unsigned)
    const std::uint32_t cpsrBeforeMov = cpu.debug_cpsr();
    EXPECT_EQ(cpsrBeforeMov & ARM7TDMI::kFlagZ, 0U) << "Z should be clear after MOV #255";

    cpu.step(); // MOV r8, r1

    // Flags should be unchanged by MOV high
    EXPECT_EQ(cpu.debug_cpsr(), cpsrBeforeMov) << "MOV high should not affect flags";
    EXPECT_EQ(cpu.debug_reg(8), 255U);
}

// ==================== BX (Branch and Exchange) Tests ====================

TEST(CPUThumbFormat5, BxStaysInThumbMode) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint32_t kTarget = kBase + 0x100U;

    // Setup: r8 = target address with bit 0 set (Thumb mode), then BX r8
    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, (kTarget & 0xFFU) | 0x1U),  // r0 = low byte of target | 1
        Thumb_MOV_high(8U, 0U),                       // r8 = target (with bit 0 set)
        Thumb_BX(8U),                                 // BX r8
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

    cpu.step(); // r0 = target | 1
    cpu.step(); // r8 = target | 1
    EXPECT_NE(cpu.debug_reg(8) & 0x1U, 0U) << "Bit 0 should be set";

    cpu.step(); // BX r8

    // PC should be at target address (bit 0 cleared for alignment)
    EXPECT_EQ(cpu.debug_pc() & ~0x1U, cpu.debug_reg(8) & ~0x1U);

    // T flag should still be set (Thumb mode)
    EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U) << "Should stay in Thumb mode";
}

TEST(CPUThumbFormat5, BxSwitchesToArmMode) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint32_t kTarget = kBase + 0x200U;

    // Setup: r9 = target address with bit 0 clear (ARM mode), then BX r9
    const std::array<std::uint16_t, 3> program{
        Thumb_MOV_imm(0U, (kTarget & 0xFCU)),  // r0 = target (word aligned, bit 0 clear)
        Thumb_MOV_high(9U, 0U),                // r9 = target
        Thumb_BX(9U),                          // BX r9 - should switch to ARM
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

    cpu.step(); // r0 = target (aligned)
    cpu.step(); // r9 = target
    EXPECT_EQ(cpu.debug_reg(9) & 0x1U, 0U) << "Bit 0 should be clear";

    cpu.step(); // BX r9

    // PC should be at target address (word aligned)
    EXPECT_EQ(cpu.debug_pc() & ~0x3U, cpu.debug_reg(9) & ~0x3U);

    // T flag should be clear (ARM mode)
    EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U) << "Should switch to ARM mode";
}

TEST(CPUThumbFormat5, BxFromLowRegister) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;
    constexpr std::uint32_t kTarget = kBase + 0x50U;

    // Test BX with low register (r0-r7)
    const std::array<std::uint16_t, 2> program{
        Thumb_MOV_imm(5U, (kTarget & 0xFFU) | 0x1U),  // r5 = target | 1
        Thumb_BX(5U),                                 // BX r5
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

    cpu.step(); // r5 = target | 1
    const std::uint32_t targetAddr = cpu.debug_reg(5);

    cpu.step(); // BX r5

    // PC should be at target (aligned)
    EXPECT_EQ(cpu.debug_pc(), targetAddr & ~0x1U);
    EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U) << "Should stay in Thumb mode";
}

TEST(CPUThumbFormat5, BxAlignmentHandling) {
    Bus bus;
    bus.reset();
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Test that BX properly aligns addresses
    // Thumb: halfword aligned (clear bit 0)
    // ARM: word aligned (clear bits 0-1)
    const std::array<std::uint16_t, 4> program{
        Thumb_MOV_imm(0U, 0x03U),            // r0 = 3 (unaligned, Thumb bit set)
        Thumb_MOV_high(10U, 0U),             // r10 = 3
        Thumb_BX(10U),                       // BX r10 - bit 0 set, so Thumb
        0x0000U,                             // padding
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

    cpu.step(); // r0 = 3
    cpu.step(); // r10 = 3
    cpu.step(); // BX r10

    // PC should be aligned to halfword (bit 0 cleared)
    EXPECT_EQ(cpu.debug_pc() & 0x1U, 0U) << "PC should be halfword aligned";
    EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U) << "Should be in Thumb mode";
}

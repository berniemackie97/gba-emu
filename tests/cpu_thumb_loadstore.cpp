// tests/cpu_thumb_loadsore
#include <array>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

namespace {
    // Bit/field layout
    constexpr std::uint16_t kTop5Shift = 11U;   // position of the 5-bit opcode
    constexpr std::uint8_t kRegFieldShift = 8U; // Rd field in low-reg encodings
    constexpr std::uint8_t kImm5Shift = 6U;     // imm5 position in STR/LDR (word)
    constexpr std::uint8_t kLow3Mask = 0x07U;
    constexpr std::uint8_t kImm5Mask = 0x1FU;

    // Top-5-bit opcodes (binary for readability)
    constexpr std::uint16_t kTop5_MOV_imm = 0b00100U;
    constexpr std::uint16_t kTop5_ADD_imm = 0b00110U;
    constexpr std::uint16_t kTop5_SUB_imm = 0b00111U;
    constexpr std::uint16_t kTop5_LDR_literal = 0b01001U;
    constexpr std::uint16_t kTop5_STR_imm_w = 0b01100U;
    constexpr std::uint16_t kTop5_LDR_imm_w = 0b01101U;

    // Data
    constexpr std::uint32_t kLiteralValue = 0x03000000U; // IWRAM base
    constexpr std::uint8_t kAnswer = 0x2AU;              // value we store/load
    constexpr std::size_t kNumInsns = 4U + 1U;           // 4 insns + literal pool word

    // Encoders used by the test

    constexpr auto Thumb_LDR_literal(std::uint8_t destReg, std::uint8_t imm8_words) -> std::uint16_t {
        // 01001 Rd, [PC, #imm8*4]
        return static_cast<std::uint16_t>((kTop5_LDR_literal << kTop5Shift) |
                                          ((destReg & kLow3Mask) << kRegFieldShift) | imm8_words);
    }

    constexpr auto Thumb_STR_imm_w(std::uint8_t destReg, std::uint8_t baseReg, std::uint8_t imm5_words)
        -> std::uint16_t {
        // 01100 Rd, [Rb, #imm5*4]
        return static_cast<std::uint16_t>((kTop5_STR_imm_w << kTop5Shift) | ((imm5_words & kImm5Mask) << kImm5Shift) |
                                          ((baseReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    constexpr auto Thumb_LDR_imm_w(std::uint8_t destReg, std::uint8_t baseReg, std::uint8_t imm5_words)
        -> std::uint16_t {
        // 01101 Rd, [Rb, #imm5*4]
        return static_cast<std::uint16_t>((kTop5_LDR_imm_w << kTop5Shift) | ((imm5_words & kImm5Mask) << kImm5Shift) |
                                          ((baseReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

    constexpr auto Thumb_MOV_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        // 00100 Rd, #imm8
        return static_cast<std::uint16_t>((kTop5_MOV_imm << kTop5Shift) | ((destReg & kLow3Mask) << kRegFieldShift) |
                                          imm8);
    }

} // namespace

TEST(CPUThumbLoadStore, LdrLiteralThenStoreAndLoadWord) {
    Bus bus;
    bus.reset();

    // Code will:
    //  1) LDR r1, =0x03000000 (via literal)
    //  2) MOV r0, #0x2A
    //  3) STR r0, [r1, #4]
    //  4) LDR r2, [r1, #4]
    constexpr std::uint8_t kAnswer = 0x2AU;

    // Exactly 4 instructions — the literal pool word is written separately at the end.
    constexpr std::size_t kInstrCount = 4;

    const std::array<std::uint16_t, kInstrCount> code{
        Thumb_LDR_literal(/*destReg*/ 1U, /*imm8_words*/ 1U),               // PC+4 (aligned) + 1*4 -> literal
        Thumb_MOV_imm(/*destReg*/ 0U, kAnswer),                             // r0 = 42
        Thumb_STR_imm_w(/*destReg*/ 0U, /*baseReg*/ 1U, /*imm5_words*/ 2U), // [r1 + 8] = r0  (safe: doesn’t hit code)
        Thumb_LDR_imm_w(/*destReg*/ 2U, /*baseReg*/ 1U, /*imm5_words*/ 2U), // r2 = [r1 + 8]
    };

    // Lay down the code in IWRAM
    constexpr std::uint32_t base = MMU::IWRAM_BASE;
    std::uint32_t addr = base;
    for (const auto insn : code) {
        bus.write16(addr, insn);
        addr += 2U;
    }
    // literal word at the end (word-aligned, as PC-relative LDR requires)
    bus.write32(addr, kLiteralValue); // 'addr' == base + 8 here

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base);

    cpu.step(); // LDR literal -> r1 = 0x03000000
    cpu.step(); // MOV r0, #42
    cpu.step(); // STR [r1+4] = r0
    cpu.step(); // LDR r2 = [r1+4]

    EXPECT_EQ(cpu.debug_reg(1), kLiteralValue);
    EXPECT_EQ(cpu.debug_reg(2), static_cast<std::uint32_t>(kAnswer));
}

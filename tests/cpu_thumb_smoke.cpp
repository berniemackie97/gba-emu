// tests/cpu_thumb_smoke.cpp
#include <array>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

namespace {
    // Bit/field constants to avoid "magic".
    constexpr std::uint16_t kThumbTop5Shift = 11U; // position of the 5-bit opcode field
    constexpr std::uint8_t kLow3RegMask = 0x07U;   // Rd in low regs encodings
    constexpr std::uint16_t kImm11Mask = 0x07FFU;
    constexpr std::uint8_t kRegFieldShift = 8U; // position of Rd in low-reg forms
    constexpr std::uint16_t kTop5_MOV = 0b00100U;
    constexpr std::uint16_t kTop5_ADD = 0b00110U;
    constexpr std::uint16_t kTop5_SUB = 0b00111U;
    constexpr std::uint16_t kTop5_B = 0b11100U;

    // Thumb encoding helpers (self-documenting, no magic)
    constexpr auto Thumb_MOV_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        // 00100 Rd, #imm8
        // MOV
        return static_cast<std::uint16_t>((kTop5_MOV << kThumbTop5Shift) |
                                          ((destReg & kLow3RegMask) << kRegFieldShift) | imm8);
    }
    constexpr auto Thumb_ADD_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        // 00110 Rd, #imm8
        // ADD
        return static_cast<std::uint16_t>((kTop5_ADD << kThumbTop5Shift) |
                                          ((destReg & kLow3RegMask) << kRegFieldShift) | imm8);
    }
    constexpr auto Thumb_SUB_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        // 00111 Rd, #imm8
        // SUB
        return static_cast<std::uint16_t>((kTop5_SUB << kThumbTop5Shift) |
                                          ((destReg & kLow3RegMask) << kRegFieldShift) | imm8);
    }
    constexpr auto Thumb_B_off11(std::int16_t offsetBytes) -> std::uint16_t {
        // offBytes must be even; imm11 holds signed (offBytes >> 1)
        const auto imm11 = static_cast<std::uint16_t>((offsetBytes >> 1) & kImm11Mask);
        return static_cast<std::uint16_t>((kTop5_B << kThumbTop5Shift) | imm11);
    }
} // namespace

TEST(CPUThumb, MovAddSubAndBranch) {
    Bus bus;
    bus.reset();

    // Program in IWRAM (Thumb fetches 16-bit values).
    constexpr std::uint32_t kBase = MMU::IWRAM_BASE;

    // Use std::array and a range write to appease clang-tidy about indexing/C arrays.
    const std::array<std::uint16_t, 6> program{
        Thumb_MOV_imm(0U, 5U),    // r0 = 5
        Thumb_ADD_imm(0U, 3U),    // r0 = 8
        Thumb_SUB_imm(0U, 2U),    // r0 = 6
        Thumb_B_off11(2),         // skip next instruction (+2 bytes)
        Thumb_ADD_imm(0U, 0x7FU), // skipped
        Thumb_SUB_imm(0U, 0U),    // landing
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

    // Execute 5 instructions (landing included)
    cpu.step(); // MOV
    cpu.step(); // ADD
    cpu.step(); // SUB
    cpu.step(); // B (skips next)
    cpu.step(); // SUB 0

    EXPECT_EQ(cpu.debug_reg(0), 6U);
    // After 5 steps PC advanced 5*2 and branch added +2 => base + 12
    EXPECT_EQ(cpu.debug_pc(), kBase + 12U);
}

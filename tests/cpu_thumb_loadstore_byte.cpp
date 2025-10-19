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
    constexpr std::uint16_t kTop5Shift = 11U;
    constexpr std::uint8_t kRegFieldShift = 8U;
    constexpr std::uint8_t kImm5Shift = 6U;
    constexpr std::uint8_t kLow3Mask = 0x07U;
    constexpr std::uint8_t kImm5Mask = 0x1FU;

    // Top-5-bit opcodes (binary for readability)
    constexpr std::uint16_t kTop5_MOV_imm = 0b00100U;
    constexpr std::uint16_t kTop5_STRB_imm = 0b01110U;
    constexpr std::uint16_t kTop5_LDRB_imm = 0b01111U;

    // Test data/constants
    constexpr std::uint32_t kPrimedWord = 0x11223344U; // initial 32-bit word at base
    constexpr std::uint32_t kCodeOffset = 0x100U;      // keep code separate from data
    constexpr std::uint8_t kMsbOffset = 3U;            // byte lane at [addr + 3] in little-endian

    // Encoders

    constexpr auto Thumb_MOV_imm(std::uint8_t destReg, std::uint8_t imm8) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_MOV_imm << kTop5Shift) | ((destReg & kLow3Mask) << kRegFieldShift) |
                                          imm8);
    }
    constexpr auto Thumb_STRB_imm(std::uint8_t destReg, std::uint8_t baseReg, std::uint8_t imm5) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_STRB_imm << kTop5Shift) | ((imm5 & kImm5Mask) << kImm5Shift) |
                                          ((baseReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }
    constexpr auto Thumb_LDRB_imm(std::uint8_t destReg, std::uint8_t baseReg, std::uint8_t imm5) -> std::uint16_t {
        return static_cast<std::uint16_t>((kTop5_LDRB_imm << kTop5Shift) | ((imm5 & kImm5Mask) << kImm5Shift) |
                                          ((baseReg & kLow3Mask) << 3U) | (destReg & kLow3Mask));
    }

} // namespace

TEST(CPUThumbLoadStoreByte, StrbThenLdrbImmediate) {
    Bus bus;
    bus.reset();

    // Primed word so we can see only one byte change (little-endian).
    constexpr std::uint32_t base = MMU::IWRAM_BASE;
    bus.write32(base, kPrimedWord);

    //  r1 := base (via debug hook), then code writes/reads at [r1 + 3]
    constexpr std::uint8_t kByte = 0xABU;
    const std::array<std::uint16_t, 3> code{
        Thumb_MOV_imm(/*rd*/ 0U, kByte),                   // r0 = 0xAB
        Thumb_STRB_imm(/*rd*/ 0U, /*rb*/ 1U, /*imm5*/ 3U), // [r1+3] = r0 (MSB of the 32-bit word)
        Thumb_LDRB_imm(/*rd*/ 2U, /*rb*/ 1U, /*imm5*/ 3U), // r2 = [r1+3]
    };

    // Place code
    std::uint32_t programCounter = base + kCodeOffset; // keep data and code separate
    for (const auto insn : code) {
        bus.write16(programCounter, insn);
        programCounter += 2U;
    }

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(base + kCodeOffset);
    cpu.debug_set_reg(1, base); // r1 = base

    cpu.step(); // MOV
    cpu.step(); // STRB
    cpu.step(); // LDRB

    // The MSB should now be 0xAB, rest unchanged -> 0xAB223344
    EXPECT_EQ(bus.read32(base), 0xAB223344U);
    EXPECT_EQ(cpu.debug_reg(2), static_cast<std::uint32_t>(kByte));
}

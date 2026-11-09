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

    constexpr std::uint32_t kCode = MMU::IWRAM_BASE + 0x2000U;

    TEST(ThumbControlFlow, BranchWithLinkCallsAndReturnsThroughBx) {
        constexpr std::array<std::uint16_t, 6> program{
            0xF000U, // BL high half: PC+4
            0xF802U, // BL low half: PC+4
            0x2207U, // MOVS r2,#7 (return address)
            0x46C0U, // MOV r8,r8
            0x202AU, // MOVS r0,#42 (subroutine)
            0x4770U  // BX lr
        };
        Bus bus;
        bus.reset();
        for (std::size_t i = 0; i < program.size(); ++i) {
            bus.write16(kCode + static_cast<std::uint32_t>(i * 2U), program[i]);
        }
        ARM7TDMI cpu;
        cpu.attach(bus);
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegLR), kCode + 4U);
        EXPECT_EQ(cpu.debug_pc(), kCode + 2U);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), kCode + 8U);
        EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegLR), (kCode + 4U) | 1U);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(0), 42U);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), kCode + 4U);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(2), 7U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ThumbControlFlow, LongBranchUsesSignedUpperOffset) {
        struct Vector {
            std::uint16_t upper;
            std::uint16_t lower;
            std::uint32_t destination;
        };
        constexpr std::array<Vector, 3> vectors{{
            {0xF000U, 0xF800U, kCode + 4U},
            {0xF7FFU, 0xF800U, kCode + 4U - 4096U},
            {0xF3FFU, 0xFFFFU, kCode + 4U + 0x3FF000U + 4094U}
        }};
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        for (const auto &v : vectors) {
            cpu.reset();
            cpu.debug_set_program_counter(kCode);
            bus.write16(kCode, v.upper);
            bus.write16(kCode + 2U, v.lower);
            cpu.step();
            cpu.step();
            EXPECT_EQ(cpu.debug_pc(), v.destination);
            EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegLR), (kCode + 4U) | 1U);
            EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
            EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
        }
    }

    TEST(ThumbControlFlow, HighRegisterPcOperandAndWritesUseArchitecturalPc) {
        constexpr std::uint32_t target = kCode + 0x100U;
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);

        bus.write16(kCode, 0x4678U); // MOV r0,pc
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(0), kCode + 4U);

        bus.write16(kCode, 0x4487U); // ADD pc,r0
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(0, 3U);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), kCode + 6U);
        EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);

        bus.write16(kCode, 0x4687U); // MOV pc,r0
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(0, target | 1U);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), target);
        EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);

        bus.write16(kCode, 0x4778U); // BX pc
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), kCode + 4U);
        EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U);
    }

    TEST(ThumbControlFlow, InvalidBranchExchangeBitsFaultWithPreciseInstructionAddress) {
        constexpr std::array<std::uint16_t, 2> invalid{0x4701U, 0x4780U};
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        for (const auto opcode : invalid) {
            bus.write16(kCode, opcode);
            cpu.reset();
            cpu.debug_set_program_counter(kCode);
            cpu.step();
            EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::UndefinedThumb);
            EXPECT_EQ(cpu.execution_fault().pc, kCode);
            EXPECT_EQ(cpu.execution_fault().opcode, opcode);
            EXPECT_EQ(cpu.debug_pc(), kCode);
        }
    }

} // namespace

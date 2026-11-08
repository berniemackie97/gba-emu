#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"
#include "support/thumb_program.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

TEST(ThumbStack, PushStoresAscendingRegistersOnDescendingStack) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t stack = MMU::IWRAM_BASE + 0x1000U;

    Bus bus;
    bus.reset();
    bus.write16(code, 0xB585U); // PUSH {r0,r2,r7,lr}
    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);
    cpu.debug_set_reg(0, 0x11223344U);
    cpu.debug_set_reg(2, 0x55667788U);
    cpu.debug_set_reg(7, 0x99AABBCCU);
    cpu.debug_set_reg(ARM7TDMI::kRegLR, 0x03002101U);

    const auto flags = cpu.debug_cpsr();
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack - 16U);
    EXPECT_EQ(bus.read32(stack - 16U), 0x11223344U);
    EXPECT_EQ(bus.read32(stack - 12U), 0x55667788U);
    EXPECT_EQ(bus.read32(stack - 8U), 0x99AABBCCU);
    EXPECT_EQ(bus.read32(stack - 4U), 0x03002101U);
    EXPECT_EQ(cpu.debug_cpsr(), flags);
}

TEST(ThumbStack, PopRestoresLowRegistersAndAdvancesStack) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t stack = MMU::IWRAM_BASE + 0x1000U;

    Bus bus;
    bus.reset();
    bus.write16(code, 0xBC85U); // POP {r0,r2,r7}
    bus.write32(stack, 0x11223344U);
    bus.write32(stack + 4U, 0x55667788U);
    bus.write32(stack + 8U, 0x99AABBCCU);

    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);

    const auto flags = cpu.debug_cpsr();
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(0), 0x11223344U);
    EXPECT_EQ(cpu.debug_reg(2), 0x55667788U);
    EXPECT_EQ(cpu.debug_reg(7), 0x99AABBCCU);
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack + 12U);
    EXPECT_EQ(cpu.debug_pc(), code + 2U);
    EXPECT_EQ(cpu.debug_cpsr(), flags);
}

TEST(ThumbStack, PopPcKeepsThumbStateRegardlessOfTargetBitZero) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t stack = MMU::IWRAM_BASE + 0x1000U;
    constexpr std::uint32_t target = MMU::IWRAM_BASE + 0x200U;
    constexpr std::array<std::uint32_t, 2> addresses{target, target | 1U};

    Bus bus;
    bus.reset();
    bus.write16(code, 0xBD00U); // POP {pc}

    ARM7TDMI cpu;
    cpu.attach(bus);
    for (const auto address : addresses) {
        cpu.reset();
        cpu.debug_set_program_counter(code);
        cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);
        bus.write32(stack, address);

        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), target);
        EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack + 4U);
        EXPECT_NE(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }
}

TEST(ThumbStack, GeneratedCartridgePushPopRoundTripAndReturn) {
    constexpr std::array<std::uint8_t, 10> program{
        0x01U, 0xB5U, // PUSH {r0,lr}
        0x00U, 0x20U, // MOVS r0, #0
        0x01U, 0xBDU, // POP {r0,pc}
        0x63U, 0x22U, // MOVS r2, #99 (skipped)
        0x07U, 0x21U  // MOVS r1, #7
    };
    constexpr std::uint32_t stack = MMU::IWRAM_BASE + 0x1000U;

    Bus bus;
    bus.reset();
    ASSERT_TRUE(bus.load_gamepak(program));
    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(MMU::WS0_BASE);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);
    cpu.debug_set_reg(ARM7TDMI::kRegLR, MMU::WS0_BASE + 9U);
    cpu.debug_set_reg(0, 0x12345678U);

    cpu.step();
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack - 8U);
    EXPECT_EQ(bus.read32(stack - 8U), 0x12345678U);
    EXPECT_EQ(bus.read32(stack - 4U), MMU::WS0_BASE + 9U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(0), 0U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(0), 0x12345678U);
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack);
    EXPECT_EQ(cpu.debug_pc(), MMU::WS0_BASE + 8U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(1), 7U);
    EXPECT_EQ(cpu.debug_reg(2), 0U);
    EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
}

TEST(ThumbStack, EmptyRegisterListsFailWithoutChangingStack) {
    constexpr std::uint32_t base = MMU::IWRAM_BASE;
    constexpr std::array<std::uint16_t, 2> encodings{0xB400U, 0xBC00U};
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);
    for (const auto encoding : encodings) {
        cpu.reset();
        cpu.debug_set_reg(ARM7TDMI::kRegSP, base + 0x1000U);
        cpu.debug_set_program_counter(base);
        bus.write16(base, encoding);
        cpu.step();
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::UndefinedThumb);
        EXPECT_EQ(cpu.execution_fault().opcode, encoding);
        EXPECT_EQ(cpu.debug_pc(), base);
        EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), base + 0x1000U);
    }
}

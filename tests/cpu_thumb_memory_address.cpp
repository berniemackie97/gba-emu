#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

TEST(ThumbAddressMemory, ImmediateHalfwordLoadStoreAtMaximumOffset) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t data = MMU::IWRAM_BASE + 0x1000U;
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    bus.write16(code, 0x87C8U); // STRH r0,[r1,#62]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(0, 0xAABBCCDDU);
    cpu.debug_set_reg(1, data);
    cpu.step();
    EXPECT_EQ(bus.read16(data + 62U), 0xCCDDU);

    bus.write16(code, 0x8FCAU); // LDRH r2,[r1,#62]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(1, data);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(2), 0xCCDDU);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
}

TEST(ThumbAddressMemory, ImmediateHalfwordUnalignedReadAndStore) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t data = MMU::IWRAM_BASE + 0x1000U;
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    bus.write16(code, 0x80C8U); // STRH r0,[r1,#6]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(0, 0xAABBU);
    cpu.debug_set_reg(1, data + 1U);
    cpu.step();
    EXPECT_EQ(bus.read16(data + 6U), 0xAABBU);

    bus.write16(code, 0x88CAU); // LDRH r2,[r1,#6]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(1, data + 1U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(2), 0xBB0000AAU);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
}

TEST(ThumbAddressMemory, StackRelativeTransfersUseScaledImmediateWithoutFlags) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t stack = MMU::IWRAM_BASE + 0x1200U;
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    bus.write16(code, 0x90FFU); // STR r0,[SP,#1020]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(0, 0xCAFE1234U);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);
    cpu.step();
    EXPECT_EQ(bus.read32(stack + 1020U), 0xCAFE1234U);
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack);

    bus.write16(code, 0x99FFU); // LDR r1,[SP,#1020]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(1), 0xCAFE1234U);
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
}

TEST(ThumbAddressMemory, RelativeAddressUsesAlignedArchitecturalPcOrFullSp) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE + 0x102U;
    constexpr std::uint32_t stack = MMU::IWRAM_BASE + 0x1235U;
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);
    bus.write16(code, 0xA201U); // ADD r2,PC,#4
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(2), MMU::IWRAM_BASE + 0x108U);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);

    bus.write16(code, 0xABFFU); // ADD r3,SP,#1020
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(3), stack + 1020U);
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
}

TEST(ThumbAddressMemory, StackAdjustSupportsBothSignsAndUnsignedWrap) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    bus.write16(code, 0xB07FU); // ADD SP,#508
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, 0xFFFFFFFCU);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), 0x1F8U);

    bus.write16(code, 0xB0FFU); // SUB SP,#508
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, 0x100U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), 0xFFFFFF04U);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
}

TEST(ThumbAddressMemory, GeneratedProgramUsesPcAndSpAddressingAcrossSeveralInstructions) {
    constexpr std::array<std::uint8_t, 10> program{
        0x01U, 0xA0U, // ADD r0,PC,#4 -> cartridge address +8
        0x00U, 0x90U, // STR r0,[SP,#0]
        0x00U, 0x99U, // LDR r1,[SP,#0]
        0x81U, 0xB0U, // SUB SP,#4
        0x01U, 0xB0U  // ADD SP,#4
    };
    constexpr std::uint32_t stack = MMU::IWRAM_BASE + 0x1700U;
    Bus bus;
    bus.reset();
    ASSERT_TRUE(bus.load_gamepak(program));
    ARM7TDMI cpu;
    cpu.attach(bus);
    cpu.reset();
    cpu.debug_set_program_counter(MMU::WS0_BASE);
    cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);
    for (int step = 0; step < 5; ++step) {
        cpu.step();
        ASSERT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None) << step;
    }
    EXPECT_EQ(cpu.debug_reg(0), MMU::WS0_BASE + 8U);
    EXPECT_EQ(cpu.debug_reg(1), MMU::WS0_BASE + 8U);
    EXPECT_EQ(bus.read32(stack), MMU::WS0_BASE + 8U);
    EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
}

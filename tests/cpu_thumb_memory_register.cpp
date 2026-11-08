#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

using gba::ARM7TDMI;
using gba::Bus;
using gba::MMU;

TEST(ThumbRegisterMemory, UnalignedWordLoadsRotateAndStoresDoNotRotate) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t data = MMU::IWRAM_BASE + 0x800U;
    constexpr std::array<std::uint32_t, 4> expected{
        0x11223344U, 0x44112233U, 0x33441122U, 0x22334411U
    };

    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);
    for (std::uint32_t offset = 0; offset < expected.size(); ++offset) {
        bus.write32(data, 0x11223344U);
        bus.write16(code, 0x588BU); // LDR r3, [r1, r2]
        cpu.reset();
        cpu.debug_set_program_counter(code);
        cpu.debug_set_reg(1, data);
        cpu.debug_set_reg(2, offset);
        const auto flags = cpu.debug_cpsr();
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(3), expected.at(offset)) << offset;
        EXPECT_EQ(cpu.debug_cpsr(), flags);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);

        bus.write16(code, 0x5088U); // STR r0, [r1, r2]
        cpu.reset();
        cpu.debug_set_program_counter(code);
        cpu.debug_set_reg(0, 0xA1B2C3D4U);
        cpu.debug_set_reg(1, data);
        cpu.debug_set_reg(2, offset);
        cpu.step();
        EXPECT_EQ(bus.read32(data), 0xA1B2C3D4U) << offset;
        EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
    }
}

TEST(ThumbRegisterMemory, ByteLoadsStoresUseExactByteAddressAndPreserveFlags) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t data = MMU::IWRAM_BASE + 0x900U;
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    bus.write16(code, 0x5488U); // STRB r0, [r1, r2]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(0, 0x123456E5U);
    cpu.debug_set_reg(1, data);
    cpu.debug_set_reg(2, 3U);
    cpu.step();
    EXPECT_EQ(bus.read32(data), 0xE5000000U);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);

    bus.write16(code, 0x5C8BU); // LDRB r3, [r1, r2]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(1, data);
    cpu.debug_set_reg(2, 3U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(3), 0xE5U);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
}

TEST(ThumbRegisterMemory, HalfwordAndSignedLoadsFollowArmv4TAlignment) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t data = MMU::IWRAM_BASE + 0xA00U;
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    bus.write16(data, 0x80FEU);
    bus.write16(code, 0x568BU); // LDSB r3, [r1, r2]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(1, data);
    cpu.debug_set_reg(2, 1U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(3), 0xFFFFFF80U);

    bus.write16(code, 0x5E8DU); // LDSH r5, [r1, r2]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(1, data);
    cpu.debug_set_reg(2, 0U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(5), 0xFFFF80FEU);

    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(1, data);
    cpu.debug_set_reg(2, 1U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(5), 0xFFFFFF80U);

    bus.write16(code, 0x5A8CU); // LDRH r4, [r1, r2]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(1, data);
    cpu.debug_set_reg(2, 0U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(4), 0x80FEU);

    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(1, data);
    cpu.debug_set_reg(2, 1U);
    cpu.step();
    EXPECT_EQ(cpu.debug_reg(4), 0xFE000080U);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
}

TEST(ThumbRegisterMemory, HalfwordStoresAlignDownWithoutByteReplications) {
    constexpr std::uint32_t code = MMU::IWRAM_BASE;
    constexpr std::uint32_t data = MMU::IWRAM_BASE + 0xA00U;
    Bus bus;
    bus.reset();
    ARM7TDMI cpu;
    cpu.attach(bus);

    bus.write16(data, 0x1234U);
    bus.write16(code, 0x5288U); // STRH r0, [r1, r2]
    cpu.reset();
    cpu.debug_set_program_counter(code);
    cpu.debug_set_reg(0, 0xAABB99E7U);
    cpu.debug_set_reg(1, data);
    cpu.debug_set_reg(2, 1U);
    cpu.step();

    EXPECT_EQ(bus.read16(data), 0x99E7U);
    EXPECT_EQ(bus.read8(data + 2U), 0U);
    EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
    EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
}

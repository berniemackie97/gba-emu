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

    constexpr std::uint32_t kCode = MMU::IWRAM_BASE;
    constexpr std::uint32_t kData = MMU::IWRAM_BASE + 0x1800U;

    TEST(ThumbMultipleTransfer, StoresAndLoadsSparseRegisterListsInAscendingOrder) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);

        bus.write16(kCode, 0xC315U); // STMIA r3!,{r0,r2,r4}
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(3, kData);
        cpu.debug_set_reg(0, 0x11U);
        cpu.debug_set_reg(2, 0x22U);
        cpu.debug_set_reg(4, 0x44U);
        cpu.step();
        EXPECT_EQ(bus.read32(kData), 0x11U);
        EXPECT_EQ(bus.read32(kData + 4U), 0x22U);
        EXPECT_EQ(bus.read32(kData + 8U), 0x44U);
        EXPECT_EQ(cpu.debug_reg(3), kData + 12U);
        EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);

        bus.write16(kCode, 0xCD85U); // LDMIA r5!,{r0,r2,r7}
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(5, kData);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(0), 0x11U);
        EXPECT_EQ(cpu.debug_reg(2), 0x22U);
        EXPECT_EQ(cpu.debug_reg(7), 0x44U);
        EXPECT_EQ(cpu.debug_reg(5), kData + 12U);
        EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ThumbMultipleTransfer, StoreIncludedBaseUsesOldValueOnlyWhenFirst) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);

        bus.write16(kCode, 0xC005U); // STMIA r0!,{r0,r2}
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(0, kData);
        cpu.debug_set_reg(2, 0xAABBCCDDU);
        cpu.step();
        EXPECT_EQ(bus.read32(kData), kData);
        EXPECT_EQ(bus.read32(kData + 4U), 0xAABBCCDDU);
        EXPECT_EQ(cpu.debug_reg(0), kData + 8U);

        bus.write16(kCode, 0xC205U); // STMIA r2!,{r0,r2}
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(0, 0x11223344U);
        cpu.debug_set_reg(2, kData);
        cpu.step();
        EXPECT_EQ(bus.read32(kData), 0x11223344U);
        EXPECT_EQ(bus.read32(kData + 4U), kData + 8U);
        EXPECT_EQ(cpu.debug_reg(2), kData + 8U);
    }

    TEST(ThumbMultipleTransfer, LoadIncludedBaseDoesNotOverwriteLoadedValueWithWriteback) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        bus.write16(kCode, 0xCA05U); // LDMIA r2!,{r0,r2}
        bus.write32(kData, 0x10101010U);
        bus.write32(kData + 4U, 0x20202020U);
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(2, kData);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(0), 0x10101010U);
        EXPECT_EQ(cpu.debug_reg(2), 0x20202020U);
        EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
    }

    TEST(ThumbMultipleTransfer, MisalignedBaseAlignsTransfersButPreservesWritebackBits) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        bus.write16(kCode, 0xC301U); // STMIA r3!,{r0}
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(3, kData + 1U);
        cpu.debug_set_reg(0, 0xDEADBEEFU);
        cpu.step();
        EXPECT_EQ(bus.read32(kData), 0xDEADBEEFU);
        EXPECT_EQ(cpu.debug_reg(3), kData + 5U);
    }

    TEST(ThumbMultipleTransfer, EmptyRegisterListsUseArmv4TTransferAndSixtyFourByteWriteback) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);

        bus.write16(kCode, 0xC100U); // STMIA r1!,{}
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(1, kData);
        cpu.step();
        EXPECT_EQ(bus.read32(kData), kCode + 4U);
        EXPECT_EQ(cpu.debug_reg(1), kData + 0x40U);

        constexpr std::uint32_t target = MMU::IWRAM_BASE + 0x400U;
        bus.write16(kCode, 0xC900U); // LDMIA r1!,{}
        bus.write32(kData, target | 1U);
        cpu.reset();
        cpu.debug_set_program_counter(kCode);
        cpu.debug_set_reg(1, kData);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), target);
        EXPECT_EQ(cpu.debug_reg(1), kData + 0x40U);
        EXPECT_EQ(cpu.debug_cpsr(), ARM7TDMI::kFlagT);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ThumbMultipleTransfer, GeneratedCartridgeRoundTripsThroughWordMemory) {
        constexpr std::array<std::uint8_t, 6> program{
            0x05U, 0xC3U, // STMIA r3!,{r0,r2}
            0x08U, 0x3BU, // SUBS r3,#8
            0x05U, 0xCBU  // LDMIA r3!,{r0,r2}
        };
        Bus bus;
        bus.reset();
        ASSERT_TRUE(bus.load_gamepak(program));
        ARM7TDMI cpu;
        cpu.attach(bus);
        cpu.reset();
        cpu.debug_set_program_counter(MMU::WS0_BASE);
        cpu.debug_set_reg(3, kData);
        cpu.debug_set_reg(0, 0x12345678U);
        cpu.debug_set_reg(2, 0x87654321U);
        cpu.step();
        cpu.step();
        cpu.debug_set_reg(0, 0U);
        cpu.debug_set_reg(2, 0U);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(0), 0x12345678U);
        EXPECT_EQ(cpu.debug_reg(2), 0x87654321U);
        EXPECT_EQ(cpu.debug_reg(3), kData + 8U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

} // namespace

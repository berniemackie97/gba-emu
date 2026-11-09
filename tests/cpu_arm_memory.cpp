#include <cstdint>
#include <gtest/gtest.h>

#include "support/arm_program.h"

namespace {

    using gba::ARM7TDMI;
    using gba::Bus;
    using namespace gba::test;

    TEST(ArmMemory, PreIndexedWordStoreAndPostIndexedLoadWriteBackCorrectly) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        cpu.debug_set_reg(1, 0x11223344U);
        cpu.debug_set_reg(4, kArmData);
        bus.write32(kArmEntry, 0xE5A41004U); // STR r1,[r4,#4]!
        bus.write32(kArmEntry + 4U, 0xE4942004U); // LDR r2,[r4],#4
        cpu.step();
        EXPECT_EQ(bus.read32(kArmData + 4U), 0x11223344U);
        EXPECT_EQ(cpu.debug_reg(4), kArmData + 4U);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(2), 0x11223344U);
        EXPECT_EQ(cpu.debug_reg(4), kArmData + 8U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ArmMemory, RegisterScaledAddressUsesBarrelShifterWithoutChangingFlags) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu, true);
        cpu.debug_set_reg(1, 0xDEADBEEFU);
        cpu.debug_set_reg(3, 2U);
        cpu.debug_set_reg(4, kArmData);
        bus.write32(kArmEntry, 0xE7841103U); // STR r1,[r4,r3,LSL #2]
        bus.write32(kArmEntry + 4U, 0xE7942103U); // LDR r2,[r4,r3,LSL #2]
        const auto flags = cpu.debug_cpsr();
        cpu.step();
        cpu.step();
        EXPECT_EQ(bus.read32(kArmData + 8U), 0xDEADBEEFU);
        EXPECT_EQ(cpu.debug_reg(2), 0xDEADBEEFU);
        EXPECT_EQ(cpu.debug_cpsr(), flags);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ArmMemory, UnalignedLoadRotatesWordAndByteTransfersAccessExactByte) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        cpu.debug_set_reg(4, kArmData);
        cpu.debug_set_reg(1, 0xABCDU);
        bus.write32(kArmData, 0x11223344U);
        bus.write32(kArmEntry, 0xE5942001U); // LDR r2,[r4,#1]
        bus.write32(kArmEntry + 4U, 0xE5C41001U); // STRB r1,[r4,#1]
        bus.write32(kArmEntry + 8U, 0xE5D43001U); // LDRB r3,[r4,#1]
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(2), 0x44112233U);
        cpu.step();
        cpu.step();
        EXPECT_EQ(bus.read8(kArmData + 1U), 0xCDU);
        EXPECT_EQ(cpu.debug_reg(3), 0xCDU);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ArmMemory, MultipleRegisterTransfersPreserveOrderAndWriteback) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        cpu.debug_set_reg(1, 0x11U);
        cpu.debug_set_reg(2, 0x22U);
        cpu.debug_set_reg(3, 0x33U);
        cpu.debug_set_reg(4, kArmData);
        bus.write32(kArmEntry, 0xE8A4000EU); // STMIA r4!,{r1,r2,r3}
        bus.write32(kArmEntry + 4U, 0xE8B4000EU); // LDMIA r4!,{r1,r2,r3}
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(4), kArmData + 12U);
        EXPECT_EQ(bus.read32(kArmData), 0x11U);
        EXPECT_EQ(bus.read32(kArmData + 4U), 0x22U);
        EXPECT_EQ(bus.read32(kArmData + 8U), 0x33U);
        cpu.debug_set_reg(1, 0U);
        cpu.debug_set_reg(2, 0U);
        cpu.debug_set_reg(3, 0U);
        cpu.debug_set_reg(4, kArmData);
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(1), 0x11U);
        EXPECT_EQ(cpu.debug_reg(2), 0x22U);
        EXPECT_EQ(cpu.debug_reg(3), 0x33U);
        EXPECT_EQ(cpu.debug_reg(4), kArmData + 12U);
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::None);
    }

    TEST(ArmMemory, DecrementBeforeStackStoreAndIncrementAfterPcRestore) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        constexpr std::uint32_t stack = kArmData + 0x200U;
        constexpr std::uint32_t target = kArmEntry + 0x200U;
        cpu.debug_set_reg(1, 0x11U);
        cpu.debug_set_reg(2, 0x22U);
        cpu.debug_set_reg(3, 0x33U);
        cpu.debug_set_reg(ARM7TDMI::kRegSP, stack);
        cpu.debug_set_reg(ARM7TDMI::kRegLR, target);
        bus.write32(kArmEntry, 0xE92D400EU); // STMDB sp!,{r1,r2,r3,lr}
        bus.write32(kArmEntry + 4U, 0xE8BD800EU); // LDMIA sp!,{r1,r2,r3,pc}
        cpu.step();
        EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack - 16U);
        EXPECT_EQ(bus.read32(stack - 16U), 0x11U);
        EXPECT_EQ(bus.read32(stack - 12U), 0x22U);
        EXPECT_EQ(bus.read32(stack - 8U), 0x33U);
        EXPECT_EQ(bus.read32(stack - 4U), target);
        cpu.debug_set_reg(1, 0U);
        cpu.debug_set_reg(2, 0U);
        cpu.debug_set_reg(3, 0U);
        cpu.step();
        EXPECT_EQ(cpu.debug_pc(), target);
        EXPECT_EQ(cpu.debug_reg(ARM7TDMI::kRegSP), stack);
        EXPECT_EQ(cpu.debug_reg(1), 0x11U);
        EXPECT_EQ(cpu.debug_reg(2), 0x22U);
        EXPECT_EQ(cpu.debug_reg(3), 0x33U);
        EXPECT_EQ(cpu.debug_cpsr() & ARM7TDMI::kFlagT, 0U);
    }

    TEST(ArmMemory, PrivilegedPostIndexedTransferDoesNotSilentlyUseNormalBusAccess) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        cpu.debug_set_reg(4, kArmData);
        cpu.debug_set_reg(1, 0xAABBCCDDU);
        bus.write32(kArmEntry, 0xE4A41004U); // STRT r1,[r4],#4
        bus.write32(kArmData, 0x11111111U);
        cpu.step();
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::UnsupportedArmOpcode);
        EXPECT_EQ(cpu.debug_reg(4), kArmData);
        EXPECT_EQ(bus.read32(kArmData), 0x11111111U);
        EXPECT_EQ(cpu.debug_pc(), kArmEntry);
    }

    TEST(ArmMemory, UnpredictableBaseAliasIsRejectedBeforeMemoryMutation) {
        Bus bus;
        bus.reset();
        ARM7TDMI cpu;
        cpu.attach(bus);
        enter_arm(bus, cpu);
        cpu.debug_set_reg(4, kArmData);
        bus.write32(kArmEntry, 0xE8B40010U); // LDMIA r4!,{r4}
        bus.write32(kArmData, 0x11223344U);
        cpu.step();
        EXPECT_EQ(cpu.execution_fault().kind, ARM7TDMI::FaultKind::UnsupportedArmOpcode);
        EXPECT_EQ(cpu.debug_reg(4), kArmData);
        EXPECT_EQ(bus.read32(kArmData), 0x11223344U);
        EXPECT_EQ(cpu.debug_pc(), kArmEntry);
    }

} // namespace

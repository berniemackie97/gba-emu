#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include "core/io/io.h"
#include "core/mmu/mmu.h"

using gba::IORegs;
using gba::MMU;

TEST(IOOwnership, RegionsHaveOneExplicitOwner) {
    using Device = IORegs::Device;
    struct Case {
        std::uint32_t address;
        Device owner;
    };
    constexpr std::array cases{
        Case{0x000U, Device::Display},
        Case{0x054U, Device::Display},
        Case{0x055U, Device::Display},
        Case{0x056U, Device::Unmapped},
        Case{0x060U, Device::Audio},
        Case{0x0A7U, Device::Audio},
        Case{0x0B0U, Device::Dma},
        Case{0x0DFU, Device::Dma},
        Case{0x100U, Device::Timer},
        Case{0x10FU, Device::Timer},
        Case{0x120U, Device::Serial},
        Case{0x12BU, Device::Serial},
        Case{0x130U, Device::Keypad},
        Case{0x133U, Device::Keypad},
        Case{0x134U, Device::Serial},
        Case{0x200U, Device::Interrupt},
        Case{0x203U, Device::Interrupt},
        Case{0x204U, Device::WaitControl},
        Case{0x205U, Device::WaitControl},
        Case{0x208U, Device::Interrupt},
        Case{0x209U, Device::Interrupt},
        Case{0x300U, Device::System},
        Case{0x301U, Device::System},
        Case{0x302U, Device::Unmapped},
        Case{0x3FFU, Device::Unmapped},
        Case{0x400U, Device::Unmapped}
    };
    for (const auto &vector : cases) {
        EXPECT_EQ(IORegs::owner(vector.address), vector.owner) << vector.address;
    }
}

TEST(IOOwnership, UnsupportedAndReservedOffsetsCannotActAsRam) {
    IORegs io;
    io.reset();
    for (const std::uint32_t offset : {0x002U, 0x008U, 0x056U, 0x060U, 0x0A0U,
                                       0x0B0U, 0x0DEU, 0x100U, 0x10EU, 0x120U,
                                       0x134U, 0x206U, 0x300U, 0x3FEU, 0x3FFU, 0x400U}) {
        const auto before = io.read8(offset);
        EXPECT_EQ(before, 0xFFU) << offset;
        io.write8(offset, 0x12U);
        EXPECT_EQ(io.read8(offset), before) << offset;
    }
    io.write32(0x3FEU, 0x12345678U);
    EXPECT_EQ(io.read32(0x3FEU), 0xFFFFFFFFU);
}

TEST(IOOwnership, KeyInputIsActiveLowAndKeyControlMasksReservedBits) {
    IORegs io;
    io.reset();
    EXPECT_EQ(io.read16(IORegs::kOffKEYINPUT), 0x03FFU);
    EXPECT_EQ(io.read16(IORegs::kOffKEYCNT), 0U);

    io.write32(IORegs::kOffKEYINPUT, 0xFFFFFFFFU);
    EXPECT_EQ(io.read16(IORegs::kOffKEYINPUT), 0x03FFU);
    EXPECT_EQ(io.read16(IORegs::kOffKEYCNT), 0xC3FFU);
    EXPECT_EQ(io.read32(IORegs::kOffKEYINPUT), 0xC3FF03FFU);

    io.debug_set_pressed_keys_for_tests(0x0009U);
    EXPECT_EQ(io.read16(IORegs::kOffKEYINPUT), 0x03F6U);
    io.write16(IORegs::kOffKEYINPUT, 0U);
    EXPECT_EQ(io.read16(IORegs::kOffKEYINPUT), 0x03F6U);
    io.debug_set_pressed_keys_for_tests(0xFFFFU);
    EXPECT_EQ(io.read16(IORegs::kOffKEYINPUT), 0U);
    io.reset();
    EXPECT_EQ(io.read16(IORegs::kOffKEYINPUT), 0x03FFU);
    EXPECT_EQ(io.read16(IORegs::kOffKEYCNT), 0U);
}

TEST(IOOwnership, InterruptRequestMaskAndWriteOneClearAreIndependentOfEnable) {
    IORegs io;
    io.reset();
    io.debug_request_interrupt_for_tests(0xFFFFU);
    EXPECT_EQ(io.read16(IORegs::kOffIF), 0x3FFFU);
    EXPECT_FALSE(io.interrupt_pending());

    io.write16(IORegs::kOffIE, 0xFFFFU);
    EXPECT_EQ(io.read16(IORegs::kOffIE), 0x3FFFU);
    EXPECT_FALSE(io.interrupt_pending());
    io.write16(IORegs::kOffIME, 0xFFFFU);
    EXPECT_EQ(io.read16(IORegs::kOffIME), 1U);
    EXPECT_TRUE(io.interrupt_pending());

    io.write8(IORegs::kOffIF, 0x01U);
    EXPECT_EQ(io.read16(IORegs::kOffIF), 0x3FFEU);
    io.write8(IORegs::kOffIF + 1U, 0x3FU);
    EXPECT_EQ(io.read16(IORegs::kOffIF), 0x00FEU);
    io.write16(IORegs::kOffIF, 0xFFFFU);
    EXPECT_EQ(io.read16(IORegs::kOffIF), 0U);
    EXPECT_FALSE(io.interrupt_pending());
    io.debug_request_interrupt_for_tests(0x2000U);
    EXPECT_TRUE(io.interrupt_pending());

    io.write16(IORegs::kOffIE, 0x0001U);
    EXPECT_FALSE(io.interrupt_pending());
    EXPECT_EQ(io.read16(IORegs::kOffIF), 0x2000U);
    io.write16(IORegs::kOffIME, 0U);
    io.reset();
    EXPECT_EQ(io.read16(IORegs::kOffIE), 0U);
    EXPECT_EQ(io.read16(IORegs::kOffIF), 0U);
    EXPECT_EQ(io.read16(IORegs::kOffIME), 0U);
}

TEST(IOOwnership, WaitControlMasksReservedBitsWithoutImplementingTiming) {
    IORegs io;
    io.reset();
    EXPECT_EQ(io.read16(IORegs::kOffWAITCNT), 0U);
    io.write16(IORegs::kOffWAITCNT, 0xFFFFU);
    EXPECT_EQ(io.read16(IORegs::kOffWAITCNT), 0x5FFFU);
    io.write8(IORegs::kOffWAITCNT + 1U, 0xA0U);
    EXPECT_EQ(io.read16(IORegs::kOffWAITCNT), 0x00FFU);
    io.write8(IORegs::kOffWAITCNT + 1U, 0x40U);
    EXPECT_EQ(io.read16(IORegs::kOffWAITCNT), 0x40FFU);
}

TEST(IOOwnership, UnalignedWordTransfersHonorAdjacentHalfwordSideEffects) {
    IORegs io;
    io.reset();
    io.debug_request_interrupt_for_tests(0x0303U);
    io.write32(IORegs::kOffIE + 1U, 0x44332211U);
    EXPECT_EQ(io.read16(IORegs::kOffIE), 0x1100U);
    EXPECT_EQ(io.read16(IORegs::kOffIF), 0x0001U);
    EXPECT_EQ(io.read16(IORegs::kOffWAITCNT), 0x0044U);
    EXPECT_EQ(io.read32(IORegs::kOffIE + 1U), 0x44000111U);

    io.write32(IORegs::kOffIF, 0xFFFFFFFFU);
    EXPECT_EQ(io.read16(IORegs::kOffIF), 0U);
    EXPECT_EQ(io.read16(IORegs::kOffWAITCNT), 0x5FFFU);
}

TEST(IOOwnership, MemoryBusRoutesWordTransfersAcrossRegisterOwners) {
    MMU mmu;
    mmu.reset();
    constexpr auto io = MMU::IO_BASE;
    mmu.write32(io + IORegs::kOffKEYINPUT, 0xFFFFFFFFU);
    EXPECT_EQ(mmu.read16(io + IORegs::kOffKEYINPUT), 0x03FFU);
    EXPECT_EQ(mmu.read16(io + IORegs::kOffKEYCNT), 0xC3FFU);

    mmu.write32(io + IORegs::kOffIE, 0x00000001U);
    EXPECT_EQ(mmu.read16(io + IORegs::kOffIE), 1U);
    EXPECT_EQ(mmu.read16(io + IORegs::kOffIF), 0U);

    mmu.write32(io + IORegs::kOffWAITCNT, 0xFFFFFFFFU);
    EXPECT_EQ(mmu.read16(io + IORegs::kOffWAITCNT), 0x5FFFU);
    EXPECT_EQ(mmu.read16(io + 0x206U), 0xFFFFU);
    mmu.reset();
    EXPECT_EQ(mmu.read16(io + IORegs::kOffWAITCNT), 0U);
    EXPECT_EQ(mmu.read16(io + IORegs::kOffKEYINPUT), 0x03FFU);
}

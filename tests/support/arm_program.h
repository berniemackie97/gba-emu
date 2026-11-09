#pragma once

#include "core/bus/bus.h"
#include "core/cpu/arm7tdmi.h"
#include "core/mmu/mmu.h"

namespace gba::test {

    inline constexpr std::uint32_t kArmEntry = MMU::IWRAM_BASE + 0x1000U;
    inline constexpr std::uint32_t kThumbEntry = MMU::IWRAM_BASE + 0x100U;
    inline constexpr std::uint32_t kArmData = MMU::IWRAM_BASE + 0x3000U;

    inline void enter_arm(Bus &bus, ARM7TDMI &cpu, bool carry = false) {
        bus.write16(kThumbEntry, carry ? 0x2F00U : 0x2F01U);
        bus.write16(kThumbEntry + 2U, 0x4700U);
        cpu.reset();
        cpu.debug_set_program_counter(kThumbEntry);
        cpu.debug_set_reg(0, kArmEntry);
        cpu.debug_set_reg(7, 0U);
        cpu.step();
        cpu.step();
    }

} // namespace gba::test

#include "core/cpu/arm7tdmi.h"
#include "core/bus/bus.h"

#include <bit>

namespace gba {

    void ARM7TDMI::exec_multiple_transfer(u16 instruction) noexcept {
        const u32 baseRegister = (instruction >> 8U) & 7U;
        const u32 registerList = instruction & 0xFFU;
        const bool load = (instruction & 0x0800U) != 0U;
        const u32 base = regs_[baseRegister];
        u32 address = base & kWordMask;

        if (registerList == 0U) {
            if (load) {
                regs_[kRegPC] = bus_->read32(address) & ~u32{1};
            } else {
                bus_->write32(address, thumb_pc());
            }
            regs_[baseRegister] = base + 0x40U;
            return;
        }

        const u32 finalBase = base + 4U * static_cast<u32>(std::popcount(registerList));
        const bool baseIncluded = (registerList & (1U << baseRegister)) != 0U;
        const u32 firstRegister = static_cast<u32>(std::countr_zero(registerList));

        for (u32 reg = 0U; reg < 8U; ++reg) {
            if ((registerList & (1U << reg)) == 0U) {
                continue;
            }
            if (load) {
                regs_[reg] = bus_->read32(address);
            } else {
                const u32 value = reg == baseRegister && firstRegister != baseRegister
                    ? finalBase : regs_[reg];
                bus_->write32(address, value);
            }
            address += 4U;
        }

        if (!load || !baseIncluded) {
            regs_[baseRegister] = finalBase;
        }
    }

    void ARM7TDMI::exec_long_branch(u16 instruction) noexcept {
        const u32 offset = instruction & 0x7FFU;
        if ((instruction & 0x0800U) == 0U) {
            regs_[kRegLR] = thumb_pc() + sign_extend<23U>(offset << 12U);
            return;
        }

        const u32 target = regs_[kRegLR] + (offset << 1U);
        regs_[kRegLR] = regs_[kRegPC] | 1U;
        regs_[kRegPC] = target & ~u32{1};
    }

} // namespace gba

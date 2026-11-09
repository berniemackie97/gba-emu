#include "core/cpu/arm7tdmi.h"
#include "core/bus/bus.h"

#include <bit>

namespace gba {

    void ARM7TDMI::exec_arm_single_transfer(u32 instruction, u32 instructionAddress) noexcept {
        const bool registerOffset = (instruction & 0x02000000U) != 0U;
        const bool preIndex = (instruction & 0x01000000U) != 0U;
        const bool increment = (instruction & 0x00800000U) != 0U;
        const bool byte = (instruction & 0x00400000U) != 0U;
        const bool writeback = (instruction & 0x00200000U) != 0U || !preIndex;
        const bool load = (instruction & 0x00100000U) != 0U;
        const u32 rn = (instruction >> 16U) & 15U;
        const u32 rd = (instruction >> 12U) & 15U;

        if ((registerOffset && (instruction & 0x10U) != 0U) ||
            (!preIndex && (instruction & 0x00200000U) != 0U) ||
            (rn == kRegPC && writeback) ||
            (load && writeback && rn == rd) ||
            (byte && rd == kRegPC)) {
            record_fault(FaultKind::UnsupportedArmOpcode, instructionAddress, instruction);
            return;
        }

        const u32 base = arm_register(rn);
        const u32 offset = registerOffset
            ? arm_shift(arm_register(instruction & 15U),
                        (instruction >> 5U) & 3U, (instruction >> 7U) & 31U, false).value
            : (instruction & 0xFFFU);
        const u32 adjusted = increment ? base + offset : base - offset;
        const u32 address = preIndex ? adjusted : base;

        if (load) {
            const u32 value = byte ? bus_->read8(address) : read_u32_unaligned(address);
            regs_[rd] = rd == kRegPC ? value & kWordMask : value;
        } else {
            const u32 value = rd == kRegPC ? instructionAddress + 12U : regs_[rd];
            if (byte) {
                bus_->write8(address, static_cast<u8>(value));
            } else {
                write_u32_unaligned(address, value);
            }
        }
        if (writeback) {
            regs_[rn] = adjusted;
        }
    }

    void ARM7TDMI::exec_arm_block_transfer(u32 instruction, u32 instructionAddress) noexcept {
        const bool preIndex = (instruction & 0x01000000U) != 0U;
        const bool increment = (instruction & 0x00800000U) != 0U;
        const bool userBank = (instruction & 0x00400000U) != 0U;
        const bool writeback = (instruction & 0x00200000U) != 0U;
        const bool load = (instruction & 0x00100000U) != 0U;
        const u32 rn = (instruction >> 16U) & 15U;
        const u32 list = instruction & 0xFFFFU;
        const u32 count = list == 0U ? 16U : static_cast<u32>(std::popcount(list));
        const bool baseIncluded = (list & (1U << rn)) != 0U;
        if (userBank || rn == kRegPC || (load && writeback && baseIncluded)) {
            record_fault(FaultKind::UnsupportedArmOpcode, instructionAddress, instruction);
            return;
        }

        const u32 base = regs_[rn];
        const u32 delta = 4U * count;
        u32 address = increment
            ? base + (preIndex ? 4U : 0U)
            : base - delta + (preIndex ? 0U : 4U);
        const u32 finalBase = increment ? base + delta : base - delta;
        const u32 first = list == 0U ? kRegPC : static_cast<u32>(std::countr_zero(list));

        for (u32 reg = 0U; reg < 16U; ++reg) {
            if ((list == 0U && reg != kRegPC) ||
                (list != 0U && (list & (1U << reg)) == 0U)) {
                continue;
            }
            if (load) {
                const u32 value = bus_->read32(address & kWordMask);
                regs_[reg] = reg == kRegPC ? value & kWordMask : value;
            } else {
                u32 value = reg == kRegPC ? instructionAddress + 12U : regs_[reg];
                if (writeback && reg == rn && first != rn) {
                    value = finalBase;
                }
                bus_->write32(address & kWordMask, value);
            }
            address += 4U;
        }

        if (writeback) {
            regs_[rn] = finalBase;
        }
    }

} // namespace gba

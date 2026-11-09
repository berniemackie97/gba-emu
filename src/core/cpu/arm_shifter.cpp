#include "core/cpu/arm7tdmi.h"

#include <bit>

namespace gba {

    auto ARM7TDMI::arm_shift(u32 value, ShiftKind kind, u32 amount, bool byRegister) const noexcept -> ArmOperand {
        const bool carryIn = (cpsr_ & kFlagC) != 0U;
        if (byRegister && amount == 0U) {
            return {value, carryIn};
        }

        switch (kind) {
        case ShiftKind::Lsl:
            if (amount == 0U) return {value, carryIn};
            if (amount < 32U) return {value << amount, ((value >> (32U - amount)) & 1U) != 0U};
            if (amount == 32U) return {0U, (value & 1U) != 0U};
            return {0U, false};
        case ShiftKind::Lsr:
            if (!byRegister && amount == 0U) amount = 32U;
            if (amount == 32U) return {0U, (value & kSignBit) != 0U};
            if (amount > 32U) return {0U, false};
            return {value >> amount, ((value >> (amount - 1U)) & 1U) != 0U};
        case ShiftKind::Asr: {
            if (!byRegister && amount == 0U) amount = 32U;
            const bool negative = (value & kSignBit) != 0U;
            if (amount >= 32U) return {negative ? ~u32{0} : 0U, negative};
            return {(value >> amount) | (negative ? (~u32{0} << (32U - amount)) : 0U),
                    ((value >> (amount - 1U)) & 1U) != 0U};
        }
        case ShiftKind::Ror:
            if (!byRegister && amount == 0U) {
                return {(value >> 1U) | (carryIn ? kSignBit : 0U), (value & 1U) != 0U};
            }
            if ((amount & 31U) == 0U) {
                return {value, (value & kSignBit) != 0U};
            }
            value = std::rotr(value, static_cast<int>(amount & 31U));
            return {value, (value & kSignBit) != 0U};
        default: return {value, carryIn};
        }
    }

    auto ARM7TDMI::arm_operand2(u32 instruction) const noexcept -> ArmOperand {
        const bool carryIn = (cpsr_ & kFlagC) != 0U;
        if ((instruction & 0x02000000U) != 0U) {
            const u32 rotation = ((instruction >> 8U) & 15U) * 2U;
            const u32 immediate = instruction & 0xFFU;
            if (rotation == 0U) {
                return {immediate, carryIn};
            }
            const u32 value = std::rotr(immediate, static_cast<int>(rotation));
            return {value, (value & kSignBit) != 0U};
        }

        const bool byRegister = (instruction & 0x10U) != 0U;
        const u32 rm = instruction & 15U;
        const u32 value = arm_register(rm, byRegister);
        const u32 amount = byRegister
            ? arm_register((instruction >> 8U) & 15U) & 0xFFU
            : (instruction >> 7U) & 31U;
        return arm_shift(value, static_cast<ShiftKind>((instruction >> 5U) & 3U), amount, byRegister);
    }

} // namespace gba
